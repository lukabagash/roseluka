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
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

HIDDEN swap_t swapPool[2 * UPROCMAX]; /* Swap Pool table: 2 entries for 1 page each (assuming 1 page per process) */
int swapPoolSemaphore; /* A mutual exclusion semaphore (hence initialized to 1) that controls access to the Swap Pool data structure. */

void initSwapStructs() {
    int i; /* For loop index */
    
    /* Initialize the Swap Pool table */
    for (i = 0; i < 2 * UPROCMAX; i++) {
        swapPool[i].asid = -1; /* Set the ASID to -1 indicating that the entry is free */
    }

    /* Initialize the Swap Pool semaphore */
    swapPoolSemaphore = 1; /* Set the semaphore to 1 indicating the pool is available, for mutual exclusion */
}

void mutex(int *sem, int bool) {
    if (bool == TRUE){ /* gain mutual exclusion, now having exclusive access */
		SYSCALL(PASSEREN, (unsigned int) sem, 0, 0); 
	}
	else{ /* lose mutual exclusion, allowing others to access */
		SYSCALL(VERHOGEN, (unsigned int) sem, 0, 0); 
	}
}

void supLvlTlbExceptionHandler() {
    /* 14 steps in [Section 4.4.2] */
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0); /* Get the pointer to the Current Process’s Support Structure */
    unsigned int cause = sPtr->sup_exceptState[0].s_cause; /* Determine the cause of the TLB exception */
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT; /* Extract the exception code from the cause register */
    unsigned int entryHI = sPtr->sup_exceptState[0].s_entryHI; /* Get the Entry HI value from the saved state */
    int missingPN = (entryHI & VPNMASK) >> VPNSHIFT; /* Extract the missing page number from Entry HI */
    static int frameNumber; /* Frame number to be used for the page replacement */
    frameNumber = (frameNumber + 1) % (2 * UPROCMAX); /* Simple page replacement algorithm: round-robin replacement for the sake of example */

    /* TLB-Modification Exception - a store instruction tries to write to a page */
    if(exc_code == TLBEXCPT) { 
        programTrapHandler(); /* Handle the TLB modification exception by invoking the program trap handler */
    }

    mutex((int *) &swapPoolSemaphore, TRUE); /* Perform a P operation on the swap pool semaphore to gain mutual exclusion */
    int dnum = 1; /* temporary placemholder until I figure out */
    int frameAddr = frameNumber * PAGESIZE + FRAMEPOOLSTART; /* Starting address of current frame number */
    int flashId = (FLASHINT - OFFSET) * DEVPERINT + dnum; /* Device number for the flash device */
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR; /* Get the device register area base address */
    device_t flashdev = reg->devreg[flashId];
    flashdev.d_data0 = frameAddr;
    if(swapPool[frameNumber].asid != -1) /* If the frame is already occupied(assume page is dirty), we need to swap out the existing page */
    {
       setSTATUS(getSTATUS() & IECOFF); /* Disable interrupts while updating pagetable to prevent context switching during the page replacement */
       swapPool[frameNumber].pte->entryLO = (swapPool[frameNumber].pte->entryLO & VALIDOFFTLB); /* Valid off */ 
       TLBCLR(); /* Erase ALL the entries in the TLB to update TLB */
       flashdev.d_command = frameNumber << FLASCOMHSHIFT | WRITEBLK; /* Write the contents of frame to the correct location on process backing store/flash device. */
       SYSCALL(WAITIO, FLASHINT, dnum, FALSE); /* Perform write operation on the flash device */
       if(flashdev.d_status == WRITEERR){
            schizoUserProcTerminate(&swapPoolSemaphore); /* Handle the error if the write operation failed */
       }   
       setSTATUS(getSTATUS() | IECON); /* Enable interrupts again after setting the status */
    }
    flashdev.d_command = frameNumber << FLASCOMHSHIFT | READBLK; /* Write the contents of frame to the correct location on process backing store/flash device. */
    SYSCALL(WAITIO, FLASHINT, dnum, TRUE); /* Perform write operation on the flash device */
    if (flashdev.d_status == READERR) {
        schizoUserProcTerminate(&swapPoolSemaphore);
    } /* Handle the error if the read operation failed */

    swapPool[frameNumber].VPN = missingPN;  /* update page p belonging to the Current Process’s ASID */
    swapPool[frameNumber].asid = sPtr->sup_asid;  /* Set the ASID of the process that owns this swap entry */
    swapPool[frameNumber].pte = &(sPtr->sup_privatePgTbl[missingPN]); /* Set the pointer to the page table entry associated with this swap entry */

    setSTATUS(getSTATUS() & IECOFF); /* Disable interrupts while updating pagetable to prevent context switching during the page replacement */

    sPtr->sup_privatePgTbl[missingPN].entryLO = (frameNumber << PFNSHIFT) | VALIDON; /* Update the Current Process’s Page Table entry for page p to indicate it is now present (V bit) */
    TLBCLR(); /* Erase ALL the entries in the TLB to update TLB */
    
    setSTATUS(getSTATUS() | IECON); /* Enable interrupts again after setting the status */

    mutex((int *) &swapPoolSemaphore, FALSE); /* mutex, Perform a V operation on the swap pool semaphore to ensure mutual exclusion */
    LDST(&(currentProcess->p_s));
}

void uTLB_RefillHandler(){
    support_t *sPtr = SYSCALL (GETSUPPORTPTR, 0, 0, 0);
    unsigned int entryHI = sPtr->sup_exceptState[0].s_entryHI; /* Get the Entry HI value from the saved state */
    int missingPN = (entryHI & VPNMASK) >> VPNSHIFT; /* Extract the missing page number from Entry HI */
    pte_entry_t entry = sPtr->sup_privatePgTbl[missingPN];  /* Get the Page Table entry for page number of the Current Process */
    /* Write this Page Table entry into the TLB */
    setENTRYHI(entry.entryHI);  
    setENTRYLO(entry.entryLO);
    TLBWR();
    LDST(&(currentProcess->p_s));   /* Return control to the Current Process to retry the instruction that caused the TLB-Refill event */
}

void programTrapHandler(){
    schizoUserProcTerminate(NULL); /* Terminate the current process if it encounters a program trap */
}