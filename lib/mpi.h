
#ifndef PROJECT_I__MPI_H
#define PROJECT_I__MPI_H

#include <semaphore.h>

typedef struct
{
    sem_t *init;
    sem_t *go;
    sem_t *mutex;
    sem_t *sem_empty;
    sem_t *sem_full;
    void *shm_p;
    int shm_fd;
    int *use;
    int *fill;
} inbox_t;

int
MPI_Init(int *argc, char ***argv);

int
MPI_Finalize();

/**
 * Number of processes in the communication group.
 * @param size [out] The number of processes in this communication group is passed to this parameter.
 */
int
MPI_Comm_size(int *size);

/**
 * Rank of the calling process in the group of communication.
 * @param rank [out]
 */
int
MPI_Comm_rank(int *rank);

int
MPI_Recv(void *out, int count, int size, int source, int tag);

int
MPI_Send(const void *data, int count, int size, int dest, int tag);

#endif // PROJECT_I__MPI_H
