#include <stdio.h>
#include <mpi.h>

int
main(int argc, char *argv[], char *environ[])
{
    int npes, myrank, number;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(&npes);
    MPI_Comm_rank(&myrank);
    if (myrank % 2 == 0)
    {
        int other = (myrank + 1) % npes;
        printf("%d is waiting data from %d\n", myrank, other);
        MPI_Recv(&number, 1, sizeof(int), other, 0);
        printf("From process %d to process %d data = %d, RECEIVED!\n", other, myrank, number);
        printf("%d: is sending data (%d) to %d\n", myrank, number, other);
        MPI_Send(&number, 1, sizeof(int), other, 0);
        printf("From process %d to process %d data = %d, SENT!\n", myrank, other, number);
    }
    else
    {
        number = myrank;
        int other = (myrank - 1 + npes) % npes;
        printf("%d: is sending data (%d) to %d\n", myrank, number, other);
        MPI_Send(&number, 1, sizeof(int), other, 0);
        printf("From process %d to process %d data = %d, SENT!\n", myrank, other, number);
        printf("%d is waiting data from %d\n", myrank, other);
        MPI_Recv(&number, 1, sizeof(int), other, 0);
        printf("From process %d to process %d data = %d, RECEIVED!\n", other, myrank, number);
    }
    MPI_Finalize();
}
