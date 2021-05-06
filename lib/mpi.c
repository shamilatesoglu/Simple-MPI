#include "mpi.h"
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <fcntl.h>    /* For O_* constants */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define DEBUG 0

static int comm_size;
static int comm_rank;

static inbox_t inboxes[MAX_PROCESS_COUNT];

static process_status_t processes[MAX_PROCESS_COUNT];

/* Private methods */
void
MPI_debug_print(char *tag, char *fmt, ...);

void
MPI_create_shared_memory(char *name, size_t size, byte **out_shm_pointer, int *out_fd);

void
MPI_create_or_open_semaphore(char *name, sem_t **out_sem, int initial);

/* Implementations */

int
MPI_Init(int *argc, char ***argv)
{
    char *arg_rank = (*argv)[1];
    char *arg_n = (*argv)[2];

    comm_rank = (int) strtol(arg_rank, NULL, 10);
    comm_size = (int) strtol(arg_n, NULL, 10);

    for (int i = 0; i < comm_size; i++)
    {
        char inbox_name[100];
        sprintf(inbox_name, SHM_INBOX_NAME_FORMAT, i);
        int inbox_shm_fd;
        byte *inbox_shm_pointer;
        size_t inbox_memory_size = ((2 * sizeof(int) * comm_size) + (MAX_MESSAGE_COUNT_PER_SENDER * comm_size)) * sizeof(envelope_t);
        MPI_create_shared_memory(inbox_name, inbox_memory_size, &inbox_shm_pointer, &inbox_shm_fd);

        char sem_name[100];
        sprintf(sem_name, SEM_INITIALIZED_NAME_FORMAT, i);
        sem_t *init;
        MPI_create_or_open_semaphore(sem_name, &init, 0);

        sprintf(sem_name, SEM_GO_NAME_FORMAT, i);
        sem_t *go;
        MPI_create_or_open_semaphore(sem_name, &go, 0);

        sprintf(sem_name, SEM_TERMINATE_NAME_FORMAT, i);
        sem_t *terminate;
        MPI_create_or_open_semaphore(sem_name, &terminate, 0);

        process_status_t process_status;
        process_status.init = init;
        process_status.go = go;
        process_status.terminate = terminate;
        processes[i] = process_status;

        sprintf(sem_name, SEM_MUTEX_NAME_FORMAT, i);
        sem_t *mutex;
        MPI_create_or_open_semaphore(sem_name, &mutex, 1);

        sprintf(sem_name, SEM_FULL_NAME_FORMAT, i);
        sem_t *full;
        MPI_create_or_open_semaphore(sem_name, &full, 0);

        sprintf(sem_name, SEM_EMPTY_NAME_FORMAT, i);
        sem_t *empty;
        MPI_create_or_open_semaphore(sem_name, &empty, MAX_MESSAGE_COUNT_PER_SENDER);

        inbox_t inbox;
        inbox.lock = mutex;
        inbox.sem_full = full;
        inbox.sem_empty = empty;
        inbox.shm_p = inbox_shm_pointer;
        inboxes[i] = inbox;
    }

    if (comm_rank == 0)
    {
        for (int i = 1; i < comm_size; i++)
        {
            MPI_debug_print("INFO", "Root process is waiting for %d to initialize\n", i);
            sem_wait(processes[i].init);
        }
        for (int i = 0; i < comm_size; i++)
        {
            MPI_debug_print("INFO", "Root process is sending go signal to %d\n", i);
            sem_post(processes[i].go);
        }
        sem_wait(processes[0].go);
    }
    else
    {
        MPI_debug_print("INFO", "%d is notifying root that it has been successfully initialized\n", comm_rank);
        sem_post(processes[comm_rank].init);
        MPI_debug_print("INFO", "%d is waiting for go signal from root\n", comm_rank);
        sem_wait(processes[comm_rank].go);
    }

    return 0;
}

int
MPI_Finalize()
{
    MPI_debug_print("INFO", "Finalize %d\n", comm_rank);

    if (comm_rank == 0)
    {
        for (int i = 1; i < comm_size; i++)
        {
            MPI_debug_print("INFO", "Root process is waiting for %d to terminate\n", i);
            sem_wait(processes[i].terminate);
            MPI_debug_print("INFO", "Root process has received terminate signal from %d\n", i);
        }

        for (int i = 0; i < comm_size; i++)
        {
            sem_close(processes[i].init);
            sem_close(processes[i].go);
            sem_close(processes[i].terminate);
            sem_destroy(processes[i].init);
            sem_destroy(processes[i].go);
            sem_destroy(processes[i].terminate);

            inbox_t inbox = inboxes[i];

            char shm_name[100];
            sprintf(shm_name, SHM_INBOX_NAME_FORMAT, i);
            shm_unlink(shm_name);

            sem_close(inbox.sem_full);
            sem_close(inbox.sem_empty);
            sem_destroy(inbox.sem_full);
            sem_destroy(inbox.sem_empty);

            char sem_name[100];
            sprintf(sem_name, SEM_INITIALIZED_NAME_FORMAT, i);
            sem_unlink(sem_name);
            sprintf(sem_name, SEM_GO_NAME_FORMAT, i);
            sem_unlink(sem_name);
            sprintf(sem_name, SEM_TERMINATE_NAME_FORMAT, i);
            sem_unlink(sem_name);
            sprintf(sem_name, SEM_FULL_NAME_FORMAT, i);
            sem_unlink(sem_name);
            sprintf(sem_name, SEM_EMPTY_NAME_FORMAT, i);
            sem_unlink(sem_name);
            sprintf(sem_name, SEM_MUTEX_NAME_FORMAT, i);
            sem_unlink(sem_name);
        }
    }
    else
    {
        MPI_debug_print("INFO", "Process %d is terminating\n", comm_rank);
        sem_post(processes[comm_rank].terminate);
    }

    return 0;
}

