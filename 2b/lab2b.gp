#! /usr/bin/gnuplot
# NAME: Michael Huo
# EMAIL: EMAIL
# ID : UID
# 
# input: lab2_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#	8. wait-for-lock time per operation (ns)

# general plot parameters
set terminal png
set datafile separator ","

# throughput with different sync options
set title "1: Throughput vs threads with synchronization options"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput (operations/second)"
set logscale y 10
set output 'lab2b_1.png'

# grep out protected runs with 1000 iterations
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1e9)/($7) \
	title 'mutex' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1e9)/($7) \
	title 'spin' with linespoints lc rgb 'green'

set title "2: Wait-for-lock time and average time per operation"
set xlabel "Threads"
set logscale x 2
set ylabel "Time (ns)"
set logscale y 10
set output 'lab2b_2.png'

# grep out only mutex protected runs with 1000 iterations
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,4,' lab2b_list.csv" using ($2):($7) \
	title 'operation time' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-m,[0-9]*,1000,4,' lab2b_list.csv" using ($2):($8) \
	title 'wait-for-lock time' with linespoints lc rgb 'green'
	
set title "3: Successful runs with different synchronization options"
set xlabel "Threads"
set logscale x 2
set ylabel "Successful Iterations"
set logscale y 10
set output 'lab2b_3.png'

# grep out only yield=id runs with 1000 iterations
plot \
     "< grep -e 'list-id-none,' lab2b_list.csv" using ($2):($3) \
	title 'unprotected' with points lc rgb 'green', \
     "< grep -e 'list-id-m,' lab2b_list.csv" using ($2):($3) \
	title 'mutex' with points lc rgb 'red', \
     "< grep -e 'list-id-s,' lab2b_list.csv" using ($2):($3) \
	title 'spin' with points lc rgb 'blue'
	
set title "4: Mutex-protected throughput vs threads with sublists"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput (operations/second)"
set logscale y 10
set output 'lab2b_4.png'

# grep out only mutex runs with 1000 iterations
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1e9)/($7) \
	title '1 list' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-m,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1e9)/($7) \
	title '4 lists' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-m,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1e9)/($7) \
	title '8 lists' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-m,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1e9)/($7) \
	title '16 lists' with linespoints lc rgb 'violet', \
	
set title "5: Spinlock-protected throughput vs threads with sublists"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput (operations/second)"
set logscale y 10
set output 'lab2b_5.png'

# grep out only spin runs with 1000 iterations
plot \
     "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1e9)/($7) \
	title '1 list' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-s,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1e9)/($7) \
	title '4 lists' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-s,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1e9)/($7) \
	title '8 lists' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-s,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1e9)/($7) \
	title '16 lists' with linespoints lc rgb 'violet', \



