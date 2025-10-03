CC = gcc
CFLAGS = -g -Wall -Wextra -std=c11

.PHONY: all clean

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c

clean:
	rm -f aesdsocket *.o