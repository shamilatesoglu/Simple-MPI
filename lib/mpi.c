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

#define BOUNDED_BUFFER_SIZE 48
#define MAX_CHANNEL 100

#define SEM_FULL_NAME_FORMAT "inbox%d_full"
#define SEM_EMPTY_NAME_FORMAT "inbox%d_empty"
#define SHM_NAME_FORMAT "shm_inbox%d"

#define DEBUG 0

void
debug_print(char *tag, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

    printf("[%s] (%lld.%.9ld) ", (tag), (long long) spec.tv_sec, spec.tv_nsec);
    vprintf(fmt, args);

    va_end(args);
}

int comm_size;
int comm_rank;

sem_t *mutex;

inbox_t inboxes[MAX_CHANNEL];

/* Private methods */


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
        char name[100];
        sprintf(name, SHM_NAME_FORMAT, i);
        int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
        if (shm_fd < 0)
        {
            printf("Unable to open a shared memory segment \"%s\".\n", name);
            exit(0);
        }
        ftruncate(shm_fd, BOUNDED_BUFFER_SIZE);

        void *shm_pointer = mmap(NULL, BOUNDED_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (MAP_FAILED == shm_pointer)
        {
            printf("Unable to map shared memory address space for \"%s\".\n", name);
            exit(0);
        }

        char f_name[100];
        sprintf(f_name, SEM_FULL_NAME_FORMAT, i);
        sem_t *full = sem_open(f_name, O_CREAT, 0600, 0);
        sem_init(full, 1, 0);

        char e_name[100];
        sprintf(e_name, SEM_EMPTY_NAME_FORMAT, i);
        sem_t *empty = sem_open(e_name, O_CREAT, 0600, 0);
        sem_init(empty, 1, BOUNDED_BUFFER_SIZE);

        inbox_t ch;
        ch.sem_full = full;
        ch.sem_empty = empty;
        ch.shm_fd = shm_fd;
        ch.shm_p = shm_pointer;
        ch.use = 0;
        ch.fill = 0;
        inboxes[i] = ch;
    }

    mutex = sem_open("mutex_lock", O_CREAT, 0600, 0);
    sem_init(mutex, 1, 1);

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
        inbox_t ch = inboxes[i];

        char name[100];
        sprintf(name, SHM_NAME_FORMAT, i);
        shm_unlink(name);
        sem_close(ch.sem_full);
        sem_close(ch.sem_empty);
        sem_destroy(ch.sem_full);
        sem_destroy(ch.sem_empty);
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
    debug_print("INFO", "inbox%d full: %d empty: %d\n", comm_rank, fval, eval);
#endif
    sem_wait(inbox.sem_full);
    sem_wait(mutex);
    memcpy(out, inbox.shm_p + inbox.use, count * size);
    inbox.use = (inbox.use + count * size) % BOUNDED_BUFFER_SIZE;
#if DEBUG
    debug_print("INFO", "%d has read the message sent by %d\n", comm_rank, source);
    sem_getvalue(inbox.sem_full, &fval);
    sem_getvalue(inbox.sem_empty, &eval);
    debug_print("INFO", "inbox%d full: %d empty: %d\n", comm_rank, fval, eval);
#endif
    sem_post(mutex);
    sem_post(inbox.sem_empty);
#if DEBUG
    debug_print("INFO", "%d has notified %d that the inbox%d has been emptied\n", comm_rank, source, comm_rank);
    sem_getvalue(inbox.sem_full, &fval);
    sem_getvalue(inbox.sem_empty, &eval);
    debug_print("INFO", "inbox%d full: %d empty: %d\n", comm_rank, fval, eval);
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
    debug_print("INFO", "inbox%d full: %d empty: %d\n", dest, fval, eval);
#endif
    sem_wait(inbox.sem_empty);
    sem_wait(mutex);
    memcpy(inbox.shm_p + inbox.fill, data, count * size);
    inbox.fill = (inbox.fill + count * size) % BOUNDED_BUFFER_SIZE;
#if DEBUG
    debug_print("INFO", "%d has written a message to inbox%d\n", comm_rank, dest);
    sem_getvalue(inbox.sem_full, &fval);
    sem_getvalue(inbox.sem_empty, &eval);
    debug_print("INFO", "inbox%d full: %d empty: %d\n", dest, fval, eval);
#endif
    sem_post(mutex);
    sem_post(inbox.sem_full);
#if DEBUG
    debug_print("INFO", "%d has notified %d that the inbox%d has been filled\n", comm_rank, dest, dest);
    sem_getvalue(inbox.sem_full, &fval);
    sem_getvalue(inbox.sem_empty, &eval);
    debug_print("INFO", "inbox%d full: %d empty: %d\n", dest, fval, eval);
#endif
    return 0;
}

void
MPI_Print_memory()
{
    for (int i = 0; i < comm_size; i++)
    {
        printf("inbox%d: ", i);
        for (int j = 0; j < BOUNDED_BUFFER_SIZE; j += 4)
        {
            printf("|%d", *(int *) (inboxes[i].shm_p + j));
        }
        printf("| # ");
    }
    printf("\n");
}