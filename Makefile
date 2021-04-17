OBJS	= src/main.o src/mpi.o
SOURCE	= src/main.c src/mpi.c
HEADER	= src/mpi.h
INC=src/ ./
INC_PARAMS=$(foreach d, $(INC), -I$d)
OUT	= build/my_mpirun
CC	= gcc
LFLAGS	 = -lm

all: my_mpirun clean_obj_files

.PHONY: my_mpirun
my_mpirun: $(OBJS)
	@echo "[INFO] Linking C executable..."
	@$(CC)  $^ $(LFLAGS) -o $@
	@echo "[OK]   Built C executable"

%.o: %.c $(HEADER)
	@echo "[INFO] Compiling and assembling $<"
	@$(CC) $(INC_PARAMS) -c -o $@ $< $(LFLAGS)

.PHONY: clean_obj_files
clean_obj_files:
	@echo "[OK]   Cleaning object files..."
	@rm -f $(OBJS)
	@echo "[OK]   Object files are cleaned."

.PHONY: clean
clean:
	@echo "[INFO] Cleaning project..."
	@rm -f $(OBJS) $(OUT)
	@echo "[OK]   Project cleaned."
