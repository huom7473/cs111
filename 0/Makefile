# NAME: Michael Huo
# EMAIL: EMAIL
# ID: UID

CC=gcc
CFLAGS=-Wall -Wextra -g

.PHONY: clean check

lab0: lab0.c
	@$(CC) -o lab0 $(CFLAGS) lab0.c

check:
	@python3 smoke.py

clean:
	@rm -f lab0 lab0-UID.tar.gz

dist: lab0.c Makefile README smoke.py backtrace.png breakpoint.png
	@tar -czf lab0-UID.tar.gz lab0.c Makefile README smoke.py backtrace.png breakpoint.png
