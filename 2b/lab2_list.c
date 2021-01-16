/* NAME: Michael Huo */
/* EMAIL: EMAIL */
/* ID: UID */

#include "SortedList.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#define CLOCK CLOCK_MONOTONIC

typedef struct {
    SortedList_t *lists;
    int listc;
    SortedListElement_t *nodepool;
    long long iters;
    int threadnum; //so we know which nodes belong to us in nodepool
    char sync;
} arg_t;

int opt_yield;

void *thread_list(void *args) { //unprotected version
    arg_t *data = (arg_t*)args;
    int sublist; //sublist to add a particular node to
    int length, total_length = 0; //sublist length target, total length
    long long iterc = data->iters;
    int listc = data->listc;
    SortedListElement_t *node;
    SortedListElement_t *my_nodepool = &(data->nodepool[data->threadnum * iterc]);
    //know where to start for this thread

    for (int i = 0; i < iterc; ++i) { //insertions
        sublist = my_nodepool[i].key[0] % listc; //modulo hash to determine sublist. use first character of key
        SortedList_insert(&data->lists[sublist], &my_nodepool[i]);
    }

    for (int i = 0; i < listc; ++i) { //check length of all sublists
        length = SortedList_length(&data->lists[i]);
        if(length == -1) {
            fprintf(stderr,"thread %d: discovered list corruption while checking for length\n", data->threadnum);
            pthread_exit((void*)-1);
        }
        total_length += length;
        //printf("length: %d\n", SortedList_length(&data->lists[i]));
    }

    if(total_length < iterc) {
        fprintf(stderr,"thread %d: discovered list corruption while checking for length\n", data->threadnum);
        pthread_exit((void*)-1);
    }

    for (int i = 0; i < iterc; ++i) { //deletions
        sublist = my_nodepool[i].key[0] % listc;
        node = SortedList_lookup(&data->lists[sublist], my_nodepool[i].key);
        if(!node) {//we inserted this node, so we should be able to find it...
            fprintf(stderr,"thread %d: couldn't find a node that was previously inserted\n", data->threadnum);
            pthread_exit((void*)-1);
        }
        if(SortedList_delete(node)) {
            fprintf(stderr, "thread %d: discovered list corruption while deleting node\n", data->threadnum);
            pthread_exit((void*)-1);
        }
    }
    pthread_exit(0);
}

static int *spin_locks;
void *thread_list_s(void *args) {
    unsigned long long wait_time = 0;
    struct timespec ts_start, ts_end; //for lock wait timing
    int clock_errflag = 0; //or with return values, if any error it will be -1

    arg_t *data = (arg_t*)args;
    int sublist; //sublist to add a particular node to
    int length, total_length = 0; //sublist length target, total length
    long long iterc = data->iters;
    int listc = data->listc;
    SortedListElement_t *node;
    SortedListElement_t *my_nodepool = &(data->nodepool[data->threadnum * iterc]);
    //know where to start for this thread

    for (int i = 0; i < iterc; ++i) { //insertions
        sublist = my_nodepool[i].key[0] % listc; //modulo hash to determine sublist. use first character of key
        clock_errflag |= clock_gettime(CLOCK, &ts_start);
        while (__sync_lock_test_and_set(&spin_locks[sublist], 1))
            ; //spin until lock is obtained
        clock_errflag |= clock_gettime(CLOCK, &ts_end);
        SortedList_insert(&data->lists[sublist], &my_nodepool[i]);
        __sync_lock_release(&spin_locks[sublist]);
        wait_time += (ts_end.tv_sec - ts_start.tv_sec) * 1000000000 + (ts_end.tv_nsec - ts_start.tv_nsec);
    }

    for (int i = 0; i < listc; ++i) { //length checking - iterate through all lists
        clock_errflag |= clock_gettime(CLOCK, &ts_start);
        while (__sync_lock_test_and_set(&spin_locks[i], 1)); //spin until lock is obtained
        clock_errflag |= clock_gettime(CLOCK, &ts_end);
        length = SortedList_length(&data->lists[i]);
        if (length == -1) {
            fprintf(stderr, "thread %d: discovered list corruption while checking for length\n", data->threadnum);
            pthread_exit((void *)-1);
        }
        __sync_lock_release(&spin_locks[i]);
        total_length += length;
        wait_time += (ts_end.tv_sec - ts_start.tv_sec) * 1000000000 + (ts_end.tv_nsec - ts_start.tv_nsec);
    }

    if(total_length < iterc) {
        fprintf(stderr,"thread %d: discovered list corruption while checking for length\n", data->threadnum);
        pthread_exit((void*)-1);
    }

    for (int i = 0; i < iterc; ++i) { //deletions
        sublist = my_nodepool[i].key[0] % listc; //modulo hash to determine sublist. use first character of key
        clock_errflag |= clock_gettime(CLOCK, &ts_start);
        while (__sync_lock_test_and_set(&spin_locks[sublist], 1))
            ; //spin until lock is obtained
        clock_errflag |= clock_gettime(CLOCK, &ts_end);
        node = SortedList_lookup(&data->lists[sublist], my_nodepool[i].key);
        if(!node) {//we inserted this node, so we should be able to find it...
            fprintf(stderr,"thread %d: couldn't find a node that was previously inserted\n", data->threadnum);
            pthread_exit((void*)-1);
        }
        if(SortedList_delete(node)) {
            fprintf(stderr, "thread %d: discovered list corruption while deleting node\n", data->threadnum);
            pthread_exit((void*)-1);
        }
        __sync_lock_release(&spin_locks[sublist]);
        wait_time += (ts_end.tv_sec - ts_start.tv_sec) * 1000000000 + (ts_end.tv_nsec - ts_start.tv_nsec);
    }

    if (clock_errflag == -1) //some time get failed
        pthread_exit((void*)-1);

    pthread_exit((void*)wait_time);
}

