CC = gcc -Wall -g

all: server client

server: common.h server_sol.c io.c
	$(CC) -o server server_sol.c  io.c

client: common.h client_sol.c io.c
	$(CC) -o client client_sol.c io.c

.PHONY: clean

clean:
	rm -f server client
