#!/bin/bash
# NAME: Michael Huo
# EMAIL: EMAIL
# ID: UID
# See README for more information about these tests

rm -f lab2_add.csv lab2_list.csv #appending data, so remove these first

for t in 1 2 4 8 12 #lab2_add-1, 2, 3, 4, 5 tests (may generate extra/overlapping data) 
do
    for i in 100 1000 10000 100000
    do
	for s in m s c # 4
	do
	    ./lab2_add --iterations $i --threads $t --sync $s >> lab2_add.csv
	    ./lab2_add --iterations $i --threads $t --sync $s --yield >> lab2_add.csv
	done
    done
    for i in 10 20 40 80 100 1000 10000 100000 # 2
    do
	./lab2_add --iterations $i --threads $t >> lab2_add.csv # 1, 3
	./lab2_add --iterations $i --threads $t --yield >> lab2_add.csv # 2
    done
done

# start lab2_list tests

for i in 10 100 1000 10000 20000 # 1
do
    ./lab2_list --iterations $i >> lab2_list.csv 2> /dev/null
    # discard stderr messages, since we're expecting them and don't want to spam the terminal
done

for t in 2 4 8 12
do
    for i in 1 10 100 1000 # 2, part 1
    do
	./lab2_list --iterations $i --threads $t >> lab2_list.csv 2> /dev/null
    done
    for i in 1 2 4 8 16 32
    do
	for y in i d il dl
	do
	    ./lab2_list --iterations $i --threads $t --yield $y >> lab2_list.csv 2> /dev/null # 2, part 2
	    for s in m s # 3
	    do
		./lab2_list --iterations $i --threads $t --yield $y --sync $s >> lab2_list.csv 2> /dev/null
	    done	   
	done
    done
done

for t in 1 2 4 8 12 16 24 # 4
do
    for s in m s
    do
	./lab2_list --iterations 1000 --threads $t --sync $s >> lab2_list.csv 2> /dev/null
    done
done
