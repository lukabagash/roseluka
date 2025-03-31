/*This module implements the TLB exception handler (The
Pager). Since reading and writing to each U-proc’s flash device is limited
to supporting paging, this module should also contain the function(s) for
reading and writing flash devices.
Additionally, the Swap Pool table and Swap Pool semaphore are local to
this module. Instead of declaring them globally in initProc.c they can be
declared module-wide in vmSupport.c. The test function will now in-
voke a new “public” function initSwapStructs which will do the work
of initializing both the Swap Pool table and accompanying semaphore.
Technical Point: Since the code for the TLB-Refill event handler was re-
placed (without relocating the function), uTLB RefillHandler should
still be found in the Level 3/Phase 2 exceptions.c file.*/

#include "../h/vmSupport.h"
#include "../h/types.h"

HIDDEN swap_t swapPool[2*1]; /* Swap Pool table: 2 entries for 1 page each (assuming 1 page per process) */
int swapPoolSemaphore; /* A mutual exclusion semaphore (hence initialized to 1) that controls access to the Swap Pool data structure. */

void initSwapStructs() {
    int i; /* For loop index */
    
    /* Initialize the Swap Pool table */
    for (i = 0; i < 2*1; i++) {
        swapPool[i].asid = -1; /* Set the ASID to -1 indicating that the entry is free */
    }

    /* Initialize the Swap Pool semaphore */
    swapPoolSemaphore = 1; /* Set the semaphore to 1 indicating the pool is available for use */
}