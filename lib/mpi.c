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

#define BOUNDED_BUFFER_SIZE 48
#define MAX_INBOX 100

#define SEM_MUTEX_NAME_FORMAT "inbox%d_mutex"
#define SEM_FULL_NAME_FORMAT "inbox%d_full"
#define SEM_EMPTY_NAME_FORMAT "inbox%d_empty"
#define SEM_INITIALIZED_NAME_FORMAT "p%d_initialized"
#define SEM_GO_NAME_FORMAT "p%d_go"
#define SHM_INBOX_NAME_FORMAT "shm_inbox%d"
#define SHM_INBOX_STATUS_NAME_FORMAT "shm_inbox%d_status"

#define DEBUG 1
#define PRINT_MEMORY 0

void
debug_print(char *tag, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

    printf("[%s]\t(%lld.%.9ld) ", (tag), (long long) spec.tv_sec, spec.tv_nsec);
    vprintf(fmt, args);

    va_end(args);
}

static int comm_size;
static int comm_rank;

static inbox_t inboxes[MAX_INBOX];

/* Private methods */
void
debug_sprint_memory(char *out);

void
create_shared_memory(char *name, int size, void **out_shm_pointer, int *out_fd);

void
create_semaphore(char *name, sem_t **out_sem, int initial);

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
        create_shared_memory(inbox_name, BOUNDED_BUFFER_SIZE, &shm_inbox_pointer, &shm_inbox_fd);

        char status_name[100];
        sprintf(status_name, SHM_INBOX_STATUS_NAME_FORMAT, i);
        int shm_inbox_status_fd;
        void *shm_inbox_status_pointer;
        create_shared_memory(status_name, 2 * sizeof(int), &shm_inbox_status_pointer, &shm_inbox_status_fd);

        char mutex_name[100];
        sprintf(mutex_name, SEM_MUTEX_NAME_FORMAT, i);
        sem_t *mutex;
        create_semaphore(mutex_name, &mutex, 1);

        char init_name[100];
        sprintf(init_name, SEM_INITIALIZED_NAME_FORMAT, i);
        sem_t *initialized;
        create_semaphore(init_name, &initialized, 0);

        char go_name[100];
        sprintf(go_name, SEM_GO_NAME_FORMAT, i);
        sem_t *go;
        create_semaphore(go_name, &go, 0);

        char full_name[100];
        sprintf(full_name, SEM_FULL_NAME_FORMAT, i);
        sem_t *full;
        create_semaphore(full_name, &full, 0);

        char empty_name[100];
        sprintf(empty_name, SEM_EMPTY_NAME_FORMAT, i);
        sem_t *empty;
        create_semaphore(empty_name, &empty, BOUNDED_BUFFER_SIZE);

        inbox_t inbox;
        inbox.init = initialized;
        inbox.go = go;
        inbox.mutex = mutex;
        inbox.sem_full = full;
        inbox.sem_empty = empty;
        inbox.shm_fd = shm_inbox_fd;
        inbox.shm_p = shm_inbox_pointer;
        inbox.fill = (int *) (shm_inbox_status_pointer);
        inbox.use = (int *) (shm_inbox_status_pointer + sizeof(int));
        *(inbox.fill) = 0;
        *(inbox.use) = 0;
        inboxes[i] = inbox;
    }

    if (comm_rank == 0)
    {
        for (int i = 1; i < comm_size; i++)
        {
            debug_print("INFO", "Root process is waiting for %d\n", i);
            sem_wait(inboxes[i].init);
        }
        for (int i = 0; i < comm_size; i++)
        {
            debug_print("INFO", "Root process is sending go signal to %d\n", i);
            sem_post(inboxes[i].go);
        }
        sem_wait(inboxes[0].go);
    }
    else
    {
        debug_print("INFO", "%d is notifying root that it has been successfully initialized\n", comm_rank);
        sem_post(inboxes[comm_rank].init);
        debug_print("INFO", "%d is waiting for go signal from root\n", comm_rank);
        sem_wait(inboxes[comm_rank].go);
    }

    return 0;
}

