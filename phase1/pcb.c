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
 * Precondition: Called once during system initialization. */
extern void initPcbs() {
    static pcb_t pcbFreeTable[MAXPROC]; /* A static array of PCBs. */

    /* Set the head of the free list to the first element of the array. */
    pcbFree_h = &pcbFreeTable[0];

    int i;
    /* Link each PCB to the next one in the array. */
    for (i = 0; i < MAXPROC - 1; i++) {
        pcbFreeTable[i].p_next = &pcbFreeTable[i + 1];
    }
    /* The last PCB's next pointer is set to NULL. */
    pcbFreeTable[MAXPROC - 1].p_next = NULL;
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
    return (tp == NULL) ? TRUE : FALSE;
}


/* insertProcQ inserts the PCB pointed to by p into the process queue.
 * Input: 
 *    tp - Pointer to the tail pointer of a process queue.
 *    p - Pointer to a PCB to be inserted.
 * Precondition: If the queue is non-empty, *tp points to a valid tail in a circular queue. */
extern void insertProcQ(pcb_t **tp, pcb_t *p) {
    /* If the queue is empty, initialize circular list with a single element. */
    if (*tp == NULL) {
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
    if (*tp == NULL) {
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
 *    tp - Pointer to the tail pointer of a process queue (pcb_t **tp)
 *    p - Pointer to a PCB (pcb_t *p) that should be removed.
 * Precondition: If the queue is non-empty, *tp points to a valid tail of a circular queue.
 * Return: Pointer to the PCB if removal is successful; otherwise, returns NULL. */
extern pcb_t *outProcQ(pcb_t **tp, pcb_t *p) {
    /* If the queue is empty or p is invalid, return NULL. */
    if (*tp == NULL || p == NULL) {
        return NULL;
    }

    pcb_t *tail = *tp;
    pcb_t *curr = tail;
    int found = FALSE;

    /* Traverse the circular queue to verify that p is present. */
    do {
        if (curr == p) {
            found = TRUE;
            break;
        }
        curr = curr->p_next;
    } while (curr != tail);

    /* If p is not found in the queue, return NULL. */
    if (!found) {
        return NULL;
    }

    /* If p is the only element in the queue. */
    if (p->p_next == p && p->p_prev == p) {
        *tp = NULL;
    } else {
        /* Remove p by linking its neighbors. */
        p->p_prev->p_next = p->p_next;
        p->p_next->p_prev = p->p_prev;

        /* If p is the tail, update the tail pointer. */
        if (p == *tp) {
            *tp = p->p_prev;
        }
    }

    /* Clear p's pointers. */
    p->p_next = NULL;
    p->p_prev = NULL;
    return p;
}


/* headProcQ returns the head PCB of the process queue without removing it.
 * Input: tp - Pointer to the tail of a process queue.
 * Precondition: tp is either NULL or a valid tail pointer.
 * Return: Pointer to the head PCB; if the queue is empty, returns NULL. */
extern pcb_PTR headProcQ(pcb_t *tp) {
    /* If the queue is empty, return NULL. */
    if (tp == NULL) {
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
    return (p->p_child == NULL) ? TRUE : FALSE;
}


/* insertChild makes the PCB pointed to by p a child of the PCB pointed to by prnt.
 * Input:
 *    prnt - Pointer to the parent PCB.
 *    p - Pointer to the child PCB.
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
 * Return: Pointer to p if removal is successful; if p has no parent, returns NULL. */
extern pcb_PTR outChild(pcb_t *p) {
    /* If p is NULL or has no parent, nothing to remove. */
    if (p == NULL || p->p_prnt == NULL) {
        return NULL;
    }

    pcb_t *parent = p->p_prnt;

    /* If p is the first child, update parent's child pointer to the next sibling. */
    if (p->p_prev_sib == NULL) {
        parent->p_child = p->p_next_sib;
    } else {
        /* If p is not the first child, link p's previous sibling to p's next sibling. */
        p->p_prev_sib->p_next_sib = p->p_next_sib;
    }

    /* If there is a next sibling, link p's next sibling to p's previous sibling. */
    if (p->p_next_sib != NULL) {
        p->p_next_sib->p_prev_sib = p->p_prev_sib;
    }

    /* Clear the p's pointers. */
    p->p_prnt = NULL;
    p->p_next_sib = NULL;
    p->p_prev_sib = NULL;
    return p;
}