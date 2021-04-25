#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

void
create_processes(pid_t *processes, char *executable_path, int n, char *environ[]);

int
main(int argc, char *argv[], char *environ[])
{
    if (argc != 3)
    {
        printf("There must be 2 arguments in the following order: The number of instances to create, and an executable path.");
    }

    int number_of_processes = (int) strtol(argv[1], NULL, 10);
    char *executable_path = argv[2];

    pid_t processes[number_of_processes];
    create_processes(processes, executable_path, number_of_processes, environ);

    for (int i = 0; i < number_of_processes; i++)
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