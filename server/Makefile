USE_AESD_CHAR_DEVICE ?= 0
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?= -lpthread
TARGET ?= aesdsocket

all: $(TARGET)

default: all

${TARGET}:
	$(CC) $(LDFLAGS) $(CFLAGS) -DUSE_AESD_CHAR_DEVICE=$(USE_AESD_CHAR_DEVICE) aesdsocket.c -o aesdsocket

clean:
	rm -f *.o aesdsocket
