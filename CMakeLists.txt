cmake_minimum_required(VERSION 3.10)
project(BBM342_Project_I C)

include_directories(lib)

add_library(mpi lib/mpi.h lib/mpi.c)

add_executable(my_mpirun src/my_mpirun.c)
add_executable(hello src/hello.c)
add_executable(test1 test/test1.c)
add_executable(test2 test/test2.c)

target_link_libraries(mpi -lpthread -lrt)
target_link_libraries(my_mpirun)
target_link_libraries(hello mpi)
target_link_libraries(test1 mpi)
target_link_libraries(test2 mpi)
