OBJ = cmd-args.o lnvm-manager.o
CC = gcc
CFLAGS = -g
CFLAGSXX =
DEPS = lnvm-manager.h

all: lnvm

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

lnvm : $(OBJ)
	$(CC) $(CFLAGS) $(CFLAGSXX) $(OBJ) -o lnvm -llightnvm -lpthread

clean:
	rm -f *.o lnvm
