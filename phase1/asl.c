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
#include "../h/pcb.h"    
#include "../h/const.h"  
#include "../h/types.h"  


HIDDEN semd_t *semd_h = NULL;   /* Head of ASL which holds semaphore descriptors for active semaphores. */
HIDDEN semd_t *semdFree_h = NULL;   /* Head of the free list for semaphore descriptors. */


/* search_semd searches for a semaphore descriptor in the ASL based on the semaphore's physical address.
 * Input:
 *    semAdd - Pointer to the semaphore's physical address.
 *    prev   - Address of a pointer variable that will be set to point to the descriptor
 *             immediately preceding the found descriptor, or remain NULL if the found
 *             descriptor is at the head (or if the ASL is empty).
 * Precondition: The ASL is maintained in sorted order based on the s_semAdd field.
 * Return: Pointer to the semaphore descriptor with s_semAdd equal to semAdd if found; otherwise, returns NULL. */
static semd_t *search_semd (int *semAdd, semd_t **prev) {
    semd_t *curr = semd_h;
    *prev = NULL;
    while (curr != NULL && curr->s_semAdd < semAdd) {
        *prev = curr;
        curr = curr->s_next;
    }
    /*Check if the current descriptor matches the semaphore address*/
    if (curr != NULL && curr->s_semAdd == semAdd) {
        return curr;
    }
    return NULL;
}

/* insertBlocked inserts a PCB into the blocked queue for a semaphore.
 * Input:
 *    semAdd - Pointer to the semaphore's physical address.
 *    p      - Pointer to the PCB to insert.
 * Precondition:
 *    p is a valid PCB.
 * Return:
 *    TRUE if a new semaphore descriptor was needed but none was available;
 *    FALSE otherwise. */
extern int insertBlocked (int *semAdd, pcb_t *p) {
    semd_t *prev, *sd;
    
    /* Look for an existing descriptor in the ASL */
    sd = search_semd(semAdd, &prev);
    
    /*If no descriptor exists for the semaphore*/
    if (sd == NULL) {
        /* Allocate a new descriptor from the free list if available */
        if (semdFree_h == NULL) {
            return TRUE;
        }
        sd = semdFree_h;
        semdFree_h = semdFree_h->s_next;  /* Remove descriptor from free list */
        
        sd->s_semAdd = semAdd;
        sd->s_procQ = mkEmptyProcQ();
        sd->s_next = NULL;
        
        /* Insert the new descriptor into the ASL in sorted order */
        if (prev == NULL) {
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

/* removeBlocked removes the head PCB from a semaphore's blocked queue.
 * Input:
 *    semAdd - Pointer to the semaphore's physical address.
 * Precondition:
 *    semAdd is valid; however, the ASL may not contain its descriptor.
 * Return:
 *    Pointer to the removed PCB if successful; otherwise, NULL. */
extern pcb_PTR removeBlocked (int *semAdd) {
    semd_t *prev, *sd;
    pcb_t *p;
    
    sd = search_semd(semAdd, &prev);

    /*If no descriptor exists for the semaphore*/
    if (sd == NULL) {
        return NULL;
    }
    
    p = removeProcQ(&(sd->s_procQ));

    /* Clear the PCB's semaphore field */
    if (p != NULL) {
        p->p_semAdd = NULL;
    }
    
    /* If the queue becomes empty, remove the descriptor from the ASL */
    if (emptyProcQ(sd->s_procQ)) {
        /* If the descriptor is at the head of the ASL */
        if (prev == NULL) {
            semd_h = sd->s_next;
        } else {
            prev->s_next = sd->s_next;
        }
        sd->s_next = semdFree_h;
        semdFree_h = sd;
    }
    
    return p;
}

/* outBlocked removes a specific PCB from its semaphore's blocked queue.
 * Input:
 *    p - Pointer to the PCB to remove.
 * Precondition:
 *    p is valid and its p_semAdd field is non-NULL.
 * Return:
 *    Pointer to the removed PCB if found; otherwise, NULL. */
extern pcb_PTR outBlocked (pcb_PTR p) {
    semd_t *prev, *sd;
    pcb_t *removed;
    
    /*No descriptor exists for the semaphore*/
    if (p == NULL || p->p_semAdd == NULL) {
        return NULL;
    }
    
    /* Locate the semaphore descriptor for p->p_semAdd in the ASL */
    sd = search_semd(p->p_semAdd, &prev);
    if (sd == NULL) {
        return NULL;
    }
    
    removed = outProcQ(&(sd->s_procQ), p);
    /*PCB was not found in the queue*/
    if (removed == NULL) {
        return NULL;
    }
    
    /* Remove the descriptor from the ASL if its queue is now empty */
    if (emptyProcQ(sd->s_procQ)) {
        /*Descriptor is at the head of the ASL*/
        if (prev == NULL) {
            semd_h = sd->s_next;
        } else {
            prev->s_next = sd->s_next;
        }
        sd->s_next = semdFree_h;
        semdFree_h = sd;
    }
    
    return p;
}

/* headBlocked retrieves the head PCB from a semaphore's blocked queue without removal.
 * Input:
 *    semAdd - Pointer to the semaphore's physical address.
 * Precondition:
 *    The ASL may or may not contain the descriptor for semAdd.
 * Return:
 *    Pointer to the head PCB if it exists; otherwise, NULL. */
extern pcb_PTR headBlocked (int *semAdd) {
    semd_t *prev, *sd;
    
    sd = search_semd(semAdd, &prev);
    /*No descriptor exists for the semaphore*/
    if (sd == NULL) {
        return NULL;
    }
    
    return headProcQ(sd->s_procQ);
}


/* initASL initializes the ASL and free semaphore descriptor list.
 * Precondition:
 *    None.
 * Return:
 *    None. */
extern void initASL () {
    static semd_t semdTable[MAXPROC];   /* A static array of semaphore descriptors. */

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
