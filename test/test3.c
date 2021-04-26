//
// Created by msa on 4/26/21.
//

#include <stdio.h>
#include <mpi.h>
#include <unistd.h>

int
main(int argc, char *argv[], char *environ[])
{
    int npes, myrank, number;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(&npes);
    MPI_Comm_rank(&myrank);
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    if (myrank % 2 == 0)
    {
        int other = (myrank + 1) % npes;
        printf("%d is waiting data from %d\n", myrank, other);
        MPI_Recv(&number, 1, sizeof(int), other, 0);
        printf("%d has received data (%d) from %d\n", myrank, number, other);
        char ack[4];
        sprintf(ack, "ack");
        printf("%d is sending data (%s) to %d\n", myrank, ack, other);
        MPI_Send(ack, 1, sizeof(char) * 4, other, 0);
        printf("%d has sent data (%s) to %d\n", myrank, ack, other);
    }
    else
    {
        number = myrank;
        int other = (myrank - 1 + npes) % npes;
        printf("%d is sending data (%d) to %d\n", myrank, number, other);
        MPI_Send(&number, 1, sizeof(int), other, 0);
        printf("%d has sent data (%d) to %d\n", myrank, number, other);
        printf("%d is waiting data from %d\n", myrank, other);
        char ack[4];
        MPI_Recv(ack, 1, sizeof(char) * 4, other, 0);
        printf("%d has received data (%s) from %d\n", myrank, ack, other);
    }
#pragma clang diagnostic pop
    sleep(5);
    MPI_Finalize();
}