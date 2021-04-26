#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <mpi.h>
#include <fcntl.h>    /* For O_* constants */

void
create_processes(pid_t *processes, char *executable_path, int n, char *environ[]);

int
main(int argc, char *argv[], char *environ[])
{
    if (argc != 3)
    {
        printf("There must be 2 arguments in the following order: The number of instances to create, and an executable path.");
    }

    int process_count = (int) strtol(argv[1], NULL, 10);
    char *executable_path = argv[2];

    sem_t *init[process_count], *go[process_count], *terminate[process_count];
    char name[10000];
    for (int i = 0; i < process_count; i++)
    {
        sprintf(name, SEM_INITIALIZED_NAME_FORMAT, i);
        init[i] = sem_open(name, O_CREAT, 0600, 0);
        sem_init(init[i], 1, 0);

        sprintf(name, SEM_GO_NAME_FORMAT, i);
        go[i] = sem_open(name, O_CREAT, 0600, 0);
        sem_init(go[i], 1, 0);

        sprintf(name, SEM_TERMINATE_NAME_FORMAT, i);
        terminate[i] = sem_open(name, O_CREAT, 0600, 0);
        sem_init(terminate[i], 1, 0);
    }

    pid_t processes[process_count];
    create_processes(processes, executable_path, process_count, environ);

    for (int i = 0; i < process_count; i++)
        sem_wait(init[i]);            // Wait for each process to call MPI_Init

    for (int i = 0; i < process_count; i++)
        sem_post(go[i]);              // Wait for each process to complete its MPI_Init tasks (for safe initialization of data structures, semaphores, etc.)

    for (int i = 0; i < process_count; i++)
        sem_wait(terminate[i]);       // Wait for each process to call MPI_Finalize


    for (int i = 0; i < process_count; i++)
    {
        waitpid(processes[i], NULL, 0);
    }

    return 0;
}

void
create_processes(pid_t *processes, char *executable_path, int n, char *environ[])
{
    for (int i = 0; i < n; i++)
    {
        if ((processes[i] = fork()) == 0)
        {
            char arg_rank[10], arg_n[10];
            sprintf(arg_rank, "%d", i);
            sprintf(arg_n, "%d", n);

            char *argv[] = {executable_path, arg_rank, arg_n, NULL};

            if (execve(executable_path, argv, environ) == -1)
            {
                perror("Error while executing program");
            }
        }
    }
}