int
MPI_Finalize()
{
#if DEBUG
    debug_print("INFO", "Finalize %d\n", comm_rank);
#endif
    for (int i = 0; i < comm_size; i++)
    {
        inbox_t inbox = inboxes[i];

        char name[100];
        sprintf(name, SHM_INBOX_NAME_FORMAT, i);
        shm_unlink(name);

        char status_name[100];
        sprintf(status_name, SHM_INBOX_STATUS_NAME_FORMAT, i);
        shm_unlink(status_name);

        sem_close(inbox.init);
        sem_close(inbox.sem_full);
        sem_close(inbox.sem_empty);
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

#if DEBUG
    debug_print("INFO", "%d is waiting for its inbox to be filled\n", comm_rank);
    int fval, eval;
    sem_getvalue(inbox.sem_full, &fval);
    sem_getvalue(inbox.sem_empty, &eval);
#if PRINT_MEMORY
    char m[10000];
    m[0] = '\0';
    debug_sprint_memory(m);
    printf("%s", m);
#endif
//    debug_print("INFO", "inbox%d full: %d empty: %d\n", comm_rank, fval, eval);
#endif

    sem_wait(inbox.sem_full);
    sem_wait(inbox.mutex);
    memcpy(out, inbox.shm_p + *(inbox.use), count * size);
    *(inbox.use) = (*(inbox.use) + count * size) % BOUNDED_BUFFER_SIZE;

#if DEBUG
    debug_print("INFO", "%d has read the message sent by %d\n", comm_rank, source);
    sem_getvalue(inbox.sem_full, &fval);
    sem_getvalue(inbox.sem_empty, &eval);
//    debug_print("INFO", "inbox%d full: %d empty: %d\n", comm_rank, fval, eval);
#endif

    sem_post(inbox.mutex);
    sem_post(inbox.sem_empty);

#if DEBUG
    debug_print("INFO", "%d has notified %d that the inbox%d has been emptied\n", comm_rank, source, comm_rank);
    sem_getvalue(inbox.sem_full, &fval);
    sem_getvalue(inbox.sem_empty, &eval);
//    debug_print("INFO", "inbox%d full: %d empty: %d\n", comm_rank, fval, eval);
#endif
    return 0;
}

int
MPI_Send(const void *data, int count, int size, int dest, int tag)
{
    inbox_t inbox = inboxes[dest];

#if DEBUG
    debug_print("INFO", "%d is waiting for inbox%d to be emptied\n", comm_rank, dest);
    int fval, eval;
    sem_getvalue(inbox.sem_full, &fval);
    sem_getvalue(inbox.sem_empty, &eval);
//    debug_print("INFO", "inbox%d full: %d empty: %d\n", dest, fval, eval);
#endif

    sem_wait(inbox.sem_empty);
    sem_wait(inbox.mutex);
    memcpy(inbox.shm_p + *(inbox.fill), data, count * size);
    *(inbox.fill) = (*(inbox.fill) + count * size) % BOUNDED_BUFFER_SIZE;

#if DEBUG
    debug_print("INFO", "%d has written a message to inbox%d\n", comm_rank, dest);
    sem_getvalue(inbox.sem_full, &fval);
    sem_getvalue(inbox.sem_empty, &eval);
//    debug_print("INFO", "inbox%d full: %d empty: %d\n", dest, fval, eval);
#endif

    sem_post(inbox.mutex);
    sem_post(inbox.sem_full);

#if DEBUG
    debug_print("INFO", "%d has notified %d that the inbox%d has been filled\n", comm_rank, dest, dest);
    sem_getvalue(inbox.sem_full, &fval);
    sem_getvalue(inbox.sem_empty, &eval);
//    debug_print("INFO", "inbox%d full: %d empty: %d\n", dest, fval, eval);
#endif
    return 0;
}

void
create_shared_memory(char *name, int size, void **out_shm_pointer, int *out_fd)
{
    int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0)
    {
        printf("Unable to open a shared memory segment \"%s\".\n", name);
        exit(0);
    }
    ftruncate(shm_fd, BOUNDED_BUFFER_SIZE);

    void *shm_pointer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (MAP_FAILED == shm_pointer)
    {
        printf("Unable to map shared memory address space for \"%s\".\n", name);
        exit(0);
    }

    *out_fd = shm_fd;
    *out_shm_pointer = shm_pointer;
}

void
create_semaphore(char *name,  sem_t **out_sem, int initial)
{
    sem_t *sem = sem_open(name, O_CREAT, 0644, 0);
    int status = sem_init(sem, 1, initial);
    if (status != 0)
    {
        debug_print("ERROR", "Error while initializing semaphore %s: %s\n", name, strerror(errno));
    }
    else
    {
        *out_sem = sem;
    }
}

void
debug_sprint_memory(char *out)
{
    char header[100];
    sprintf(header, "MEMORY as seen by %d: \n", comm_rank);
    strcat(out, header);
    for (int i = 0; i < comm_size; i++)
    {
        char buffer[10000];
        sprintf(buffer, "inbox%d: (fill: %d use: %d) # ", i, *inboxes[i].fill, *inboxes[i].use);
        strcat(out, buffer);
        buffer[0] = '\0';
        for (int j = 0; j < BOUNDED_BUFFER_SIZE; j += 4)
        {
            sprintf(buffer, "|%7d", *(int *) (inboxes[i].shm_p + j));
            strcat(out, buffer);
            buffer[0] = '\0';
        }
        strcat(out, "|\n");
    }
}