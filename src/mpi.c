#include "mpi.h"

int
MPI_Init(int *argc, char ***argv)
{
    return 0;
}

int
MPI_Finalize()
{
    return 0;
}

int
MPI_Comm_size(int *size)
{
    return 0;
}

int
MPI_Comm_rank(int *rank)
{
    return 0;
}

int
MPI_Recv(void *buf, int count, int datatype, int source, int tag)
{
    return 0;
}

int
MPI_Send(const void *buf, int count, int datatype, int dest, int tag)
{
    return 0;
}