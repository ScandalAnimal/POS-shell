CC=gcc
CFLAGS=-Wall -g -Wextra -pedantic -std=c99

fork:
	$(CC) $(CFLAGS) fork.c -o fork -lpthread
clean:
	rm -f fork core*

