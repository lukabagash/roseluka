/******************************** delayDaemon.c **********************************
 * 
 * Support‑level Delay Facility for uMPS/Pandos ‑ SYS18 implementation.
 * 
 *  ‑ initADL()         : Initialise the Active Delay List and free list.
 *  ‑ syscall_delay()   : Service routine for SYS18 (delay current U‑proc).
 *  ‑ delay_daemon()    : Support‑level kernel process that wakes up
 *                        sleeping U‑procs whose delay has expired.
 *  ‑ Helper functions  : allocate / deallocate nodes and ordered insertion.
 * 
 * Written by Rosalie Lee, Luka Bagashvili
 **************************************************************************/


/*  
Initializing the ADL is facilitated by the InstantiatorProcess [Section 4.9] which
invokes the ADL function initADL().
Initializing the ADL is a two-step process:
1. Add each element from the static array of delay event descriptor nodes to
the free list and initialize the active list (zero, one or two dummy nodes).

2. Initialize and launch (SYS1) the Delay Daemon.
The Delay Daemon is a process where
• The PC (and s t9) is set to the function implementing the Delay Daemon.
[Section 6.3.2]
• The SP is set to an unused frame at the end of RAM. The last frame of RAM
is already allocated as the stack page for test. Whether the penultimate
frame of RAM is available is dependent on the Level 4/Phase 3 approach
for allocating the two stack spaces per U-proc: one for the Support Level’s
TLB exception handler, and one for the Support Level’s general exception
handler. If these stack spaces are allocated as part of the Support Structure,
then the penultimate RAM frame is to be used, otherwise allocate a frame
above/below the stack frames allocated for the U-proc exception handlers.
[Section 4.10]
• The Status register is set to kernel-mode with all interrupts enabled.
• The EntryHi.ASID is set to the kernel ASID: zero.
Finally, the Support Structure SYS1 parameter should be NULL.

Delay (SYS18)
This service causes the executing U-proc to be delayed for n seconds.
The Delay or SYS18 service is requested by the calling U-proc by placing the
value 18 in a0, the number of seconds to be delayed in a1, and then executing a
SYSCALL instruction.
Attempting to request a Delay for less than 0 seconds is an error and should
result in the U-proc begin terminated (SYS9). 

The SYS18 service performs two functions:
1. Allocate a delay event descriptor node from the free list, populate it, and
insert it into its proper location on the active list.
2. Perform a P (SYS3) operation on the U-proc’s private semaphore; a field in
the Support Structure.
Two considerations must be taken into account:
• The ADL is a shared data structure, accessed by both U-procs via SYS18
and the Delay Daemon. Hence, access to the ADL must be done in a mutually exclusive manner to avoid race conditions. Therefore, the ADL must
also implement a mutual exclusion semaphore (i.e. initialized to one). This
semaphore must be SYS3/P’ed prior to any access of the ADL, and then
SYS4/V’ed upon conclusion of any access.
• At the conclusion of SYS18 is a call to SYS3/P on the U-proc’s private
semaphore. However, this call must be made after the SYS4/V call on the
ADL semaphore; otherwise the U-proc will be put to sleep while holding
mutual exclusion over the ADL. Hence, one must first release the mutual
exclusion over the ADL semaphore (SYS4/V) and then block the calling
U-proc on its private semaphore (SYS3/P). Furthermore, these two actions
must be done atomically.
The complete sequence of steps to be performed for a SYS18 is:
1. Check the seconds parameter and terminate (SYS9) the U-proc if the wait
time is negative.
2. Obtain mutual exclusion over the ADL: SYS3/P on the ADL semaphore.
3. Allocate a delay event descriptor node from the free list, populate it and
insert it into its proper location on the active list. If this operation is unsuccessful, terminate (SYS9) the U-proc – after releasing mutual exclusion
over the ADL.
4. Release mutual exclusion over the ADL: SYS4/V on the ADL semaphore
AND execute a SYS3/P on the U-proc’s private semaphore atomically. This
will block the executing U-proc.
5. Return control (LDST) to the U-proc at the instruction immediately following the SYS18. This step will not be executed until after the U-proc is
awoken.


The code for the Delay Daemon is a simple infinite loop:
1. Execute a SYS7: wait for the next 100 millisecond time span (i.e. pseudo
clock tick) to pass.
2. Obtain mutual exclusion over the ADL: SYS3/P on the ADL semaphore.
3. “Process” the ADL active list and for each delay event descriptor node
whose wake up time has passed:
(a) Perform a SYS4/V on that U-proc’s private semaphore.
(b) Deallocate the delay event descriptor node and return it to the free list.
4. Release mutual exclusion over the ADL: SYS4/V on the ADL semaphore.
*/

#include "../h/types.h"
#include "../h/const.h"
#include "../h/delayDaemon.h"
#include "../h/vmSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/* --- ADL globals --- */
static delayd_t delaydArray[UPROCMAX];   /* static array of delay event descriptor nodes (Active Delay List (ADL) to keep track of sleeping U-procs) */
static delayd_t *delaydFree_h;           /* head of free list for unused delay event descriptor nodes */
static delayd_t *delayd_h;               /* head of active (sorted) list for delay event descriptor nodes */
int semDelay;               /* mutex for ADL */

