# NAME: Michael Huo, Tygan Zeng
# EMAIL: EMAIL, zengtygan@gmail.com
# ID: UID, 705391071

CC=gcc
CFLAGS=-Wall -Wextra

.PHONY: clean dist default

default: lab3a

lab3a: lab3a.c ext2_fs.h
	@$(CC) -o lab3a $(CFLAGS) lab3a.c

clean:
	@rm -f lab3a lab3a-UID.tar.gz

dist:
	@tar -czf lab3a-UID.tar.gz lab3a.c ext2_fs.h Makefile README
