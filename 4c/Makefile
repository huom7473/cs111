# NAME: Michael Huo
# EMAIL: EMAIL
# ID: UID

CC=gcc
CFLAGS=-Wall -Wextra
LINK=-lm -lmraa -lssl -lcrypto

.PHONY: clean check dist default

default: lab4c_tcp
	@ln -f lab4c_tcp lab4c_tls # avoid compiling twice

lab4c_tcp: lab4c.c
	@$(CC) -o lab4c_tcp $(CFLAGS) lab4c.c $(LINK)

lab4c_tls: lab4c.c
	@$(CC) -o lab4c_tls $(CFLAGS) lab4c.c $(LINK)

clean:
	@rm -f lab4c_tcp lab4c_tls lab4c-UID.tar.gz

dist:
	@tar -czf lab4c-UID.tar.gz lab4c.c Makefile README