int
MPI_Comm_size(int *size)
{
    *size = comm_size;
    return 0;
}

int
MPI_Comm_rank(int *rank)
{
    *rank = comm_rank;
    return 0;
}

int
MPI_Recv(void *out, int count, int size, int source, int tag)
{
    inbox_t inbox = inboxes[comm_rank];

    MPI_debug_print("INFO", "%d is waiting for its inbox to be filled\n", comm_rank);

    sem_wait(inbox.sem_full);
    sem_wait(inbox.lock);

    ///// CRITICAL SECTION /////

    size_t offset = (2 * sizeof(int) * source) + (MAX_MESSAGE_COUNT_PER_SENDER * sizeof(envelope_t) * source);
    int *use = ((int *) (inbox.shm_p + offset + sizeof(int)));

    byte *memory_start = inbox.shm_p + offset + 2 * sizeof(int);
    envelope_t envelope;

    memcpy(&envelope, memory_start + (*use) * sizeof(envelope_t), sizeof(envelope_t));
    *use = (*use + 1) % (MAX_MESSAGE_COUNT_PER_SENDER);

    memcpy(out, envelope.message.data, count * size);

    ///// CRITICAL SECTION /////

    sem_post(inbox.lock);
    sem_post(inbox.sem_empty);

    MPI_debug_print("INFO", "%d has notified everyone that its inbox has been emptied\n", comm_rank);

    return 0;
}

int
MPI_Send(const void *data, int count, int size, int dest, int tag)
{
    inbox_t inbox = inboxes[dest];

    MPI_debug_print("INFO", "%d is waiting for inbox%d to be emptied\n", comm_rank, dest);

    sem_wait(inbox.sem_empty);
    sem_wait(inbox.lock);

    ///// CRITICAL SECTION /////

    message_t message;
    message.size = count * size;
    memcpy(message.data, data, message.size);

    envelope_t envelope;
    envelope.message = message;
    envelope.sender = comm_rank;

    size_t offset = (2 * sizeof(int) * comm_rank) + (MAX_MESSAGE_COUNT_PER_SENDER * sizeof(envelope_t) * comm_rank);
    int *fill = (int *) (inbox.shm_p + offset);
    byte *memory_start = inbox.shm_p + offset + 2 * sizeof(int);
    memcpy(memory_start + (*fill) * sizeof(envelope_t), &envelope, sizeof(envelope_t));
    *fill = ((*fill) + 1) % (MAX_MESSAGE_COUNT_PER_SENDER);

    ///// CRITICAL SECTION /////

    sem_post(inbox.lock);
    sem_post(inbox.sem_full);

    MPI_debug_print("INFO", "%d has notified %d that the inbox%d has been filled\n", comm_rank, dest, dest);

    return 0;
}

void
MPI_create_shared_memory(char *name, size_t size, byte **out_shm_pointer, int *out_fd)
{
    int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0)
    {
        MPI_debug_print("ERROR", "Unable to open a shared memory segment \"%s\".\n", name);
        exit(errno);
    }
    ftruncate(shm_fd, (long) size);

    byte *shm_pointer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (MAP_FAILED == shm_pointer)
    {
        MPI_debug_print("ERROR", "Unable to map shared memory address space for \"%s\".\n", name);
        exit(errno);
    }

    *out_fd = shm_fd;
    *out_shm_pointer = shm_pointer;
}

void
MPI_create_or_open_semaphore(char *name, sem_t **out_sem, int initial)
{
    sem_t *sem = sem_open(name, O_CREAT, 0644, initial);
    *out_sem = sem;
}

void
MPI_debug_print(char *tag, char *fmt, ...)
{
#if DEBUG
    va_list args;
    va_start(args, fmt);
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

    printf("[%s]\t(%lld.%.9ld) ", (tag), (long long) spec.tv_sec, spec.tv_nsec);
    vprintf(fmt, args);

    va_end(args);
#endif
}
