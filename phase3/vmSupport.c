#include "../h/vmSupport.h"
#include "../h/initial.h"
#include "../h/types.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

HIDDEN swap_t swapPool[2 * UPROCMAX]; /* Swap Pool table: 2 entries for 1 page each (assuming 1 page per process) */
int swapPoolSemaphore; /* A mutual exclusion semaphore (hence initialized to 1) that controls access to the Swap Pool data structure. */
void debugVM(int a, int b, int c, int d) {
    /* Debugging function to print values */
    int i;
    i = 42;
    i++;
}
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
    debugVM(0x1, 0xBADBABE, 0xBEEF, 0xDEADBEEF);
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0); /* Get the pointer to the Current Process’s Support Structure */
    unsigned int cause = sPtr->sup_exceptState[0].s_cause; /* Determine the cause of the TLB exception */
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT; /* Extract the exception code from the cause register */
    unsigned int entryHI = sPtr->sup_exceptState[0].s_entryHI; /* Get the Entry HI value from the saved state */
    int missingPN = (entryHI & VPNMASK) >> VPNSHIFT; /* Extract the missing page number from Entry HI */
    missingPN = missingPN % 32; /* hash function since 32 entries per page, page number of the missing TLB entry */
    static int frameNumber; /* Frame number to be used for the page replacement */
    frameNumber = (frameNumber + 1) % (2 * UPROCMAX); /* Simple page replacement algorithm: round-robin replacement for the sake of example */
    debugVM(0x1, sPtr->sup_exceptState[0].s_cause, sPtr->sup_exceptState[0].s_entryHI, (entryHI & VPNMASK) >> VPNSHIFT);
    /* TLB-Modification Exception - a store instruction tries to write to a page */
    if(exc_code == TLBEXCPT) { 
        ph3programTrapHandler(); /* Handle the TLB modification exception by invoking the program trap handler */
    }

    mutex((int *) &swapPoolSemaphore, TRUE); /* Perform a P operation on the swap pool semaphore to gain mutual exclusion */
    int dnum = sPtr->sup_asid - 1; /*Each U-proc is associated with its own flash and terminal device. The ASID uniquely identifies the process and by extension, its devices*/
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
            schizoUserProcTerminate(&swapPoolSemaphore); /* Use sys9 here to also perform mutex that we never got to*/
       }   
       setSTATUS(getSTATUS() | IECON); /* Enable interrupts again after setting the status */
    }
    flashdev.d_command = frameNumber << FLASCOMHSHIFT | READBLK; /* Write the contents of frame to the correct location on process backing store/flash device. */
    SYSCALL(WAITIO, FLASHINT, dnum, TRUE); /* Perform write operation on the flash device */
    if (flashdev.d_status == READERR) {
        schizoUserProcTerminate(&swapPoolSemaphore); /* Use sys9 here to also perform mutex that we never got to*/
    }

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
    state_PTR savedState = (state_PTR) BIOSDATAPAGE;
    int missingPN = ((savedState->s_entryHI & VPNMASK) >> VPNSHIFT) % PGTBLSIZE; /* Extract the missing page number from Entry HI */

    setENTRYHI(currentProcess->p_supportStruct->sup_privatePgTbl[missingPN].entryHI);
    setENTRYLO(currentProcess->p_supportStruct->sup_privatePgTbl[missingPN].entryLO);

    TLBWR();
    debugVM(0x3, 0, 0, 0);
    LDST(savedState);   /* Return control to the Current Process to retry the instruction that caused the TLB-Refill event */
}

void ph3programTrapHandler(){
    debugVM(0x4, 0, 0, 0);
    schizoUserProcTerminate(NULL); /* Terminate the current process if it encounters a program trap */
}