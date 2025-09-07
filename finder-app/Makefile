CC = gcc
CFLAGS = -g -Wall -Wextra -std=c11



writer: writer.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o writer writer.c

clean:
	rm -f writer *.o