static pthread_mutex_t *mutex_locks;
void *thread_list_m(void *args) {
    unsigned long long wait_time = 0;
    struct timespec ts_start, ts_end; //for lock wait timing
    int clock_errflag = 0; //or with return values, if any error it will be -1

    arg_t *data = (arg_t*)args;
    int sublist; //sublist to add a particular node to
    int length, total_length = 0; //sublist length target, total length
    long long iterc = data->iters;
    int listc = data->listc;
    SortedListElement_t *node;
    SortedListElement_t *my_nodepool = &(data->nodepool[data->threadnum * iterc]);
    //know where to start for this thread

    for (int i = 0; i < data->iters; ++i) { //insertions
        sublist = my_nodepool[i].key[0] % listc; //modulo hash to determine sublist. use first character of key
        clock_errflag |= clock_gettime(CLOCK, &ts_start);
        //reduce measurement artifact and just check one time at the end rather than comparing every time
        pthread_mutex_lock(&mutex_locks[sublist]); //block until lock can be obtained
        clock_errflag |= clock_gettime(CLOCK, &ts_end);
        SortedList_insert(&data->lists[sublist], &my_nodepool[i]);
        pthread_mutex_unlock(&mutex_locks[sublist]);
        wait_time += (ts_end.tv_sec - ts_start.tv_sec) * 1000000000 + (ts_end.tv_nsec - ts_start.tv_nsec);
        //increment after lock is unlocked, possibly reducing measurement artifact
    }

    for (int i = 0; i < listc; ++i) { //length checking - iterate through all lists
        clock_errflag |= clock_gettime(CLOCK, &ts_start);
        pthread_mutex_lock(&mutex_locks[i]); //block until lock can be obtained
        clock_errflag |= clock_gettime(CLOCK, &ts_end);
        length = SortedList_length(&data->lists[i]);
        if (length == -1) {
            fprintf(stderr, "thread %d: discovered list corruption while checking for length\n", data->threadnum);
            pthread_exit((void *)-1);
        }
        pthread_mutex_unlock(&mutex_locks[i]);
        total_length += length;
        wait_time += (ts_end.tv_sec - ts_start.tv_sec) * 1000000000 + (ts_end.tv_nsec - ts_start.tv_nsec);
        //printf("length: %d\n", SortedList_length(&data->lists[i]));
    }

    if(total_length < iterc) {
        fprintf(stderr,"thread %d: discovered list corruption while checking for length\n", data->threadnum);
        pthread_exit((void*)-1);
    }

    for (int i = 0; i < data->iters; ++i) { //deletions
        sublist = my_nodepool[i].key[0] % listc; //modulo hash to determine sublist. use first character of key
        clock_errflag |= clock_gettime(CLOCK, &ts_start);
        pthread_mutex_lock(&mutex_locks[sublist]); //block until lock can be obtained
        clock_errflag |= clock_gettime(CLOCK, &ts_end);
        node = SortedList_lookup(&data->lists[sublist], my_nodepool[i].key);
        if(!node) {//we inserted this node, so we should be able to find it...
            fprintf(stderr,"thread %d: couldn't find a node that was previously inserted\n", data->threadnum);
            pthread_exit((void*)-1);
        }
        if(SortedList_delete(node)) {
            fprintf(stderr, "thread %d: discovered list corruption while deleting node\n", data->threadnum);
            pthread_exit((void*)-1);
        }
        pthread_mutex_unlock(&mutex_locks[sublist]);
        wait_time += (ts_end.tv_sec - ts_start.tv_sec) * 1000000000 + (ts_end.tv_nsec - ts_start.tv_nsec);
    }

    if (clock_errflag == -1) //some time get failed
        pthread_exit((void*)-1);

    pthread_exit((void*)wait_time);
}

