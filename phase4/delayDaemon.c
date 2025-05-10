/******************************** delayDaemon.c **********************************
 * 
 * Support‑level Delay Facility for uMPS/Pandos - SYS18 implementation.
 * 
 * SYS18 causes the requesting U-proc to be temporarily “put to sleep” 
 * (delayed) for a specified number of seconds. - a daemon process
 * 
 * Written by Rosalie Lee, Luka Bagashvili
 **************************************************************************/


#include "../h/types.h"
#include "../h/const.h"
#include "../h/delayDaemon.h"
#include "../h/vmSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/* --- ADL globals --- */
static delayd_t delaydArray[UPROCMAX];  /* static array of delay event descriptor nodes (Active Delay List (ADL) to keep track of sleeping U-procs) */
static delayd_t *delaydFree_h;          /* head of free list for unused delay event descriptor nodes */
static delayd_t *delayd_h;              /* head of active (sorted) list for delay event descriptor nodes */
int semDelay;                           /* ADL semaphore for mutual exclusion */

/* pop a descriptor node from the free list and return it */
static delayd_t *allocDelay(void) {
    if (!delaydFree_h) return NULL;     /* no delay event descriptor node if free list is empty */
    delayd_t *node = delaydFree_h;      /* allocate a descriptor node from free list */
    delaydFree_h = node->d_next;        /* update the head pointer to the next available node */
    return node;
}

/* return a descriptor node to free list */
static void freeDelay(delayd_t *node) {
    node->d_next = delaydFree_h;        /* Add the node to the head of the free list */
    delaydFree_h = node;
}

/* insert node into active list in proper place sorted by d_wakeTime using pointer to pointer to avoid using prev */
static void insertDelay(delayd_t *node) {
    delayd_t **pp = &delayd_h;  /* create pointer to the pointer of head */
    while (*pp != NULL && (*pp)->d_wakeTime <= node->d_wakeTime)    /* terminate if found anything greater than node's time or if NULL */
        pp = &(*pp)->d_next;
    node->d_next = *pp;         /* since the pp has to be greater or NULL, our node next will be pp */
    *pp = node;                 /* node points to pp now, and to preserve the previous order update the address directly */
}

/* Initializes the ADL and creates the delay daemon.
 * Called once at system startup by `test()` (in initProc.c). 
 */
void initADL(void) {
    int i;
    /* build free list: NULL terminated, single, linearly linked list */
    for (i = 0; i < UPROCMAX - 1; i++) {
        delaydArray[i].d_next = &delaydArray[i + 1];
    }
    delaydArray[UPROCMAX - 1].d_next = NULL;    /* a dummy node at the tail */
    delayd_h = NULL;           /* initialize head of delaydArray to NULL */
    delaydFree_h = &delaydArray[0]; /* just like delaydArray list but will hold unused delay event descriptor nodes */
    semDelay = 1;

    /* set up the Delay Daemon's process state */
    state_t st;
    devregarea_t *devArea = (devregarea_t *) RAMBASEADDR;
    memaddr ramTop = devArea->rambase + devArea->ramsize;

    st.s_pc = (memaddr) delayDaemon;    /* set to the function implementing the Delay Daemon */
    st.s_t9 = (memaddr) delayDaemon;
    st.s_sp = ramTop - PAGESIZE;         /* an unused(penultimate) frame of RAM */
    st.s_status = ALLOFF | PANDOS_IEPBITON | TEBITON | PANDOS_CAUSEINTMASK; /* Status register is set to kernel-mode with all interrupts enabled */
    st.s_entryHI = ALLOFF | (0 << ASIDSHIFT);   /* EntryHi.ASID is set to the kernel ASID: zero*/

    SYSCALL(CREATEPROCESS, (unsigned int)&st, (unsigned int)(NULL), 0); /* the Support Structure SYS1 parameter should be NULL */
}

/* SYS18 support‐level handler - this service causes the executing U-proc to be delayed for n seconds.
Insert a delay event descriptor node to proper location on ADL from the free list 
secs: n - the number of seconds to be delayed
*/
void delaySyscall(state_t *savedState, int secs) {
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* terminate if seconds is invalid */
    if (secs < 0) {
        SYSCALL(TERMINATE, 0, 0, 0);
        return;
    }

    mutex(&semDelay, TRUE); /* gain mutual exclusion over the ADL semaphore */
    delayd_t *node = allocDelay();  /* allocate a delay event descriptor node from the free list and store the descriptor */

    /* if descriptor node is invalid, release ADL, then die */
    if (!node) {
        mutex(&semDelay, FALSE); 
        SYSCALL(TERMINATE, 0, 0, 0);
        return;
    }

    /* set up wake time and link to this support struct */
    cpu_t now;
    STCK(now);
    node->d_wakeTime   = now + (cpu_t)secs * MSECONDS;
    node->d_supStruct  = sPtr;

    insertDelay(node);  /* insert the discriptor from free list into its proper location on ADL */

    /* atomically release the ADL semaphore, then P on private semaphore to sleep U-proc */
    disableInterrupts(); 
    mutex(&semDelay, FALSE); 
    mutex(&(sPtr->sup_delaySem), TRUE); 
    enableInterrupts();  

    /* resume U-proc after it's woken up */
    LDST(savedState);
}

/* the daemon that wakes up sleeping U‑procs if expired */
void delayDaemon(void) {
    while (TRUE) {
        /* SYS7: sleep until a 100 ms interrupt */
        SYSCALL(WAITCLOCK, 0, 0, 0);

        mutex(&semDelay, TRUE); /* Gain mutual exclusion over the ADL semaphore */

        /* get current TOD */
        cpu_t now;
        STCK(now);

        /* wake up all expired U-procs from ADL */
        while (delayd_h != NULL && delayd_h->d_wakeTime <= now) {

            /* Deallocate the delay event descriptor node (remove current node from the queue) */
            delayd_t *n = delayd_h;
            delayd_h = n->d_next;
            mutex(&n->d_supStruct->sup_delaySem, FALSE); /* wake up U-proc */

            /* return the node to the free list. */
            freeDelay(n);
        }
        mutex(&semDelay, FALSE); /* Release the ADL semaphore */
    }
}
