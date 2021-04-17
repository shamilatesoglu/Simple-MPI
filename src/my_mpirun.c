#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

void create_processes(char *executable_path, int n, char *environ[]);

int main(int argc, char *argv[], char *environ[])
{
    if (argc != 3)
    {
        printf("There must be 2 arguments in the following order: The number of instances to create, and an executable path.");
    }

    int number_of_processes = atoi(argv[1]);
    char *executable_path = argv[2];

    create_processes(executable_path, number_of_processes, environ);

    return 0;
}

void create_processes(char *executable_path, int n, char *environ[])
{
    int pid[n];
    for (int i = 0; i < n; i++)
    {
        if ((pid[i] = fork()) == 0)
        {
            char arg_rank[10];
            sprintf(arg_rank, "%d", i);  

            char* argv[] = {executable_path, arg_rank, NULL};

            if (execve(executable_path, argv, environ) == -1)
            {
                perror("Error while executing program");
            }
        }
    }
}