cmake_minimum_required(VERSION 3.10)
project(BBM342_Project_I C)

include_directories(lib)

add_executable(my_mpirun src/my_mpirun.c lib/mpi.h lib/mpi.c)
add_executable(hello src/hello.c lib/mpi.h lib/mpi.c)