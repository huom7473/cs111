# NAME: Michael Huo
# EMAIL: EMAIL
# ID: UID

CC=gcc
CFLAGS=-Wall -Wextra -g
LINK=-pthread -lprofiler # linking with lprofile without --no-as-needed doesn't do anything...

.PHONY: default clean tests graphs dist profile

default: lab2_list

lab2_list: lab2_list.c SortedList.c SortedList.h
	@$(CC) -o lab2_list $(CFLAGS) lab2_list.c SortedList.c $(LINK)

clean:
	@rm -f lab2_list lab2b-UID.tar.gz

tests: lab2_list
	@./test.sh

lab2b_list.csv: tests

graphs: lab2b_list.csv
	@./lab2b.gp

profile: lab2_list
	@env LD_PRELOAD=/usr/local/lib/libprofiler.so CPUPROFILE=raw.gperf ./lab2_list --threads 12 --iterations 1000 --sync s >/dev/null
	@pprof --text ./lab2_list ./raw.gperf 2> /dev/null 1> profile.out # not really interested in stderr output of pprof
	@pprof --list=thread_list_s ./lab2_list ./raw.gperf 2> /dev/null 1>> profile.out

dist: graphs
	@tar -czf lab2b-UID.tar.gz SortedList.h SortedList.c lab2_list.c \
	Makefile lab2b_list.csv profile.out lab2b_*.png \
	lab2b.gp test.sh README
