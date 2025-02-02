#include "../h/pcb.h"
#include "../h/const.h"
#include "../h/types.h"

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
    /* 
    This method is used to initialize a variable to be tail pointer to a process queue.
    Return a pointer to the tail of an empty process queue; i.e. NULL. 
     
    This method is used to initialize a variable to be the tail pointer of a process queue. 
    If the queue is empty, the tail pointer is NULL.
    */
    return NULL;
}

extern int emptyProcQ(pcb_t *tp) {
/* Return TRUE if the queue whose tail is pointed to by tp is empty.
Return FALSE otherwise. */
    return (tp == NULL) ? TRUE : FALSE;
}
extern void insertProcQ(pcb_t **tp, pcb_t *p) {
/* Insert the pcb pointed to by p into the process queue whose tailpointer is pointed to by tp. Note the double indirection through tp
to allow for the possible updating of the tail pointer as well. */
    /* If the queue is empty, initialize it with p as the sole element. */
    if (*tp == NULL) {
        p->p_next = p;  /* circular list: next points to itself */
        p->p_prev = p;  /* circular list: prev points to itself */
        *tp = p;        /* p becomes the tail */
    } else {
        /* The queue is non-empty. Let "tail" = *tp. */

        /* Link p into the circular list:
         * p->p_next should be old_tail->p_next (the head),
         * p->p_prev should be old_tail,
         * fix the old head’s p_prev to point to p,
         * fix the old tail’s p_next to point to p,
         * then update the tail pointer to p.
         */
        pcb_t *oldTail = *tp;
        pcb_t *head = oldTail->p_next;  /* first (head) element */

        p->p_next = head;
        p->p_prev = oldTail;
        head->p_prev = p;
        oldTail->p_next = p;

        /* p is the new tail */
        *tp = p;
    }
}
extern pcb_t *removeProcQ(pcb_t **tp) {
/* Remove the first (i.e. head) element from the process queue whose
tail-pointer is pointed to by tp. Return NULL if the process queue
was initially empty; otherwise return the pointer to the removed element. Update the process queue’s tail pointer if necessary. */
/* If the queue is empty, return NULL immediately. */
    if (*tp == NULL) {
        return NULL;
    }

    /* The head is the element right after the tail. */
    pcb_t *tail = *tp;
    pcb_t *head = tail->p_next;

    /* If there is only one element in the queue (head == tail) */
    if (head == tail) {
        *tp = NULL;   /* the queue becomes empty */
    } else {
        /*
         * More than one element. Remove "head" by:
         * 1. Setting tail->p_next to head->p_next.
         * 2. Setting head->p_next->p_prev to tail.
         */
        tail->p_next = head->p_next;
        head->p_next->p_prev = tail;
    }

    /* Clear pointers in the removed PCB (not strictly required, but a good practice). */
    head->p_next = NULL;
    head->p_prev = NULL;

    /* Return the pointer to the removed (head) PCB. */
    return head;
}
extern pcb_t *outProcQ(pcb_t **tp, pcb_t *p) {
    /* Remove the pcb pointed to by p from the process queue whose tailpointer is pointed to by tp. Update the process queue’s tail pointer if
necessary. If the desired entry is not in the indicated queue (an error
condition), return NULL; otherwise, return p. Note that p can point
to any element of the process queue. */
    if (*tp == NULL || p == NULL) {
        return NULL; /* empty queue or invalid pcb pointer */
    }

    /* We must verify that p is actually in the queue. Let's search. */
    pcb_t *tail = *tp;
    pcb_t *curr = tail;
    int found = FALSE;

    /* Traverse the circular list starting at tail. */
    do {
        if (curr == p) {
            found = TRUE;
            break;
        }
        curr = curr->p_next;
    } while (curr != tail);

    if (!found) {
        /* p is not in this queue */
        return NULL;
    }

    /* At this point, curr == p, meaning p is in the queue. */
    /* If p is the only element in the queue. */
    if (p->p_next == p && p->p_prev == p) {
        *tp = NULL;  /* queue becomes empty */
    } else {
        /* Remove p by linking its neighbors directly. */
        p->p_prev->p_next = p->p_next;
        p->p_next->p_prev = p->p_prev;

        /* If p was the tail, move the tail pointer */
        if (p == *tp) {
            *tp = p->p_prev;
        }
    }

    /* Clear p’s next/prev pointers. */
    p->p_next = NULL;
    p->p_prev = NULL;

    return p;
}
extern pcb_t *headProcQ(pcb_t *tp) {
/* Return a pointer to the first pcb from the process queue whose tail
is pointed to by tp. Do not remove this pcbfrom the process queue.
Return NULL if the process queue is empty. */
    if (tp == NULL) {
        return NULL;  /* empty queue => no head */
    }
    /* the "head" of a circular queue is tail->p_next */
    return tp->p_next;
}
/**
 * Check if the process control block (pcb) has no children.
 *
 * @param p Pointer to the pcb.
 * @return 1 if the pcb has no children, 0 otherwise.
 */
extern int emptyChild(pcb_PTR p) {
    /* Placeholder implementation */
    return 1;
}

/**
 * Insert a pcb as a child of another pcb.
 *
 * @param prnt Pointer to the parent pcb.
 * @param p Pointer to the child pcb to be inserted.
 */
extern void insertChild(pcb_PTR prnt, pcb_PTR p) {
    /* Placeholder implementation */
}

/**
 * Remove the first child of the given pcb.
 *
 * @param p Pointer to the parent pcb.
 * @return Pointer to the removed child pcb, or NULL if no children exist.
 */
extern pcb_PTR removeChild(pcb_PTR p) {
    /* Placeholder implementation */
    return NULL;
}

/**
 * Remove a specific child pcb from its parent.
 *
 * @param p Pointer to the child pcb to be removed.
 * @return Pointer to the removed child pcb, or NULL if it was not found.
 */
extern pcb_PTR outChild(pcb_PTR p) {
    /* Placeholder implementation */
    return NULL;
}