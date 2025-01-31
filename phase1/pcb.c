#include "../h/pcb.h"
#include "../h/const.h"
#include "../h/types.h"
#include <studio.h>

HIDDEN pcb_t pcbFreeTable[MAXPROC];
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
     */
    /* Example of zeroing out if it's a simple struct: 
    p->p_s = (state_t){0};  */

    return p;
}

extern void initPcbs () {
/* Initialize the pcbFree list to contain all the elements of the
static array of MAXPROC pcbs. This method will be called only
once during data structure initialization. */
    pcbFree_h = NULL;

    for (int i=0; i < MAXPROC; i++) {
        pcbFreeTable[i].p_next = pcbFree_h;
        pcbFree_h = &pcbFreeTable[i];
    }
}
/*
extern pcb_PTR mkEmptyProcQ ();
extern int emptyProcQ (pcb_PTR tp);
extern void insertProcQ (pcb_PTR *tp, pcb_PTR p);
extern pcb_PTR removeProcQ (pcb_PTR *tp);
extern pcb_PTR outProcQ (pcb_PTR *tp, pcb_PTR p);
extern pcb_PTR headProcQ (pcb_PTR tp);

extern int emptyChild (pcb_PTR p);
extern void insertChild (pcb_PTR prnt, pcb_PTR p);
extern pcb_PTR removeChild (pcb_PTR p);
extern pcb_PTR outChild (pcb_PTR p);
*/