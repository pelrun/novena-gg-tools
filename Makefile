CC=gcc
CFLAGS=-O2

test: main.c
	$(CC) $(CFLAGS) -o test main.c
