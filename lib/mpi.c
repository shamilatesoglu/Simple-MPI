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

#define MAX_MESSAGE_COUNT_PER_PROCESS 1
#define MAX_MESSAGE_SIZE 256
#define MAX_PROCESS_COUNT 100

#define DEBUG 0
#define PRINT_MEMORY 0

static int comm_size;
static int comm_rank;

static inbox_t inboxes[MAX_PROCESS_COUNT];

static process_status_t processes[MAX_PROCESS_COUNT];

/* Private methods */
void
MPI_debug_print(char *tag, char *fmt, ...);

void
MPI_debug_sprint_memory(char *out);

void
MPI_create_shared_memory(char *name, int size, void **out_shm_pointer, int *out_fd);

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
        int shm_inbox_fd;
        void *shm_inbox_pointer;
        MPI_create_shared_memory(inbox_name, (MAX_MESSAGE_COUNT_PER_PROCESS * MAX_MESSAGE_SIZE) , &shm_inbox_pointer, &shm_inbox_fd);

        char status_name[100];
        sprintf(status_name, SHM_INBOX_STATUS_NAME_FORMAT, i);
        int shm_inbox_status_fd;
        void *shm_inbox_status_pointer;
        MPI_create_shared_memory(status_name, 2 * sizeof(int), &shm_inbox_status_pointer, &shm_inbox_status_fd);

        char init_name[100];
        sprintf(init_name, SEM_INITIALIZED_NAME_FORMAT, i);
        sem_t *init;
        MPI_create_or_open_semaphore(init_name, &init, 0);

        char go_name[100];
        sprintf(go_name, SEM_GO_NAME_FORMAT, i);
        sem_t *go;
        MPI_create_or_open_semaphore(go_name, &go, 0);

        char terminate_name[100];
        sprintf(terminate_name, SEM_TERMINATE_NAME_FORMAT, i);
        sem_t *terminate;
        MPI_create_or_open_semaphore(terminate_name, &terminate, 0);

        process_status_t process_status;
        process_status.init = init;
        process_status.go = go;
        process_status.terminate = terminate;
        processes[i] = process_status;

        char mutex_name[100];
        sprintf(mutex_name, SEM_MUTEX_NAME_FORMAT, i);
        sem_t *mutex;
        MPI_create_or_open_semaphore(mutex_name, &mutex, 1);

        char full_name[100];
        sprintf(full_name, SEM_FULL_NAME_FORMAT, i);
        sem_t *sent;
        MPI_create_or_open_semaphore(full_name, &sent, 0);

        char empty_name[100];
        sprintf(empty_name, SEM_EMPTY_NAME_FORMAT, i);
        sem_t *received;
        MPI_create_or_open_semaphore(empty_name, &received, MAX_MESSAGE_COUNT_PER_PROCESS);

        inbox_t inbox;
        inbox.lock = mutex;
        inbox.sem_full = sent;
        inbox.sem_empty = received;
        inbox.shm_p = shm_inbox_pointer;
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

            char name[100];
            sprintf(name, SHM_INBOX_NAME_FORMAT, i);
            shm_unlink(name);

            char status_name[100];
            sprintf(status_name, SHM_INBOX_STATUS_NAME_FORMAT, i);
            shm_unlink(status_name);

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

    memcpy(out, inbox.shm_p, count * size);

    sem_post(inbox.lock);
    sem_post(inbox.sem_empty);

    MPI_debug_print("INFO", "%d has notified everyone that the inbox%d has been emptied\n", comm_rank, source, comm_rank);

    return 0;
}

int
MPI_Send(const void *data, int count, int size, int dest, int tag)
{
    inbox_t inbox = inboxes[dest];

    MPI_debug_print("INFO", "%d is waiting for inbox%d to be emptied\n", comm_rank, dest);

    sem_wait(inbox.sem_empty);
    sem_wait(inbox.lock);

    memcpy(inbox.shm_p, data, count * size);

    sem_post(inbox.lock);
    sem_post(inbox.sem_full);

    MPI_debug_print("INFO", "%d has notified %d that the inbox%d has been filled\n", comm_rank, dest, dest);

    return 0;
}

void
MPI_create_shared_memory(char *name, int size, void **out_shm_pointer, int *out_fd)
{
    int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0)
    {
        MPI_debug_print("ERROR", "Unable to open a shared memory segment \"%s\".\n", name);
        exit(0);
    }
    ftruncate(shm_fd, MAX_MESSAGE_COUNT_PER_PROCESS);

    void *shm_pointer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (MAP_FAILED == shm_pointer)
    {
        MPI_debug_print("ERROR", "Unable to map shared memory address space for \"%s\".\n", name);
        exit(0);
    }

    *out_fd = shm_fd;
    *out_shm_pointer = shm_pointer;
}

void
MPI_create_or_open_semaphore(char *name, sem_t **out_sem, int initial)
{
    sem_t *sem = sem_open(name, O_EXCL | O_CREAT, 0644, 0);
    if (errno == EEXIST)
    {
        *out_sem = sem_open(name, O_CREAT, 0644, 0);
    }
    else
    {
        int status = sem_init(sem, 1, initial);
        if (status != 0)
        {
            MPI_debug_print("ERROR", "Error while initializing semaphore %s: %s\n", name, strerror(errno));
        }
        else
        {
            *out_sem = sem;
        }
    }
}

void
MPI_debug_sprint_memory(char *out)
{
    //char header[100];
    //sprintf(header, "MEMORY as seen by %d: \n", comm_rank);
    //strcat(out, header);
    //for (int i = 0; i < comm_size; i++)
    //{
    //    char buffer[10000];
    //    sprintf(buffer, "inbox%d: (fill: %d use: %d) # ", i, *inboxes[i].fill, *inboxes[i].use);
    //    strcat(out, buffer);
    //    buffer[0] = '\0';
    //    for (int j = 0; j < MAX_MESSAGE_COUNT_PER_PROCESS; j += 4)
    //    {
    //        sprintf(buffer, "|%7d", *(int *) (inboxes[i].shm_p + j));
    //        strcat(out, buffer);
    //        buffer[0] = '\0';
    //    }
    //    strcat(out, "|\n");
    //}
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