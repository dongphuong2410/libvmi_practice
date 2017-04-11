CC=gcc
CFLAGS=
LDFLAGS=-lvmi -ljson-c

all: syscalls.c util.c
	$(CC) $^ -o syscalls $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf syscalls

