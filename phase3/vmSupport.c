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
#include "../h/initial.h"
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

void mutex() {
    /* if true, sys3 otherwise sys4 */
}

void supLvlTlbExceptionHandler() {
    /* 14 steps in [Section 4.4.2] */
    support_t *sPtr = SYSCALL (GETSUPPORTPTR, 0, 0, 0);
    unsigned int cause = sPtr->sup_exceptState[0].s_cause; /* Get the cause of the TLB exception */
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT; /* Extract the exception code from the cause register */
    unsigned int entryHI = sPtr->sup_exceptState[0].s_entryHI; /* Get the Entry HI value from the saved state */
    int missingPN; /* Page number extracted from Entry HI */
    static int frameNumber; /* Frame number to be used for the page replacement */
    frameNumber = (frameNumber + 1) % (2*1); /* Simple page replacement algorithm: round-robin replacement for the sake of example */
    u_proc 

    if(exc_code == someshit) /* TLB-Modification Exception */
    {
        programTrapHandler(); /* Handle the TLB modification exception by invoking the program trap handler */
    }

    SYSCALL(3, unsigned int &swapPoolSemaphore, 0, 0); /* Perform a P operation on the swap pool semaphore to ensure mutual exclusion */

    missingPN = (entryHI & 0xFFFFF000) >> 12; /* Extract the missing page number from Entry HI */

    int physicaladdr = frameNumber * 32 + 12;

    if(swapPool[frameNumber].asid != -1) /* If the frame is already occupied(assume page is dirty), we need to swap out the existing page */
    {
       setSTATUS(getSTATUS() & ~0x00000001); /* Disable interrupts while updating pagetable to prevent context switching during the page replacement */
       swapPool[frameNumber].pte->entryLO = (swapPool[frameNumber].pte->entryLO & 0xFFFFFDFF); /* Valid off */ 
       TLBCLR(); /* Erase ALL the entries in the TLB to update TLB */
       devregarea_t reg = RAMBASEADDR;
       device_t flashdev = reg->devreg[FLASHINT + 8]; /*  */
       flashdev.d_data0 = physicaladdr;
       flashdev.d_command = 31 << 8 | 0x10; /* deviceBlockNumber and read block(Flash Device Command Code 2) */
       SYSCALL(5, 4, FLASHINT, TRUE); /* Perform a read operation on the flash device */   
       setSTATUS(getSTATUS() | (0x00000001)); /* Enable interrupts again after setting the status */
    }

    swapPool[frameNumber].VPN = missingPN;  /* update page p belonging to the Current Process’s ASID */
    /* swapPool[frameNumber].asid = sPtr->sup_asid;  Set the ASID of the process that owns this swap entry */
    swapPool[frameNumber].pte = &(sPtr->sup_privatePgTbl[sPtr->sup_asid]);

    /* when frame is free: perform the ssd write */
    setSTATUS(getSTATUS() & ~0x00000001); /* Disable interrupts while updating pagetable to prevent context switching during the page replacement */

    sPtr->sup_privatePgTbl[sPtr->sup_asid].entryLO = ALLOFF | (frameNumber << PFNSHIFT) | VALIDON; /* Update the Current Process’s Page Table entry for page p to indicate it is now present (V bit) */
    TLBCLR(); /* Erase ALL the entries in the TLB to update TLB */
    
    setSTATUS(getSTATUS() | (0x00000001)); /* Enable interrupts again after setting the status */

    SYSCALL(4, unsigned int &swapPoolSemaphore, 0, 0); /* mutex, Perform a V operation on the swap pool semaphore to ensure mutual exclusion */
    LDST(&(currentProcess->p_s));
}

void uTLB_RefillHandler(){
    support_t *sPtr = SYSCALL (GETSUPPORTPTR, 0, 0, 0);
    unsigned int entryHI = sPtr->sup_exceptState[0].s_entryHI; /* Get the Entry HI value from the saved state */
    missingPN = (entryHI & 0xFFFFF000) >> 12; /* Extract the missing page number from Entry HI */
    pte_entry_t entry = sPtr->sup_privatePgTbl[missingPN];  /* Get the Page Table entry for page number of the Current Process */
    /* Write this Page Table entry into the TLB */
    setENTRYHI(entry.entryHI);  
    setENTRYLO(entry.entryLO);
    TLBWR();
    LDST(&(currentProcess->p_s));   /* Return control to the Current Process to retry the instruction that caused the TLB-Refill event */
}
