# NAME: Michael Huo
# EMAIL: EMAIL
# ID: UID

CC=gcc
CFLAGS=-Wall -Wextra

.PHONY: clean default

default: client server

client: lab1b-client.c
	@$(CC) -o lab1b-client $(CFLAGS) lab1b-client.c -lz

server: lab1b-server.c
	@$(CC) -o lab1b-server $(CFLAGS) lab1b-server.c -lz

clean:
	@rm -f lab1b-client lab1b-server lab1b-UID.tar.gz

dist: lab1b-client.c lab1b-server.c Makefile README
	@tar -czf lab1b-UID.tar.gz lab1b-client.c lab1b-server.c Makefile README
