# NAME: Michael Huo
# EMAIL: EMAIL
# ID: UID

CC=gcc
CFLAGS=-Wall -Wextra
LINK=-lm -lmraa

.PHONY: clean check dist

default: lab4b

lab4b: lab4b.c
	@$(CC) -o lab4b $(CFLAGS) lab4b.c $(LINK)

check:
	@python3 smoke.py

clean:
	@rm -f lab4b lab4b-UID.tar.gz

dist:
	@tar -czf lab4b-UID.tar.gz lab4b.c Makefile README smoke.py
