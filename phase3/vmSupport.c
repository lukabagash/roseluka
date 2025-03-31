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
#include "/usr/include/umps3/umps/libumps.h"

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

void supLvlTlbExceptionHandler() {
    support t *sPtr = SYSCALL (GETSUPPORTPTR, 0, 0, 0);
    unsigned int cause = sPtr->sup_exceptState[0].s_cause; /* Get the cause of the TLB exception */
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT; /* Extract the exception code from the cause register */
    unsigned int entryHI = sPtr->sup_exceptState[0].s_entryHI; /* Get the Entry HI value from the saved state */
    int missingPN; /* Page number extracted from Entry HI */
    int frameNumber; /* Frame number to be used for the page replacement */

    if(exc_code == 1) /*TLB-Modification Exception*/
    {
        programTrapHandler(); /* Handle the TLB modification exception by invoking the program trap handler */
    }

    SYSCALL(3, unsigned int &swapPoolSemaphore, 0, 0); /* Perform a P operation on the swap pool semaphore to ensure mutual exclusion */

    missingPN = (entryHI & 0xFFFFF000) >> 12; /* Extract the missing page number from Entry HI */

    frameNumber = (frameNumber + 1) % (2*1); /* Simple page replacement algorithm: round-robin replacement for the sake of example */
    if(swapPool[frameNumber].asid != -1) /* If the frame is already occupied, we need to swap out the existing page */
    {
       setSTATUS(getSTATUS() & ~0x00000001); /* Disable interrupts to prevent context switching during the page replacement */
       swapPool[frameNumber].pte->entryLO = (swapPool[frameNumber].pte->entryLO & 0xFFFFFDFF); /* Valid off*/ 
       setSTATUS(getSTATUS() | (0x00000001)); /* Enable interrupts again after setting the status */
    }

    /*
    STEPS LEFT [Section 4.4.2]:
    8. If frame iis currently occupied, assume it is occupied by logical page num-
       ber kbelonging to process x(ASID) and that it is “dirty” (i.e. been modi-
       fied):
       (a) Update process x’s Page Table: mark Page Table entry kas not valid.
       This entry is easily accessible, since the Swap Pool table’s entry i
       contains a pointer to this Page Table entry. (done)
       (b) Update the TLB, if needed. The TLB is a cache of the most recently
       executed process’s Page Table entries. If process x’s page k’s Page
       Table entry is currently cached in the TLB it is clearly out of date; it
       was just updated in the previous step.
       Important Point: This step and the previous step must be accom-
       plished atomically. [Section 4.5.3]
       (c) Update process x’s backing store. Write the contents of frame ito the
       correct location on process x’s backing store/flash device. [Section
       4.5.1]
       Treat any error status from the write operation as a program trap. [Sec-
       tion 4.8]
    9. Read the contents of the Current Process’s backing store/flash device logical
       page pinto frame i. [Section 4.5.1]
       Treat any error status from the read operation as a program trap. [Section
       4.8]
    10. Update the Swap Pool table’s entry ito reflect frame i’s new contents: page
        p belonging to the Current Process’s ASID, and a pointer to the Current
        Process’s Page Table entry for page p.
    11. Update the Current Process’s Page Table entry for page p to indicate it is
        now present (V bit) and occupying frame i(PFN field).
    12. Update the TLB. The cached entry in the TLB for the Current Process’s
        page pis clearly out of date; it was just updated in the previous step.
        Important Point: This step and the previous step must be accomplished
        atomically. [Section 4.5.3]
    13. Release mutual exclusion over the Swap Pool table. (SYS4 – V operation
        on the Swap Pool semaphore)
    14. Return control to the Current Process to retry the instruction that caused the
        page fault: LDST on the saved exception state.
    */
}