/* NAME: Michael Huo */
/* EMAIL: EMAIL */
/* ID: UID */

#include "SortedList.h"
#include <string.h>
#include <pthread.h>
//#include <stdio.h>

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
    SortedListElement_t *curr = list;
    //find correct place to insert the node. when this loop exits the element will go after curr
    //conditions:
    //the key of next is not a null pointer (came back to the head)
    //the key of the next node is strictly less than the element we're inserting
    while (curr->next->key && strcmp(curr->next->key, element->key) < 0) {
        curr = curr->next;
    }
    //printf("%0xx%x%x%x", element->key[0], element->key[1], element->key[2], element->key[3]);
    //assume circular implementation of list, and head will point to itself in an empty list
    //ok by piazza @362
    element->next = curr->next;
    element->prev = curr;

    curr->next->prev = element;

    //would be nonideal to yield when list is inconsistent
    if (opt_yield & INSERT_YIELD)
        sched_yield();

    curr->next = element;
}

int SortedList_delete(SortedListElement_t *element) {
    //check for null element to avoid segfault after trying to delete non-existent node
    if (!element || element->next->prev != element || element->prev->next != element)
        return 1;

    element->next->prev = element->prev;

    //yield here, in the middle of updates, when list is inconsistent
    //would be nonideal to yield right after finding where to insert the node
    if (opt_yield & DELETE_YIELD)
        sched_yield();

    element->prev->next = element->next;

    return 0;
}

SortedListElement_t *SortedList_lookup(SortedListElement_t *list, const char *key) {
    if (!list->next->key) //list is empty
        return NULL;

    SortedListElement_t *curr = list->next; //start search on first node
    //traverse list until we find a matching key
    while (strcmp(curr->key, key)) {
        curr = curr->next;

        if (opt_yield & LOOKUP_YIELD) //increases chance that next is not valid anymore - i.e. it gets deleted/modified
            sched_yield();

        if (!curr->key) //if we're back to the head of the list, i.e. the key is a null pointer
            return NULL;
    }

    return curr;
}

int SortedList_length(SortedList_t *list) {
    if(!list->next->key) //list is empty
        return 0;

    int count = 0;
    SortedListElement_t *curr = list->next;

    while (curr->key) {
        if (curr->next->prev != curr || curr->prev->next != curr)
            return -1;

        ++count;

        curr = curr->next;

        //increases chance that next is not valid anymore - i.e. it gets deleted/modified
        //or we start inserting a node after next (possibly yielding in the middle with an inconsistent list)
        if (opt_yield & LOOKUP_YIELD)
            sched_yield();

    }

    return count;
}