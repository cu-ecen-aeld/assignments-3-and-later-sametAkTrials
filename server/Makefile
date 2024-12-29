CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wall -Werror -g

all: aesdsocket

default: aesdsocket

aesdsocket:
	$(CC) -pthread $(CFLAGS) aesdsocket.c -o aesdsocket

clean:
	rm -f *.o aesdsocket