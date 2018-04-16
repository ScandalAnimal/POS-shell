CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99 -g3
BIN=pos-shell

ALL: $(BIN)

$(BIN): pos-shell.o
	$(CC) $(CFLAGS) pos-shell.o -o $@ -lpthread

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN) *.o 2>/dev/null
