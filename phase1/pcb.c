#include "../h/pcb.h"
#include "../h/const.h"
#include "../h/types.h"
#include <stdio.h>

HIDDEN pcb_t *pcbFree_h = NULL;

extern void freePcb (pcb_t *p) {
/* Insert the element pointed to by p onto the pcbFree list. */
    if (p == NULL) return;

    /* Insert at the head of the free list */
    p->p_next = pcbFree_h;
    pcbFree_h = p;
}
extern pcb_t *allocPcb() {
/* Return NULL if the pcbFree list is empty. Otherwise, remove
an element from the pcbFree list, provide initial values for ALL
of the pcbs fields (i.e. NULL and/or 0) and then return a pointer
to the removed element. pcbs get reused, so it is important that
no previous value persist in a pcb when it gets reallocated. */
    /* If there are no free PCBs, return NULL */
    if (pcbFree_h == NULL) {
        return NULL;
    }

    /* Remove from the head of the free list */
    pcb_t *p = pcbFree_h;
    pcbFree_h = pcbFree_h->p_next;

    /* Reset all fields so no stale information persists */
    p->p_next = NULL;
    p->p_prev = NULL;
    p->p_prnt = NULL;
    p->p_child = NULL;
    p->p_sib = NULL;

    /* Reset CPU time, semaphore address, and support structure pointer */
    p->p_time = 0;
    p->p_semAdd = NULL;
    p->p_supportStruct = NULL;
 
    /*
     * Also reset the processor state structure (p_s).
     * The exact method depends on how state_t is defined in Pandos.
     * If state_t is a struct, you could do a memset here:
     *
     *    memset(&(p->p_s), 0, sizeof(state_t));
     *
     * or assign fields individually if needed.
    Example of zeroing out if it's a simple struct: 
    p->p_s = (state_t){0};  */

    return p;
}

extern void initPcbs () {
/* Initialize the pcbFree list to contain all the elements of the
static array of MAXPROC pcbs. This method will be called only
once during data structure initialization. */
    static pcb_t pcbFreeTable[MAXPROC];
    pcbFree_h = &pcbFreeTable[0];

    int i;
    for (i=0; i < MAXPROC-1; i++) {
        pcbFreeTable[i].p_next = &pcbFreeTable[i+1];
    }

    pcbFreeTable[MAXPROC-1].p_next = NULL;
}

extern pcb_t *mkEmptyProcQ() {
/* This method is used to initialize a variable to be tail pointer to a
process queue.
Return a pointer to the tail of an empty process queue; i.e. NULL. */
}

extern int emptyProcQ(pcb_t *tp) {
/* Return TRUE if the queue whose tail is pointed to by tp is empty.
Return FALSE otherwise. */
}
extern insertProcQ(pcb_t **tp, pcb_t *p) {
/* Insert the pcb pointed to by p into the process queue whose tailpointer is pointed to by tp. Note the double indirection through tp
to allow for the possible updating of the tail pointer as well. */
}
extern pcb_t *removeProcQ(pcb_t **tp) {
/* Remove the first (i.e. head) element from the process queue whose
tail-pointer is pointed to by tp. Return NULL if the process queue
was initially empty; otherwise return the pointer to the removed element. Update the process queue’s tail pointer if necessary. */
}
extern pcb_t *outProcQ(pcb_t **tp, pcb_t *p) {
    /* Remove the pcb pointed to by p from the process queue whose tailpointer is pointed to by tp. Update the process queue’s tail pointer if
necessary. If the desired entry is not in the indicated queue (an error
condition), return NULL; otherwise, return p. Note that p can point
to any element of the process queue. */
}
extern pcb_t *headProcQ(pcb_t *tp) {
/* Return a pointer to the first pcb from the process queue whose tail
is pointed to by tp. Do not remove this pcbfrom the process queue.
Return NULL if the process queue is empty. */
}