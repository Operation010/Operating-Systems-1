#Mohammad Hammad Arif s3668630
CC     = gcc
CFLAGS = -Wall -Wextra -g

all: os1

os1: main.o
	$(CC) $(CFLAGS) main.o -o os1

main.o: main.c header.h
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f os1 *.o
