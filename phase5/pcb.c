/************************** PCB.C ******************************
*
*  The implementation file for the Process Control Block (PCB) 
*    Module.
*
*  This module manages the allocation, deallocation, and 
*    manipulation of PCBs for a system.
*
*  Written by Rosalie Lee, Luka Bagashvili
*/

#include "../h/pcb.h"
#include "../h/const.h"
#include "../h/types.h"


/* head of the free PCB list */
HIDDEN pcb_PTR pcbFree_h = NULL;


/* freePcb inserts the PCB pointed to by p at the head of the free PCB list.
 * Input: p - Pointer to a PCB.
 * Precondition: p is either NULL or points to a valid PCB structure. */
extern void freePcb(pcb_t *p) {
    /* If p is NULL, nothing to free. */
    if (p == NULL) return;

    p->p_next = pcbFree_h; 
    pcbFree_h = p;         
}


/* allocPcb removes a PCB from the head of the free list, resets its fields, and returns it.
 * Precondition: The free PCB list (pcbFree_h) has been initialized.
 * Return: Pointer to a PCB if available; otherwise, NULL. */
extern pcb_PTR allocPcb() {
    /* If the free list is empty, return NULL. */
    if (pcbFree_h == NULL) {
        return NULL;
    }

    /* Remove a PCB. */
    pcb_t *p = pcbFree_h;
    pcbFree_h = pcbFree_h->p_next;

    /* Reset all pointer fields to NULL. */
    p->p_next = NULL;
    p->p_prev = NULL;
    p->p_prnt = NULL;
    p->p_child = NULL;
    p->p_next_sib = NULL;

    /* Reset CPU time, semaphore address, support structure pointer. */
    p->p_time = 0;           
    p->p_semAdd = NULL;      
    p->p_supportStruct = NULL; 

    return p;
}


/* initPcbs initializes the free PCB list with a static array of PCBs.
 * Precondition: Called once during system initialization.
 * Modification: Uses freePcb() to link each PCB in the array.
 */
extern void initPcbs() {
    static pcb_t pcbFreeTable[MAXPROC];
    int i;

    /* Start with an empty free list. */
    pcbFree_h = NULL;

    /* Link each PCB in the array by calling freePcb(). */
    for (i = 0; i < MAXPROC; i++) {
        freePcb(&pcbFreeTable[i]);
    }
}


/* mkEmptyProcQ initializes and returns an empty process queue.
 * Precondition: None.
 * Return: NULL, representing an empty process queue by a NULL tail pointer. */
extern pcb_PTR mkEmptyProcQ() {
    return NULL;
}


/* emptyProcQ checks if the process queue is empty.
 * Input: tp - Pointer to the tail of a process queue.
 * Precondition: tp is either NULL (empty queue) or a valid tail pointer.
 * Return: TRUE if the queue is empty; otherwise, FALSE. */
extern int emptyProcQ(pcb_t *tp) {
    return tp == NULL;
}


/* insertProcQ inserts the PCB pointed to by p into the process queue.
 * Input: 
 *    tp - Pointer to the tail pointer of a process queue.
 *    p  - Pointer to a PCB to be inserted.
 * Precondition: If the queue is non-empty, *tp points to a valid tail in a circular queue. */
extern void insertProcQ(pcb_t **tp, pcb_t *p) {
    /* If the queue is empty, initialize circular list with a single element. */
    if (emptyProcQ(*tp)) {
        p->p_next = p; 
        p->p_prev = p;  
        *tp = p;        
    } else {
        /* Insert p between current tail (oldTail) and head. */
        pcb_t *oldTail = *tp;     
        pcb_t *head = oldTail->p_next; 

        p->p_next = head;         
        p->p_prev = oldTail;      
        head->p_prev = p;         
        oldTail->p_next = p;      

        *tp = p; 
    }
}


/* removeProcQ removes the head PCB from the process queue and returns it.
 * Input: tp - Pointer to the tail pointer of a process queue.
 * Precondition: *tp is either NULL or a valid tail pointer of a circular queue.
 * Return: Pointer to the removed head PCB; if the queue is empty, returns NULL. */
extern pcb_PTR removeProcQ(pcb_t **tp) {
    /* If the queue is empty, return NULL. */
    if (emptyProcQ(*tp)) {
        return NULL;
    }

    pcb_t *tail = *tp;         
    pcb_t *head = tail->p_next;  

    /* If there's only one element, the queue becomes empty. */
    if (head == tail) {
        *tp = NULL; 
    } else {
        /* More than one element: bypass the head. */
        tail->p_next = head->p_next;         
        head->p_next->p_prev = tail;           
    }

    /* Clear the removed PCB's pointers. */
    head->p_next = NULL;
    head->p_prev = NULL;
    return head;
}


