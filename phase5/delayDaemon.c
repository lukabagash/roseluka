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

#include "../h/const.h"  
#include "../h/types.h"  
#include <umps/libumps.h>

static delayd_t delayd_pool[64]; /* not sure if the size is 64, not given in pandos */
static delayd_t *delayd_h = NULL;        
static delayd_t *delaydFree_h = NULL;        
static int ADLsem = 0; 


 /* Forward declarations */
 static delayd_t *alloc_delay_node(void);
 static void       free_delay_node(delayd_t *n);
 static void       insert_delay_node(delayd_t *n);
 static inline int tod_now(void);
 static inline support_t *cur_sup(void);
 
 /* -------------------------------------------------------------------------
  *  initADL  ▸  Initialise delay descriptor lists and semaphore
  * -------------------------------------------------------------------------*/
 void initADL(void) {
     /* 1. Build the free list from the static pool */
     for (int i = 0; i < 64; ++i) { 
         delayd_pool[i].d_next = delaydFree_h;   /* EN: push node ▸ KR: 노드를 free 리스트에 삽입 */
         delaydFree_h = &delayd_pool[i];
     }
 
     /* 2. Create a dummy tail node for the active list (sentinel) */
     delayd_h            = alloc_delay_node();       /* Should never fail – pool fresh */
     delayd_h->d_next    = NULL;                     /* Active list initially empty   */
     delayd_h->d_supStruct = NULL;
     delayd_h->d_wakeTime  = INT_MAX;                /* Sentinel has max wake time    */
 
     /* 3. ADLsem initialize to 1 (binary semaphore unlocked) */
     SYSCALL(4, (int)&ADLsem, 0, 0);     /* Unlock */
 }
 
 /* -------------------------------------------------------------------------
  *  SYSCALL 18 handler – delay current U‑proc for a1 seconds
  *  NOTE: called from Support Level exception handler while current context
  *        is the requesting U‑proc.
  * -------------------------------------------------------------------------*/
 void syscall_delay(void) {
     int secCnt = (int) GET_SYSBK_PARAM1();          /* a1 holds seconds ▸ 딜레이 초수 읽기 */
 
     if (secCnt < 0) {                              /* Negative delay ➜ error */
         SYSCALL(SYSCALL_TERMINATEPROCESS, 0, 0, 0); /* SYS9 – kill U‑proc     */
         /* Not reached */
     }
 
     /* Compute absolute wake‑up time in µs */
     int wakeTime = tod_now() + (secCnt * 1000000); /* one second = 1000000 micro‑seconds */
 
     /* ─── Enter critical section on ADL (P(ADLsem)) ─── */
     SYSCALL(3, (int)&ADLsem, 0, 0);   /* Binary semaphore lock */
 
     /* Allocate a descriptor node */
     delayd_t *newN = alloc_delay_node();
     if (newN == NULL) {
         /* Out of descriptors – release lock then terminate */
         SYSCALL(4, (int)&ADLsem, 0, 0);     /* Unlock */
         SYSCALL(SYSCALL_TERMINATEPROCESS, 0, 0, 0); /* Die    */
     }
 
     /* Populate and insert into ADL */
     newN->d_supStruct = cur_sup();  /* Pointer to caller's support structure */
     newN->d_wakeTime  = wakeTime;
     insert_delay_node(newN);        /* Ordered insertion */
 
     /* Combined atomic sequence: V(ADLsem) + P(privSem) ------------------ */
     support_t *sup = newN->d_supStruct;            /* Current U‑proc support */
 
     /* Unlock ADL */
     SYSCALL(SYSCALL_V, (int)&ADLsem, 0, 0);
 
     /* Block on private semaphore (sup->sup_sem) – the Delay‑Daemon will V() */
     SYSCALL(SYSCALL_P, (int)&(sup->sup_sem), 0, 0);
 
     /* ── When execution resumes here, the process has been woken up ── */
     return; /* Execution returns to user at LDST in caller’s exception handler */
 }
 
 /* -------------------------------------------------------------------------
  *  delay_daemon  ▸  special kernel ASID = 0 process, endless loop
  * -------------------------------------------------------------------------*/
 void delay_daemon(void) {
     while (TRUE) {
         /* Wait for next pseudo‑clock tick (100 ms) */
         SYSCALL(SYSCALL_WAITCLOCK, 0, 0, 0);
 
         /* Lock ADL */
         SYSCALL(SYSCALL_P, (int)&ADLsem, 0, 0);
 
         int now = tod_now();
         /* Examine list from head (sentinel is delayd_h) */
         while (delayd_h->d_next != NULL && delayd_h->d_next->d_wakeTime <= now) {
             delayd_t *expired = delayd_h->d_next;        /* First real node */
             delayd_h->d_next  = expired->d_next;         /* Remove from list */
 
             /* Un‑block the sleeping U‑proc */
             SYSCALL(SYSCALL_V, (int)&(expired->d_supStruct->sup_sem), 0, 0);
 
             /* Return descriptor to free list */
             free_delay_node(expired);
         }
 
         /* Unlock ADL */
         SYSCALL(SYSCALL_V, (int)&ADLsem, 0, 0);
     }
 }
 
 /* =====================  Helper Routines  ===================== */
 
 /* alloc_delay_node ▸ Pop head of free list */
 static delayd_t *alloc_delay_node(void) {
     delayd_t *n = delaydFree_h;             /* EN: take first free node ▸ KR: free 리스트 첫 노드 할당 */
     if (n != NULL) {
         delaydFree_h = n->d_next;           /* Remove from free list */
         n->d_next    = NULL;                /* Clean pointer          */
     }
     return n;                               /* May be NULL if pool exhausted */
 }
 
 /* free_delay_node ▸ Push node back onto free list */
 static void free_delay_node(delayd_t *n) {
     if (n == NULL) return;
     n->d_next    = delaydFree_h;
     delaydFree_h = n;
 }
 
 /* insert_delay_node ▸ ordered insert by d_wakeTime ascending */
 static void insert_delay_node(delayd_t *n) {
     delayd_t *prev = delayd_h;
     delayd_t *cur  = delayd_h->d_next;
 
     while (cur != NULL && cur->d_wakeTime <= n->d_wakeTime) {
         prev = cur;
         cur  = cur->d_next;
     }
     /* Insert n between prev and cur */
     n->d_next = cur;
     prev->d_next = n;
 }
 
 /* tod_now ▸ get current TOD in µs (lower 32‑bits are enough here) */
 static inline int tod_now(void) {
     unsigned int now;
     STCK(now);                    /* MIPS uMPS STCK macro – TOD low loaded */
     return (int) now;             /* Cast to signed 32‑bit */
 }
 
 /* cur_sup ▸ obtain pointer to current support structure (SYS1) */
 static inline support_t *cur_sup(void) {
     return (support_t *) SYSCALL(SYSCALL_GETSUPPORTPTR, 0, 0, 0);
 }