/************************** ASL.C ******************************
*
*  The implementation file for the Active Semaphore List (ASL) Module.
*
*  This module manages the active semaphore descriptors and their associated
*  process queues. It provides routines to insert, remove, and query processes
*  blocked on semaphores, as well as initializing the semaphore descriptor free list.
*
*  Written by Luka Bagashvili, Rosalie Lee
*/

#include "../h/asl.h"
#include "../h/pcb.h"    /* If pcb_PTR is defined here or in types.h, include as appropriate */
#include "../h/const.h"  /* For MAXPROC and other constants */
#include "../h/types.h"  /* For type definitions */

/* 
 * Global Variables:
 *
 * semd_h: Head of the Active Semaphore List (ASL). This list holds the semaphore
 *         descriptors for active semaphores.
 *
 * semdFree_h: Head of the free list for semaphore descriptors. This list is used
 *             to allocate new semaphore descriptors when a blocked process requires
 *             one.
 */
HIDDEN semd_t *semd_h = NULL;
HIDDEN semd_t *semdFree_h = NULL;

/*
 * semdTable: A static array of semaphore descriptors.
 *           The free list is initialized to contain all the elements in this table.
 */
static semd_t semdTable[MAXPROC];

/*
 * search_semd
 *
 * Searches for a semaphore descriptor in the ASL based on the semaphore's physical address.
 *
 * Input:
 *    semAdd - Pointer to the semaphore's physical address.
 *    prev   - Address of a pointer variable that will be set to point to the descriptor
 *             immediately preceding the found descriptor, or remain NULL if the found
 *             descriptor is at the head (or if the ASL is empty).
 *
 * Precondition:
 *    The ASL is maintained in sorted order based on the s_semAdd field.
 *
 * Return:
 *    Pointer to the semaphore descriptor with s_semAdd equal to semAdd if found;
 *    otherwise, returns NULL.
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

/*
 * insertBlocked
 *
 * Inserts the PCB pointed to by p into the process queue associated with the
 * semaphore whose physical address is semAdd.
 *
 * Input:
 *    semAdd - Pointer to the semaphore's physical address.
 *    p      - Pointer to the PCB to be inserted.
 *
 * Precondition:
 *    p is a valid PCB pointer.
 *
 * Behavior:
 *    - If the semaphore descriptor for semAdd does not exist in the ASL, a new descriptor
 *      is allocated from the semdFree list.
 *    - The new descriptor is initialized (fields s_semAdd, s_procQ, and s_next) and inserted
 *      into the ASL in its proper sorted position.
 *    - The PCB is then inserted at the tail of the process queue for the semaphore,
 *      and its p_semAdd field is set to semAdd.
 *
 * Return:
 *    TRUE if a new semaphore descriptor was needed but none was available (error condition),
 *    FALSE otherwise.
 */
extern int insertBlocked (int *semAdd, pcb_t *p) {
    semd_t *prev, *sd;
    
    /* Search the ASL for an existing semaphore descriptor with semAdd */
    sd = search_semd(semAdd, &prev);
    
    if (sd == NULL) {
        /* No descriptor found; allocate one from the free list */
        if (semdFree_h == NULL) {
            return TRUE;  /* Error: no free semaphore descriptor available */
        }
        sd = semdFree_h;
        semdFree_h = semdFree_h->s_next;  /* Remove descriptor from free list */
        
        /* Initialize the new semaphore descriptor */
        sd->s_semAdd = semAdd;
        sd->s_procQ = mkEmptyProcQ();  /* Creates an empty process queue (should return NULL) */
        sd->s_next = NULL;
        
        /* Insert the new descriptor into the ASL in sorted order */
        if (prev == NULL) {
            /* Insertion at the head of the ASL */
            sd->s_next = semd_h;
            semd_h = sd;
        } else {
            /* Insertion after the descriptor pointed to by prev */
            sd->s_next = prev->s_next;
            prev->s_next = sd;
        }
    }
    
    /* Insert the PCB into the process queue for this semaphore */
    insertProcQ(&(sd->s_procQ), p);
    p->p_semAdd = semAdd;
    return FALSE;
}

