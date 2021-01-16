#!/bin/bash
# NAME: Michael Huo
# EMAIL: EMAIL
# ID: UID
# See README for more information about these tests

rm -f lab2b_list.csv  #appending data, so remove these first

for t in 1 2 4 8 12 16 24 # 1 2 4 5 - generates extra data
do
    for s in m s
    do
	for l in 1 4 8 16
	do
	    ./lab2_list --iterations 1000 --threads $t --sync $s --lists $l >> lab2b_list.csv 2> /dev/null
	done
    done
done

for t in 1 4 8 12 16 # 3
do
    for i in 1 2 4 8 16 32 64 # added extra iterations...
    do
	./lab2_list --iterations $i --threads $t --lists 4 --yield id >> lab2b_list.csv 2> /dev/null
    done
    for i in 10 20 40 80
    do
	for s in m s
	do
	    ./lab2_list --iterations $i --threads $t --lists 4 --sync $s --yield id>> lab2b_list.csv 2> /dev/null
	done
    done
done
