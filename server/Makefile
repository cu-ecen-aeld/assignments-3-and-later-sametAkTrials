CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?= -lpthread
TARGET ?= aesdsocket

all: $(TARGET)

default: all

${TARGET}:
	$(CC) $(LDFLAGS) $(CFLAGS) aesdsocket.c -o aesdsocket

clean:
	rm -f *.o aesdsocket
