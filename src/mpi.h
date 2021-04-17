

int MPI_Init(int *argc, char ***argv);
int MPI_Finalize();
int MPI_Comm_size(int *size);
int MPI_Comm_rank(int *rank);
int MPI_Recv(void *buf, int count, int datatype, int source, int tag);
int MPI_Send(const void *buf, int count, int datatype, int dest, int tag);

