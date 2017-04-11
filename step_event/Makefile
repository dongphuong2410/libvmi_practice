CC=gcc
CFLAGS=-I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
LDFLAGS=-lxenctrl -lglib-2.0 -lxenlight -lxentoollog -lvmi

all: main.c
	$(CC) $^ -o step_event $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf step_event