/*
 * removeBlocked
 *
 * Removes the first (head) PCB from the process queue associated with the semaphore
 * whose physical address is semAdd.
 *
 * Input:
 *    semAdd - Pointer to the semaphore's physical address.
 *
 * Precondition:
 *    semAdd is a valid semaphore address; however, the ASL may not contain a descriptor
 *    for it.
 *
 * Behavior:
 *    - Searches for the semaphore descriptor in the ASL.
 *    - If found, removes the head PCB from its process queue and sets that PCB's p_semAdd field to NULL.
 *    - If the process queue becomes empty after removal, the semaphore descriptor is removed from the ASL
 *      and returned to the semdFree list.
 *
 * Return:
 *    Pointer to the removed PCB if successful; otherwise, returns NULL.
 */
extern pcb_PTR removeBlocked (int *semAdd) {
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
    
    /* If the process queue is now empty, remove the semaphore descriptor from the ASL */
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

/*
 * outBlocked
 *
 * Removes a specific PCB from the process queue associated with its semaphore.
 *
 * Input:
 *    p - Pointer to the PCB to be removed.
 *
 * Precondition:
 *    p is a valid PCB pointer and its p_semAdd field is non-NULL.
 *
 * Behavior:
 *    - Locates the semaphore descriptor corresponding to p->p_semAdd in the ASL.
 *    - Removes the PCB p from the associated process queue.
 *    - If p is not found within that queue, returns NULL.
 *    - If the process queue becomes empty after removal, the semaphore descriptor is removed
 *      from the ASL and returned to the semdFree list.
 *    - Unlike removeBlocked, p->p_semAdd is NOT reset.
 *
 * Return:
 *    Pointer to the PCB if removal is successful; otherwise, returns NULL.
 */
extern pcb_PTR outBlocked (pcb_PTR p) {
    semd_t *prev, *sd;
    pcb_t *removed;
    
    /* Validate input PCB and its semaphore address */
    if (p == NULL || p->p_semAdd == NULL) {
        return NULL;
    }
    
    /* Locate the semaphore descriptor for p->p_semAdd in the ASL */
    sd = search_semd(p->p_semAdd, &prev);
    if (sd == NULL) {
        return NULL;
    }
    
    /* Remove PCB p from the process queue */
    removed = outProcQ(&(sd->s_procQ), p);
    if (removed == NULL) {
        return NULL;
    }
    
    /* If the process queue is now empty, remove the semaphore descriptor from the ASL */
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

/*
 * headBlocked
 *
 * Returns a pointer to the head PCB of the process queue associated with the
 * semaphore whose physical address is semAdd, without removing it.
 *
 * Input:
 *    semAdd - Pointer to the semaphore's physical address.
 *
 * Precondition:
 *    The ASL may or may not contain a descriptor for semAdd.
 *
 * Behavior:
 *    - Searches for the semaphore descriptor in the ASL.
 *    - If found, returns the head of its process queue.
 *    - If the descriptor is not found or the process queue is empty, returns NULL.
 *
 * Return:
 *    Pointer to the head PCB if available; otherwise, NULL.
 */
extern pcb_PTR headBlocked (int *semAdd) {
    semd_t *prev, *sd;
    
    /* Locate the semaphore descriptor in the ASL */
    sd = search_semd(semAdd, &prev);
    if (sd == NULL) {
        return NULL;
    }
    
    return headProcQ(sd->s_procQ);
}

/*
 * initASL
 *
 * Initializes the Active Semaphore List (ASL) data structures.
 *
 * Behavior:
 *    - Initializes the semdFree list to include all elements of the static semdTable array.
 *    - Sets the s_semAdd and s_procQ fields of each descriptor in semdTable to NULL.
 *    - This function is called once during system initialization.
 *
 * Precondition:
 *    None.
 *
 * Return:
 *    None.
 */
extern void initASL () {
    int i;
    
    /* Initialize the semdTable entries and link them into the free list */
    for (i = 0; i < MAXPROC - 1; i++) {
        semdTable[i].s_next = &semdTable[i + 1];
        semdTable[i].s_semAdd = NULL;
        semdTable[i].s_procQ = NULL;
    }
    semdTable[MAXPROC - 1].s_next = NULL;
    semdTable[MAXPROC - 1].s_semAdd = NULL;
    semdTable[MAXPROC - 1].s_procQ = NULL;
    
    /* Set the free list head to the beginning of semdTable and clear the ASL */
    semdFree_h = semdTable;
    semd_h = NULL;
}
