
#ifndef PROJECT_I__MPI_H
#define PROJECT_I__MPI_H

#include <semaphore.h>

#define SEM_MUTEX_NAME_FORMAT "inbox%d_mutex"
#define SEM_FULL_NAME_FORMAT "inbox%d_full"
#define SEM_EMPTY_NAME_FORMAT "inbox%d_empty"
#define SEM_INITIALIZED_NAME_FORMAT "p%d_initialized"
#define SEM_GO_NAME_FORMAT "p%d_go"
#define SEM_TERMINATE_NAME_FORMAT "p%d_terminate"
#define SHM_INBOX_NAME_FORMAT "shm_inbox%d"
#define SHM_INBOX_STATUS_NAME_FORMAT "shm_inbox%d_status"

typedef struct
{
    sem_t *lock;
    sem_t *sem_sent;
    sem_t *sem_received;
    void *shm_p;
} inbox_t;

typedef struct
{
    sem_t *init;
    sem_t *go;
    sem_t *terminate;
} process_status_t;

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
