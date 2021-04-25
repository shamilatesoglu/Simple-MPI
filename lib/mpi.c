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

#define BOUNDED_BUFFER_SIZE 4096
#define MAX_CHANNEL 100

#define SEM_FULL_NAME_FORMAT "ch_%d_full_%d"
#define SEM_EMPTY_NAME_FORMAT "ch_%d_empty_%d"
#define SHM_NAME_FORMAT "shm_ch_%d"

int comm_size;
int comm_rank;

channel_t channels[MAX_CHANNEL];

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
        sprintf(f_name, SEM_FULL_NAME_FORMAT, i, comm_rank);
        sem_t *full = sem_open(f_name, O_CREAT, 0600, 0);
        sem_init(full, 1, 0);

        char e_name[100];
        sprintf(e_name, SEM_EMPTY_NAME_FORMAT, i, comm_rank);
        sem_t *empty = sem_open(e_name, O_CREAT, 0600, 0);
        sem_init(empty, 1, BOUNDED_BUFFER_SIZE);

        channel_t ch;
        ch.s_full = full;
        ch.s_empty = empty;
        ch.shm_fd = shm_fd;
        ch.shm_p = shm_pointer;
        ch.use = 0;
        ch.fill = 0;
        channels[i] = ch;
    }

    return 0;
}

int
MPI_Finalize()
{
    for (int i = 0; i < comm_size; i++)
    {
        channel_t ch = channels[i];

        char name[100];
        sprintf(name, SHM_NAME_FORMAT, i);
        shm_unlink(name);
        sem_close(ch.s_full);
        sem_close(ch.s_empty);
        sem_destroy(ch.s_full);
        sem_destroy(ch.s_empty);
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
MPI_Recv(void *buf, int count, int size, int source, int tag)
{
    channel_t ch = channels[source];
    sem_wait(ch.s_full);
    memcpy(buf, ch.shm_p + ch.use, count * size);
    ch.use = (ch.use + count * size) % BOUNDED_BUFFER_SIZE;
    sem_post(ch.s_empty);
    return 0;
}

int
MPI_Send(const void *buf, int count, int size, int dest, int tag)
{
    channel_t ch = channels[dest];
    sem_wait(channels[dest].s_empty);
    memcpy(ch.shm_p + ch.fill, buf, count * size);
    ch.fill = (ch.fill + count * size) % BOUNDED_BUFFER_SIZE;
    sem_post(ch.s_full);
    return 0;
}