#include "../h/asl.h"
#include "../h/pcb.h"    /* If pcb_PTR is defined here or in types.h, include as appropriate */
#include "../h/const.h"  /* If needed for MAXPROC or other constants */
#include "../h/types.h"  /* If needed for type definitions */

HIDDEN semd_t *semd_h = NULL;     /* Head of the Active Semaphore List (ASL) */
HIDDEN semd_t *semdFree_h = NULL; /* Head of the free list for semaphore descriptors */

static semd_t semdTable[MAXPROC];

/*
 * Helper function: search_semd
 *
 * Given a semaphore address, this function traverses the ASL (which is sorted by s_semAdd)
 * and returns a pointer to the semaphore descriptor if found.
 * In addition, it sets *prev to the pointer to the descriptor immediately preceding the
 * matching descriptor (or remains NULL if the matching descriptor is at the head).
 *
 * If no descriptor with semAdd is found, returns NULL and *prev points to the last node
 * whose s_semAdd is less than semAdd (or remains NULL if the ASL is empty or semAdd is smallest).
 */
static semd_t *search_semd (int *semAdd, semd_t **prev) {
    semd_t *curr = semd_h;
    *prev = NULL;
    while (curr != NULL && curr->s_semAdd < semAdd) {
        *prev = curr;
        curr = curr->s_next;
    }
    if (curr != NULL && curr->s_semAdd == semAdd) {
        return curr;
    }
    return NULL;
}

extern int insertBlocked (int *semAdd, pcb_t *p) {
    /* Insert the pcb pointed to by p at the tail of the process queue associated with the semaphore whose physical address is semAdd and
    set the semaphore address of p to semAdd. If the semaphore is currently not active (i.e. there is no descriptor for it in the ASL), allocate
    a new descriptor from the semdFree list, insert it in the ASL (at the
    appropriate position), initialize all of the fields (i.e. set s_semAdd
    to semAdd, and s_procq to mkEmptyProcQ()), and proceed as
    above. If a new semaphore descriptor needs to be allocated and the
    semdFree list is empty, return TRUE. In all other cases return FALSE.
    */
   semd_t *prev, *sd;
    
    /* Search the ASL for semAdd */
    sd = search_semd(semAdd, &prev);
    
    if (sd == NULL) {
        /* No descriptor found; we need to allocate one from semdFree_h */
        if (semdFree_h == NULL) {
            return TRUE;  /* error: no free semaphore descriptor available */
        }
        sd = semdFree_h;
        semdFree_h = semdFree_h->s_next;  /* remove from free list */
        
        /* Initialize the new semaphore descriptor */
        sd->s_semAdd = semAdd;
        sd->s_procQ = mkEmptyProcQ();  /* should return NULL (empty queue) */
        sd->s_next = NULL;
        
        /* Insert the new descriptor into the ASL so that the list remains sorted */
        if (prev == NULL) {
            /* Insertion at head */
            sd->s_next = semd_h;
            semd_h = sd;
        } else {
            sd->s_next = prev->s_next;
            prev->s_next = sd;
        }
    }
    
    /* Insert the PCB into the process queue for this semaphore */
    insertProcQ(&(sd->s_procQ), p);
    p->p_semAdd = semAdd;
    return FALSE;
}


extern pcb_PTR removeBlocked (int *semAdd) {
    /* Search the ASL for a descriptor of this semaphore. If none is found,
    return NULL; otherwise, remove the first (i.e. head) pcb from the process queue of the found semaphore descriptor, set that pcb’s address
    to NULL, and return a pointer to it. If the process queue for this
    semaphore becomes empty (emptyProcQ(s_procq) is TRUE),
    remove the semaphore descriptor from the ASL and return it to the
    semdFree list. */
    semd_t *prev, *sd;
    pcb_t *p;
    
    /* Locate the semaphore descriptor in the ASL */
    sd = search_semd(semAdd, &prev);
    if (sd == NULL) {
        return NULL;
    }
    
    /* Remove the head PCB from the process queue */
    p = removeProcQ(&(sd->s_procQ));
    if (p != NULL) {
        p->p_semAdd = NULL;
    }
    
    /* If the process queue becomes empty, remove the semaphore descriptor from the ASL */
    if (emptyProcQ(sd->s_procQ)) {
        if (prev == NULL) {
            semd_h = sd->s_next;
        } else {
            prev->s_next = sd->s_next;
        }
        /* Return the descriptor to the free list */
        sd->s_next = semdFree_h;
        semdFree_h = sd;
    }
    
    return p;

}


extern pcb_PTR outBlocked (pcb_PTR p) {
    /* Remove the pcb pointed to by p from the process queue associated
    with p’s semaphore (p→ p_semAdd) on the ASL. If pcb pointed to by p does not appear in the process queue associated with p’s
    semaphore, which is an error condition, return NULL; otherwise, return p. Unlike in removeBlocked do NOT (re)set p’s semaphore
    address (p_semAdd) to NULL. */
    semd_t *prev, *sd;
    pcb_t *removed;
    
    if (p == NULL || p->p_semAdd == NULL) {
        return NULL;
    }
    
    /* Locate the semaphore descriptor corresponding to p->p_semAdd */
    sd = search_semd(p->p_semAdd, &prev);
    if (sd == NULL) {
        return NULL;
    }
    
    /* Remove p from the process queue */
    removed = outProcQ(&(sd->s_procQ), p);
    if (removed == NULL) {
        return NULL;
    }
    
    /* If the process queue becomes empty, remove the semaphore descriptor from the ASL */
    if (emptyProcQ(sd->s_procQ)) {
        if (prev == NULL) {
            semd_h = sd->s_next;
        } else {
            prev->s_next = sd->s_next;
        }
        sd->s_next = semdFree_h;
        semdFree_h = sd;
    }
    
    /* Do NOT reset p->p_semAdd */
    return p;
}


extern pcb_PTR headBlocked (int *semAdd) {
    /* Return a pointer to the pcb that is at the head of the process queue
    associated with the semaphore semAdd. Return NULL if semAdd is
    not found on the ASL or if the process queue associated with semAdd
    is empty. */
    semd_t *prev, *sd;
    
    sd = search_semd(semAdd, &prev);
    if (sd == NULL) {
        return NULL;
    }
    
    return headProcQ(sd->s_procQ);

}


extern void initASL () {
    /* Initialize the semdFree list to contain all the elements of the array
    static semd_t semdTable[MAXPROC]
    This method will be only called once during data structure initialization. */
    int i;
    
    for (i = 0; i < MAXPROC - 1; i++) {
        semdTable[i].s_next = &semdTable[i + 1];
        semdTable[i].s_semAdd = NULL;
        semdTable[i].s_procQ = NULL;
    }
    semdTable[MAXPROC - 1].s_next = NULL;
    semdTable[MAXPROC - 1].s_semAdd = NULL;
    semdTable[MAXPROC - 1].s_procQ = NULL;
    
    semdFree_h = semdTable;
    semd_h = NULL;
}