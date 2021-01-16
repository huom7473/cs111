# NAME: Michael Huo
# EMAIL: EMAIL
# ID: UID

CC=gcc
CFLAGS=-Wall -Wextra

.PHONY: clean

lab1a: lab1.c
	@$(CC) -o lab1a $(CFLAGS) lab1.c

clean:
	@rm -f lab1a lab1a-UID.tar.gz

dist: lab1.c Makefile README
	@tar -czf lab1a-UID.tar.gz lab1.c Makefile README
