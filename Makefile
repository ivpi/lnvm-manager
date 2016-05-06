OBJ = lnvm-manager.o
CC = gcc
CFLAGS = -g 
CFLAGSXX =

lnvm : $(OBJ)
	$(CC) $(CFLAGS) $(CFLAGSXX) $(OBJ) -o lnvm -llightnvm -lpthread 

lnvm-manager.o : lnvm-manager.c
	$(CC) $(CFLAGSXX) -c lnvm-manager.c
