cmake_minimum_required(VERSION 3.10)
project(BBM342_Project_I C)

set(CMAKE_C_STANDARD 11)

add_executable(my_mpirun src/main.c src/mpi.h src/mpi.c)