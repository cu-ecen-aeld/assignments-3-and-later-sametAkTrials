USE_AESD_CHAR_DEVICE ?= 1
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g -DUSE_AESD_CHAR_DEVICE=$(USE_AESD_CHAR_DEVICE)
LDFLAGS ?= -lpthread
TARGET ?= aesdsocket

all: $(TARGET)

default: all

${TARGET}:
	$(CC) $(LDFLAGS) $(CFLAGS) aesdsocket.c -o aesdsocket

clean:
	rm -f *.o aesdsocket
