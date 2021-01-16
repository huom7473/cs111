/* NAME: Michael Huo */
/* EMAIL: EMAIL */
/* ID: UID */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <string.h>

#define CLOCK CLOCK_MONOTONIC

typedef struct {
    long long* ptr;
    long long iters;
    char sync;
} arg_t;

int opt_yield;
void add(long long *pointer, long long value) {
    long long sum = *pointer + value;
    if (opt_yield)
        sched_yield();
    *pointer = sum;
}

static pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
void add_mutex(long long *pointer, long long value) {
    pthread_mutex_lock(&mutex_lock); //block until lock can be obtained
    long long sum = *pointer + value;
    if (opt_yield)
        sched_yield();
    *pointer = sum;
    pthread_mutex_unlock(&mutex_lock);
}

static int spin_lock;
void add_spin(long long *pointer, long long value) {
    while (__sync_lock_test_and_set(&spin_lock, 1))
        ; //spin
    long long sum = *pointer + value;
    if (opt_yield)
        sched_yield();
    *pointer = sum;
    __sync_lock_release(&spin_lock);
}

void add_compare(long long *pointer, long long value) {
    long long oldval, newval;
    do {
        oldval = *pointer;
        newval = oldval + value;
        if (opt_yield)
            sched_yield();
    } while (__sync_val_compare_and_swap(pointer, oldval, newval) != oldval);
    //keep trying to update until it's successful
    //we will know the new value is wrong if the value returned by the atomic function is different
    //from oldval, which we calculated newval from
}

void *thread_add(void *args) {
    arg_t *data = (arg_t*)args;
    void (*add_func)(long long*, long long);

    switch (data->sync) {
        case 'm':
            add_func = add_mutex;
            break;
        case 's':
            add_func = add_spin;
            break;
        case 'c':
            add_func = add_compare;
            break;
        default:
            add_func = add;
    }

    for (int i = 0; i < data->iters; ++i) {
        add_func(data->ptr, 1);
    }
    for (int i = 0; i < data->iters; ++i) {
        add_func(data->ptr, -1);
    }
    return NULL;
}

int main(int argc, char **argv) {
    int threadc = 1, iterationc = 1;
    char *t_endptr = "\0", *i_endptr = "\0";
    char *sync = "none";

    opt_yield = 0; //false by default
    static struct option long_options[] = {
            {"threads", required_argument, 0, 't'},
            {"iterations", required_argument, 0, 'i'},
            {"yield", no_argument, 0, 'y'},
            {"sync", required_argument, 0, 's'},
            {0, 0, 0,0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "", long_options, 0)) != -1) {
        switch(c) {
            case 't':
                threadc = strtol(optarg, &t_endptr, 10);
                break;
            case 'i':
                iterationc = strtol(optarg, &i_endptr, 10);
                break;
            case 'y':
                opt_yield = 1;
                break;
            case 's':
                sync = optarg;
                break;
            default:
                fprintf(stderr, "usage: %s [--threads=<num>] [--iterations=<num>] [--yield] "
                                "[--sync=m|s|c]\n", argv[0]);
                exit(1);
        }
    }

    //check for invalid thread/iteration # or string parsing error in strtol calls
    if(threadc < 0 || iterationc < 0 || *t_endptr || *i_endptr) {
        fprintf(stderr, "%s: must specify positive integer (or nothing) for threads and iterations\n", argv[0]);
        exit(1);
    }

    if(strcmp(sync, "none") && strcmp(sync, "m") && strcmp(sync, "s") && strcmp(sync, "c")) {
        fprintf(stderr, "%s: must specify sync option [m]utex, [s]pin, [c]ompare/swap (or nothing)\n", argv[0]);
        exit(1);
    }
    else if(sync[0] == 's') { //spin, make it available
        spin_lock = 0;
    }

    long long counter = 0;
    pthread_t *threads = (pthread_t*)malloc(threadc * sizeof(pthread_t));
    arg_t args = {&counter, iterationc, sync[0]}; //first character of sync "string"
    struct timespec ts_start, ts_end;

    if(clock_gettime(CLOCK, &ts_start) == -1) {
        fprintf(stderr, "%s: error getting clock start time\n", argv[0]);
        exit(1);
    }

    for(int i = 0; i < threadc; ++i) {
        if(pthread_create(&threads[i], NULL, thread_add, (void *) &args)) {
            fprintf(stderr, "%s: error in pthread_create\n", argv[0]);
            exit(2);
        }
    }

    for(int i = 0; i < threadc; ++i) {
        if(pthread_join(threads[i], NULL)) {
            fprintf(stderr, "%s: error in pthread_join\n", argv[0]);
            exit(2);
        }
    }

    if(clock_gettime(CLOCK, &ts_end) == -1) {
        fprintf(stderr, "%s: error getting clock end time\n", argv[0]);
        exit(1);
    }
    unsigned long long time_taken = (ts_end.tv_sec - ts_start.tv_sec) * 1e9 + (ts_end.tv_nsec - ts_start.tv_nsec);
    unsigned opcount = threadc * iterationc * 2;

    const char* test_name = "add";
    const char* yield = opt_yield ? "-yield" : "";

    printf("%s%s-%s,%u,%u,%u,%llu,%llu,%lld\n",
           test_name, yield, sync, threadc, iterationc, opcount, time_taken, time_taken/opcount, counter);

    exit(0);
}