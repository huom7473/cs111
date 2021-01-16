# NAME: Michael Huo
# EMAIL: EMAIL
# ID: UID

CC=gcc
CFLAGS=-Wall -Wextra

.PHONY: build add list clean tests graphs dist

build: add list

add: lab2_add

lab2_add: lab2_add.c
	@$(CC) -o lab2_add $(CFLAGS) lab2_add.c -pthread

list: lab2_list

lab2_list: lab2_list.c SortedList.c SortedList.h
	@$(CC) -o lab2_list $(CFLAGS) lab2_list.c SortedList.c -pthread

clean:
	@rm -f lab2_add lab2_list lab2a-UID.tar.gz

tests: build
	@./test.sh

lab2_list.csv: tests
lab2_add.csv: tests

graphs: lab2_list.csv lab2_add.csv
	@./lab2_add.gp
	@./lab2_list.gp

dist: graphs
	@tar -czf lab2a-UID.tar.gz lab2_add.c SortedList.h SortedList.c lab2_list.c \
	Makefile lab2_add.csv lab2_list.csv lab2_add-*.png lab2_list-*.png \
	lab2_add.gp lab2_list.gp test.sh README