static delayd_t *allocDelay(void) {
    if (!delaydFree_h) return NULL; /* no delay event descriptor node if free list is empty */
    delayd_t *node = delaydFree_h; /* allocate a descriptor node from free list */
    delaydFree_h = node->d_next;    /* update the head pointer to the next available node */
    return node;
}

/* return a descriptor to free list */
static void freeDelay(delayd_t *node) {
    node->d_next = delaydFree_h;    /* Add the node to the head of the free list */
    delaydFree_h = node;
}

/* insert node into active list sorted by d_wakeTime using pointer to pointer to avoid using prev */
static void insertDelay(delayd_t *node) {
    delayd_t **pp = &delayd_h; /* create pointer to the pointer of head*/
    while (*pp != NULL && (*pp)->d_wakeTime <= node->d_wakeTime) /* terminate if found anything greater than node's time or if NULL*/
        pp = &(*pp)->d_next;
    node->d_next = *pp; /* because the pp has to be greater or NULL our node next will be pp*/
    *pp = node; /* node points to pp now, and to preserve the previous order (if any) update the address directly*/
}

/* Called once at system startup (by test()) */
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

    state_t st;
    devregarea_t *devArea = (devregarea_t *) RAMBASEADDR;
    memaddr ramTop = devArea->rambase + devArea->ramsize;

    st.s_pc = (memaddr) delayDaemon;    /* set to the function implementing the Delay Daemon */
    st.s_t9 = (memaddr) delayDaemon;
    st.s_sp = ramTop - PAGESIZE;         /* an unused(penultimate) frame of RAM */
    st.s_status = ALLOFF | PANDOS_IEPBITON | TEBITON | PANDOS_CAUSEINTMASK; /* Status register is set to kernel-mode with all interrupts enabled */
    st.s_entryHI = ALLOFF | (0 << ASIDSHIFT);   /* how does this EntryHi.ASID is set to the kernel ASID: zero*/
    /* no support struct, runs in kernel ASID */

    SYSCALL(CREATEPROCESS, (unsigned int)&st, (unsigned int)(NULL), 0); /* the Support Structure SYS1 parameter should be NULL */
}

/* SYS18 support‐level handler 
Insert a delay event descriptor node to proper location on ADL from the free list */
void delaySyscall(state_t *savedState, int secs) {
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* terminate if invalid*/
    if (secs < 0) {
        SYSCALL(TERMINATE, 0, 0, 0);
        return;
    }

    /* P on ADL mutex */
    /*SYSCALL(PASSEREN, (unsigned int)&semDelay, 0, 0);*/
    mutex(&semDelay, TRUE); /* Gain mutual exclusion over the ADL semaphore */
    delayd_t *node = allocDelay();  /* Allocate a delay event descriptor node from the free list and store the descriptor */
    if (!node) {
        /* release ADL, then die */
        mutex(&semDelay, FALSE); /* Release the ADL semaphore */
        SYSCALL(TERMINATE, 0, 0, 0);
        return;
    }

    /* set up wake time & link to this support struct */
    cpu_t now;
    STCK(now);
    node->d_wakeTime   = now + (cpu_t)secs * MSECONDS;
    node->d_supStruct  = sPtr;

    insertDelay(node);  /* insert the discriptor from free list into its proper location on ADL */

    /* V on ADL mutex, then P on this proc’s private semaphore atomically */
    disableInterrupts(); /* disable interrupts */
    mutex(&semDelay, FALSE); /* Release the ADL semaphore */
    mutex(&(sPtr->sup_delaySem), TRUE); /* P on this proc's private semaphore */
    enableInterrupts();  /* re-enable interrupts */
    /* when woken, return here */
    LDST(savedState);
}

/* the daemon that wakes up sleeping U‑procs */
void delayDaemon(void) {
    while (TRUE) {
        /* SYS7: sleep until a 100 ms interrupt */
        SYSCALL(WAITCLOCK, 0, 0, 0);

        mutex(&semDelay, TRUE); /* Gain mutual exclusion over the ADL semaphore */
        /* awaken from the ADL for each delay event descriptor node whose wakeTime has passed */
        cpu_t now;
        STCK(now);
        /* When ADL is not empty and wakeTime has passed */
        while (delayd_h != NULL && delayd_h->d_wakeTime <= now) {
            /* Unblock the U-proc */
            /* Deallocate the delay event descriptor node (remove current node from the queue) */
            delayd_t *n = delayd_h;
            delayd_h = n->d_next;
            mutex(&n->d_supStruct->sup_delaySem, FALSE); /* Release the private semaphore */

            /* return the node to the free list. */
            freeDelay(n);
        }
        mutex(&semDelay, FALSE); /* Release the ADL semaphore */
    }
}
