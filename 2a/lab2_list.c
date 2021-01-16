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
    SortedList_t *list;
    SortedListElement_t *nodepool;
    long long iters;
    int threadnum; //so we know which nodes belong to us in nodepool
    char sync;
} arg_t;

int opt_yield;

void *thread_list(void *args) { //unprotected version
    arg_t *data = (arg_t*)args;
    SortedListElement_t *node;
    SortedListElement_t *my_nodepool = &(data->nodepool[data->threadnum * data->iters]);
    //know where to start for this thread

    for (int i = 0; i < data->iters; ++i) { //insertions
        //node = &(data->nodepool[i]);
        //printf("%0xx%x%x%x\n", node->key[0], node->key[1], node->key[2], node->key[3]);
        SortedList_insert(data->list, &my_nodepool[i]);
    }

    int length = SortedList_length(data->list);
    //length checking - the length better be positive (so no error) and all the nodes we added should be there
    if(length < data->iters) {
        fprintf(stderr,"thread %d: discovered list corruption while checking for length\n", data->threadnum);
        pthread_exit((void*)1);
    }
    //printf("length: %d\n", SortedList_length(data->list));

    for (int i = 0; i < data->iters; ++i) { //deletions
        node = SortedList_lookup(data->list, my_nodepool[i].key);
        if(!node) {//we inserted this node, so we should be able to find it...
            fprintf(stderr,"thread %d: couldn't find a node that was previously inserted\n", data->threadnum);
            pthread_exit((void*)1);
        }
        if(SortedList_delete(node)) {
            fprintf(stderr, "thread %d: discovered list corruption while deleting node\n", data->threadnum);
            pthread_exit((void*)1);
        }
    }
    pthread_exit(0);
}

static int spin_lock;
void *thread_list_s(void *args) {
    arg_t *data = (arg_t*)args;
    SortedListElement_t *node;
    SortedListElement_t *my_nodepool = &(data->nodepool[data->threadnum * data->iters]);
    //know where to start for this thread

    for (int i = 0; i < data->iters; ++i) { //insertions
        while (__sync_lock_test_and_set(&spin_lock, 1))
            ; //spin until lock is obtained
        //node = &(data->nodepool[i]);
        //printf("%0xx%x%x%x\n", node->key[0], node->key[1], node->key[2], node->key[3]);
        SortedList_insert(data->list, &my_nodepool[i]);
        __sync_lock_release(&spin_lock);
    }

    while (__sync_lock_test_and_set(&spin_lock, 1))
        ; //spin until lock is obtained
    if(SortedList_length(data->list) == -1) { //length checking
        fprintf(stderr,"thread %d: discovered list corruption while checking for length\n", data->threadnum);
        pthread_exit((void*)1);
    }
    __sync_lock_release(&spin_lock);
    //printf("length: %d\n", SortedList_length(data->list));

    for (int i = 0; i < data->iters; ++i) { //deletions
        while (__sync_lock_test_and_set(&spin_lock, 1))
            ; //spin until lock is obtained
        node = SortedList_lookup(data->list, my_nodepool[i].key);
        if(!node) {//we inserted this node, so we should be able to find it...
            fprintf(stderr,"thread %d: couldn't find a node that was previously inserted\n", data->threadnum);
            pthread_exit((void*)1);
        }
        if(SortedList_delete(node)) {
            fprintf(stderr, "thread %d: discovered list corruption while deleting node\n", data->threadnum);
            pthread_exit((void*)1);
        }
        __sync_lock_release(&spin_lock);
    }
    pthread_exit(0);
}

static pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
void *thread_list_m(void *args) {
    arg_t *data = (arg_t*)args;
    SortedListElement_t *node;
    SortedListElement_t *my_nodepool = &(data->nodepool[data->threadnum * data->iters]);
    //know where to start for this thread

    for (int i = 0; i < data->iters; ++i) { //insertions
        pthread_mutex_lock(&mutex_lock); //block until lock can be obtained
        //node = &(data->nodepool[i]);
        //printf("%0xx%x%x%x\n", node->key[0], node->key[1], node->key[2], node->key[3]);
        SortedList_insert(data->list, &my_nodepool[i]);
        pthread_mutex_unlock(&mutex_lock);
    }
    pthread_mutex_lock(&mutex_lock); //block until lock can be obtained
    if(SortedList_length(data->list) == -1) { //length checking
        fprintf(stderr,"thread %d: discovered list corruption while checking for length\n", data->threadnum);
        pthread_exit((void*)1);
    }
    pthread_mutex_unlock(&mutex_lock);
    //printf("length: %d\n", SortedList_length(data->list));

    for (int i = 0; i < data->iters; ++i) { //deletions
        pthread_mutex_lock(&mutex_lock); //block until lock can be obtained
        node = SortedList_lookup(data->list, my_nodepool[i].key);
        if(!node) {//we inserted this node, so we should be able to find it...
            fprintf(stderr,"thread %d: couldn't find a node that was previously inserted\n", data->threadnum);
            pthread_exit((void*)1);
        }
        if(SortedList_delete(node)) {
            fprintf(stderr, "thread %d: discovered list corruption while deleting node\n", data->threadnum);
            pthread_exit((void*)1);
        }
        pthread_mutex_unlock(&mutex_lock);
    }

    pthread_exit(0);
}

void handle_segfault() {
    fprintf(stderr, "Caught segfault\n");
    exit(2);
}

int main(int argc, char **argv) {
    int threadc = 1, iterationc = 1;
    char *t_endptr = "\0", *i_endptr = "\0";
    char *sync = "none";
    char *yield_opts = "";
    opt_yield = 0; //false by default
    static struct option long_options[] = {
            {"threads", required_argument, 0, 't'},
            {"iterations", required_argument, 0, 'i'},
            {"yield", required_argument, 0, 'y'},
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
                yield_opts = optarg; //set opt_yield accordingly later
                break;
            case 's':
                sync = optarg;
                break;
            default:
                fprintf(stderr, "usage: %s [--threads=<num>] [--iterations=<num>] [--yield=[idl]]"
                                "[--sync=s|m]\n", argv[0]);
                exit(1);
        }
    }

    //check for invalid thread/iteration # or string parsing error in strtol calls
    if(threadc < 0 || iterationc < 0 || *t_endptr || *i_endptr) {
        fprintf(stderr, "%s: must specify positive integer (or nothing) for threads and iterations\n", argv[0]);
        exit(1);
    }

    if(strcmp(sync, "none") && strcmp(sync, "m") && strcmp(sync, "s")) {
        fprintf(stderr, "%s: must specify sync option [m]utex, [s]pin (or nothing)\n", argv[0]);
        exit(1);
    }
    else if(sync[0] == 's') { //initialize spin lock to be available/usable
        spin_lock = 0;
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
    SortedList_t list; //initialize head of circular list
    list.prev = &list;
    list.next = &list;
    list.key = NULL;

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
        arg_array[i] = (arg_t) {&list, nodepool, iterationc, i, sync[0]};
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
        int status;
        pthread_join(threads[i], (void**)&status);
        if(status) {
            fprintf(stderr, "%s: list inconsistency discovered - thread exited with error status\n", argv[0]);
            exit(2);
        }
    }

    if(clock_gettime(CLOCK, &ts_end) == -1) {
        if(fprintf(stderr, "%s: error getting clock end time\n", argv[0])) {
            fprintf(stderr, "%s: error in pthread_join\n", argv[0]);
            exit(1);
        }
    }

    if(SortedList_length(&list)) { // if there's stuff in the list still
        fprintf(stderr, "%s: list inconsistency discovered - length not 0\n", argv[0]);
        exit(2);
    }

    unsigned long long time_taken = (ts_end.tv_sec - ts_start.tv_sec) * 1e9 + (ts_end.tv_nsec - ts_start.tv_nsec);
    unsigned opcount = threadc * iterationc * 3;

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

    printf("%s-%s-%s,%u,%u,1,%u,%llu,%llu\n",
           test_name, yieldopts, sync, threadc, iterationc, opcount, time_taken, time_taken/opcount);

    exit(0);
}