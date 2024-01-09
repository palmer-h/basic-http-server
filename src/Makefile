CC=gcc
CFLAGS=-Wall -Wextra

OBJS=date_utils.o mime.o socket.o

all: server

server: $(OBJS)
	gcc -o $@ $^

date_utils.o: date_utils.c date_utils.h

mime.o: mime.c net.h

socket.o: socket.c socket.h

clean:
	rm -f $(OBJS)
	rm -f server

.PHONY: all, clean
