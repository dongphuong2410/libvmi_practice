CC=gcc
CFLAGS=-ggdb -O0
LDFLAGS=-lvmi -ljson-c

all: fileaccess.c util.c
	$(CC) $^ -o fileaccess $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf fileaccess