/* outProcQ removes the specified PCB from the process queue.
 * Input:
 *    tp - Pointer to the tail pointer of a process queue.
 *    p  - Pointer to a PCB that should be removed.
 * Precondition: If the queue is non-empty, *tp points to a valid tail of a circular queue.
 * Return: Pointer to the PCB if removal is successful; otherwise, returns NULL.
 * Modification: If p is the head of the queue, use removeProcQ().
 */
extern pcb_t *outProcQ(pcb_t **tp, pcb_t *p) {
    if (emptyProcQ(*tp) || p == NULL) {
        return NULL;
    }

    pcb_PTR head = headProcQ(*tp);
    if (p == head) {
        return removeProcQ(tp);
    }

    pcb_t *curr = head->p_next;
    /* Loop until we either circle back to head or find p */
    while (curr != head && curr != p) {
        curr = curr->p_next;
    }

    if (curr == p) {
        /* Remove p by updating its neighbors */
        p->p_prev->p_next = p->p_next;
        p->p_next->p_prev = p->p_prev;

        /* Update tail pointer if necessary */
        if (p == *tp) {
            *tp = p->p_prev;
        }

        /* Clear p's links */
        p->p_next = NULL;
        p->p_prev = NULL;

        return p;
    }

    return NULL;
}


/* headProcQ returns the head PCB of the process queue without removing it.
 * Input: tp - Pointer to the tail of a process queue.
 * Precondition: tp is either NULL or a valid tail pointer.
 * Return: Pointer to the head PCB; if the queue is empty, returns NULL. */
extern pcb_PTR headProcQ(pcb_t *tp) {
    /* If the queue is empty, return NULL. */
    if (emptyProcQ(tp)) {
        return NULL;
    }

    return tp->p_next;
}


/* emptyChild checks if the PCB has any children.
 * Input: p - Pointer to a PCB.
 * Precondition: p is either NULL or points to a valid PCB structure.
 * Return: TRUE if the PCB has no children; otherwise, FALSE. */
extern int emptyChild(pcb_PTR p) {
    /* If p is NULL, treat it as having no children. */
    if (p == NULL) {
        return TRUE;
    }
    /* Return TRUE if p->p_child is NULL. */
    return p->p_child == NULL;
}


/* insertChild makes the PCB pointed to by p a child of the PCB pointed to by prnt.
 * Input:
 *    prnt - Pointer to the parent PCB.
 *    p    - Pointer to the child PCB.
 * Precondition: Both prnt and p are valid PCB pointers.
 * Return: None. */
extern void insertChild(pcb_PTR prnt, pcb_PTR p) {
    /* If either parent or child is invalid, do nothing. */
    if (prnt == NULL || p == NULL) {
        return;
    }

    /* Set the parent. */
    p->p_prnt = prnt;
    /* The next sibling of the p is the head of prnt's child list. */
    p->p_next_sib = prnt->p_child;
    /* If there is an existing child, update its previous sibling pointer. */
    if (prnt->p_child != NULL) {
        prnt->p_child->p_prev_sib = p;
    }
    /* Update parent's child pointer to p. */
    prnt->p_child = p;
}


/* removeChild removes and returns the first child of the given PCB.
 * Input: p - Pointer to a parent PCB.
 * Precondition: p is a valid PCB pointer.
 * Return: Pointer to the removed first child; if no children exist, returns NULL. */
extern pcb_PTR removeChild(pcb_PTR p) {
    /* If the parent is NULL or has no children, return NULL. */
    if (p == NULL || p->p_child == NULL) {
        return NULL;
    }

    pcb_PTR child = p->p_child;  /* First child. */
    /* Remove the child from the parent's child list. */
    p->p_child = child->p_next_sib;

    /* Clear the removed child's parent and next sibling pointers. */
    child->p_prnt = NULL;
    child->p_next_sib = NULL;
    return child;
}


/* outChild removes the PCB from its parent's child list.
 * Input: p - Pointer to a PCB.
 * Precondition: p is a valid PCB pointer and may be a child of some parent.
 * Return: Pointer to p if removal is successful; if p has no parent, returns NULL.
 * Modification: If p is the first child, use removeChild().
 */
extern pcb_PTR outChild(pcb_t *p) {
    if (p == NULL || p->p_prnt == NULL) {
        return NULL;
    }

    pcb_t *parent = p->p_prnt;

    if (parent->p_child == p) {
        return removeChild(parent);
    } else {
        if (p->p_prev_sib != NULL) {
            p->p_prev_sib->p_next_sib = p->p_next_sib;
        }
        if (p->p_next_sib != NULL) {
            p->p_next_sib->p_prev_sib = p->p_prev_sib;
        }
        p->p_prnt     = NULL;
        p->p_next_sib = NULL;
        p->p_prev_sib = NULL;
        return p;
    }
}