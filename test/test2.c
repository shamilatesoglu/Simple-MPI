//
// Created by MSA on 25/04/2021.
//

#include <stdio.h>
#include <mpi.h>

int
main(int argc, char *argv[], char *environ[])
{
    int npes, myrank, number, i;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(&npes);
    MPI_Comm_rank(&myrank);
    if (myrank == 0)
    {
        printf("%d is waiting data from %d\n", myrank, 1);
        MPI_Recv(&number, 1, sizeof(int), 1, 0);
        printf("RECEIVED!\n");
    }
    else
    {
        printf("%d is sending data (%d) to %d\n", myrank, myrank, 0);
        MPI_Send(&myrank, 1, sizeof(int), 0, 0);
        printf("SENT!\n");
    }

    printf("STAGE 2!\n");

    if (myrank == 0)
    {
        printf("%d is waiting data from %d\n", myrank, 1);
        MPI_Recv(&number, 1, sizeof(int), 1, 1);
        printf("RECEIVED >>> 2\n");
    }
    else
    {
        printf("%d is sending data (%d) to %d\n", myrank, myrank, 0);
        MPI_Send(&myrank, 1, sizeof(int), 0, 0);
        printf("SENT >>> 2\n");
    }

    printf("FINISHED %d\n", myrank);

    MPI_Finalize();
}