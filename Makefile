CC     = gcc
CFLAGS = -Wall -Wextra -g
OBJS   = main.o

all: os1

os1: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o os1

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f os1 *.o