void handle_segfault() {
    fprintf(stderr, "Caught segfault\n");
    exit(2);
}

int main(int argc, char **argv) {
    int threadc = 1, iterationc = 1, listc = 1;
    char *t_endptr = "\0", *i_endptr = "\0", *l_endptr = "\0";
    char *sync = "none";
    char *yield_opts = "";
    opt_yield = 0; //false by default
    static struct option long_options[] = {
            {"threads", required_argument, 0, 't'},
            {"iterations", required_argument, 0, 'i'},
            {"yield", required_argument, 0, 'y'},
            {"sync", required_argument, 0, 's'},
            {"lists", required_argument, 0, 'l'},
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
            case 'l':
                listc = strtol(optarg, &l_endptr, 10);
                break;
            case 'y':
                yield_opts = optarg; //set opt_yield accordingly later
                break;
            case 's':
                sync = optarg;
                break;
            default:
                fprintf(stderr, "usage: %s [--threads=<num>] [--iterations=<num>] [--yield=[idl]]"
                                "[--sync=s|m] [--lists=<num>]\n", argv[0]);
                exit(1);
        }
    }

    //check for invalid thread/iteration # or string parsing error in strtol calls
    if(threadc < 0 || iterationc < 0 || listc < 1 || *t_endptr || *i_endptr || *l_endptr) {
        fprintf(stderr, "%s: must specify positive integer (or nothing) "
                        "for threads, iterations, and lists\n", argv[0]);
        exit(1);
    }

    if(strcmp(sync, "none") && strcmp(sync, "m") && strcmp(sync, "s")) {
        fprintf(stderr, "%s: must specify sync option [m]utex, [s]pin (or nothing)\n", argv[0]);
        exit(1);
    }
    else if(sync[0] == 's') { //initialize spin locks to be available/usable
        spin_locks = calloc(listc, sizeof(int));
        if(!spin_locks) {
            fprintf(stderr, "%s: error in allocation of spin lock array\n", argv[0]);
            exit(2);
        }
    }
    else if(sync[0] == 'm') { //initialize mutex locks
        mutex_locks = malloc(listc * sizeof(pthread_mutex_t));
        if(!mutex_locks) {
            fprintf(stderr, "%s: error in allocation of mutex lock array\n", argv[0]);
            exit(2);
        }

        for(int i = 0; i < listc; ++i) {
            if(pthread_mutex_init(&mutex_locks[i], NULL)){
                fprintf(stderr, "%s: error in initialization of mutex lock array\n", argv[0]);
                exit(2);
            }
        }
    }

    if(strlen(yield_opts) > 3) {
        fprintf(stderr, "%s: must specify valid yield option as a subset of [idl]\n", argv[0]);
        exit(1);
    }
    else {
        int len = strlen(yield_opts);
        for(int i = 0; i < len; ++i) {
            switch (yield_opts[i]) {
                case 'i':
                    opt_yield |= INSERT_YIELD;
                    break;
                case 'd':
                    opt_yield |= DELETE_YIELD;
                    break;
                case 'l':
                    opt_yield |= LOOKUP_YIELD;
                    break;
                default:
                    fprintf(stderr, "%s: must specify valid yield option as a subset of [idl]\n", argv[0]);
                    exit(1);
            }
        }
    }
    SortedList_t *lists = (SortedList_t*)malloc(sizeof(SortedList_t) * listc); //initialize head(s) of lists
    for(int i = 0; i < listc; ++i) {
        lists[i].prev = &lists[i];
        lists[i].next = &lists[i];
        lists[i].key = NULL;
    }

    srandom(time(NULL)); //seed random gen for node initialization
    long long nodecount = threadc * iterationc;
    SortedListElement_t *nodepool = (SortedListElement_t*)malloc(sizeof(SortedListElement_t) * nodecount);
    if (!nodepool) { //malloc fail
        fprintf(stderr, "%s: error during allocation of nodes\n", argv[0]);
        exit(1);
    }
    for (int i = 0; i < nodecount; ++i) {
        unsigned long r = (unsigned long)random();
        char *key = malloc(5); //4 byte node key with extra byte for 0
        //load bytes in one at a time
        key[0] = (r >> 24) & 0xFF;
        key[1] = (r >> 16) & 0xFF;
        key[2] = (r >> 8) & 0xFF;
        key[3] = r & 0xFF;
        key[4] = '\0';
        nodepool[i].key = key;
    }

    pthread_t *threads = (pthread_t*)malloc(threadc * sizeof(pthread_t));
    arg_t *arg_array = (arg_t*)malloc(threadc * sizeof(arg_t));

    for(int i = 0; i < threadc; ++i) { //initialize arguments before starting time (may be a negligible difference...)
        arg_array[i] = (arg_t) {lists, listc, nodepool, iterationc, i, sync[0]};
    }

    void* (*thread_func)(void*); //pick correct function to run
    switch (sync[0]) {
        case 'm':
            thread_func = thread_list_m;
            break;
        case 's':
            thread_func = thread_list_s;
            break;
        default:
            thread_func = thread_list;
    }

    signal(SIGSEGV, handle_segfault); //catch segfaults

    //will designate threadnum in thread loop
    struct timespec ts_start, ts_end;
    unsigned long long total_wait = 0;

    if(clock_gettime(CLOCK, &ts_start) == -1) {
        fprintf(stderr, "%s: error getting clock start time\n", argv[0]);
        exit(1);
    }

    for(int i = 0; i < threadc; ++i) {
        if(pthread_create(&threads[i], NULL, thread_func, (void *) &arg_array[i])) {
            fprintf(stderr, "%s: error in pthread_create\n", argv[0]);
            exit(1);
        }
    }

    for(int i = 0; i < threadc; ++i) {
        unsigned long long status;
        if(pthread_join(threads[i], (void**)&status))
            fprintf(stderr, "%s: error in pthread_join\n", argv[0]);
        if(status == (unsigned long long)-1) { //kind of a sketchy compare relying on unsigned cast, but it works
            fprintf(stderr, "%s: list inconsistency discovered - thread exited with error status\n", argv[0]);
            exit(2);
        }
        total_wait += (unsigned long long)status;
    }

    if(clock_gettime(CLOCK, &ts_end) == -1) {
        fprintf(stderr, "%s: error getting clock end time\n", argv[0]);
    }

    for(int i = 0; i < listc; ++i) {
        if (SortedList_length(&lists[i])) { // if there's stuff in the list still
            fprintf(stderr, "%s: list inconsistency discovered - length not 0\n", argv[0]);
            exit(2);
        }
    }
    unsigned long long time_taken = (ts_end.tv_sec - ts_start.tv_sec) * 1000000000 + (ts_end.tv_nsec - ts_start.tv_nsec);
    unsigned long long opcount = threadc * iterationc * 3;
    unsigned long long lockcount = (threadc * iterationc * 2) + (listc * threadc);

    char yieldopts[5] = {0}; //zero-initialize
    if (opt_yield) { //some yield ops were specified
        if (opt_yield & INSERT_YIELD)
            strcat(yieldopts, "i");
        if (opt_yield & DELETE_YIELD)
            strcat(yieldopts, "d");
        if(opt_yield & LOOKUP_YIELD)
            strcat(yieldopts, "l");
    }
    else
        strcpy(yieldopts, "none");

    const char* test_name = "list";

    printf("%s-%s-%s,%u,%u,%u,%llu,%llu,%llu,%llu\n",
           test_name, yieldopts, sync, threadc, iterationc, listc, opcount,
           time_taken, time_taken/opcount, total_wait/lockcount);
    //by the spec, this doesn't account for the length operation, but oh well... it's probably negligible

    exit(0);
}