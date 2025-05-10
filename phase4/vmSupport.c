/******************************** vmSupport.c **********************************
 * This file implements paging for user processes, managing frames in a swap pool. 
 * On page faults (TLB invalid), it either replace an existing occupant to flash 
 * or loads a new page in from flash. 
 * Also updates the TLB entries and page table entries for user-mode virtual memory.
 * 
 * Written by Rosalie Lee, Luka Bagashvili
 **************************************************************************/


#include "../h/vmSupport.h"
#include "../h/initial.h"
#include "../h/types.h"
#include "../h/sysSupport.h"
#include "../h/initProc.h"
#include "../h/deviceSupportDMA.h" /* flashOperation() */
#include "/usr/include/umps3/umps/libumps.h"

/* Each swap_t structure can hold info about a frame, who owns it, and which page number it corresponds to. */
HIDDEN swap_t swapPool[2 * UPROCMAX];  /* Swap Pool table */
int swapPoolSemaphore;                /* Controls mutual exclusion over swapPool */

/************************************************************************
 * Helper Function
 * Initialize both the Swap Pool table and accompanying semaphore.
 ************************************************************************/
void initSwapStructs() {
    int i;
    for (i = 0; i < 2 * UPROCMAX; i++) {
        swapPool[i].asid = -1; /* Mark as free */
    }
    swapPoolSemaphore = 1;
}

/************************************************************************
 * Helper Function
 * Surgically update TLB.
 ************************************************************************/
void updateTLBIfCached(unsigned int entryHI, unsigned int *entryLOptr, unsigned int newEntryLO) {
    disableInterrupts();

    *entryLOptr = newEntryLO;

    setENTRYHI(entryHI);
    TLBP();
    if ((getINDEX() & INDEXPMASK) == 0) {
        setENTRYLO(entryLOptr ? *entryLOptr : newEntryLO);
        TLBWI();
    }

    enableInterrupts();
}

/************************************************************************
 * Helper Function
 * Gain/Release mutual exclusion from the designated semaphore.
 ************************************************************************/
void mutex(int *sem, int acquire) {
    /* If 1 is passed into the acquire, call SYS3 to gain mutex */
    if (acquire) {
        SYSCALL(PASSEREN, (unsigned int)sem, 0, 0);
    } 
    /* If 0 is passed into the acquire, call SYS4 to release mutex */
    else {
        SYSCALL(VERHOGEN, (unsigned int)sem, 0, 0);
    }
}

/************************************************************************
 * Helper Functions
 * Disable/Enable interrupts to process atomically.
 ************************************************************************/
void disableInterrupts() {
    setSTATUS(getSTATUS() & IECOFF);
}
void enableInterrupts() {
    setSTATUS(getSTATUS() | IECON);
}

/************************************************************************
 * TLB exception handler – the Pager: handles TLB Invalid exceptions 
 * (page fault) of user process.
 ************************************************************************/
void supLvlTlbExceptionHandler() {
    int st;

    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0); /* Current process's support struct */
    state_PTR savedState = &(sPtr->sup_exceptState[0]);

    unsigned int cause = savedState->s_cause;
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT;

    if (exc_code == TLBMODEXC) {
        ph3programTrapHandler();
    }
    mutex(&swapPoolSemaphore, TRUE);

    unsigned int entryHI = savedState->s_entryHI;
    int missingPN = ((entryHI & VPNMASK) >> VPNSHIFT) % PGTBLSIZE; /* Hash the page number from the VPN of the missing TLB entry */

    static int frameNo;
    frameNo = (frameNo + 1) % (2 * UPROCMAX);

    int frameAddr = SWAPPOOLADDR + (frameNo * PAGESIZE);

    if (swapPool[frameNo].asid != -1) {
        /* occupant info */
        int occupantAsid = swapPool[frameNo].asid;
        int occupantVPN  = swapPool[frameNo].VPN;
        pte_entry_t *occPTEntry = swapPool[frameNo].pte;

        updateTLBIfCached(occPTEntry->entryHI, &occPTEntry->entryLO, occPTEntry->entryLO & VALIDOFFTLB);

        /* Write occupant’s page out */
        st = flashOperation(
            occupantAsid,       /* occupant process ID */
            occupantVPN,        /* occupant’s block number */
            frameAddr,          /* frame index in swap pool */
            WRITEBLK            /* operation = write */
        );
        if (st != DEVREDY) { /* Added status check here due to flashOperation modification */
            /* Release the swap pool semaphore so we don't deadlock */
            schizoUserProcTerminate(&swapPoolSemaphore); 
        }
    }

    st = flashOperation(
        sPtr->sup_asid,   /* current process ID */
        missingPN,        /* the missing page block # */
        frameAddr,          /* chosen frame index */
        READBLK           /* operation = read */
    );
    if (st != DEVREDY) { /* Added status check here due to flashOperation modification */
        /* Release the swap pool semaphore so we don't deadlock */
        schizoUserProcTerminate(&swapPoolSemaphore); 
    }

    swapPool[frameNo].asid = sPtr->sup_asid;
    swapPool[frameNo].VPN  = missingPN;
    swapPool[frameNo].pte  = &(sPtr->sup_privatePgTbl[missingPN]);

    updateTLBIfCached(
        sPtr->sup_privatePgTbl[missingPN].entryHI,
        &sPtr->sup_privatePgTbl[missingPN].entryLO, /* pass by reference update of page table*/
        frameAddr | VALIDON | DIRTYON
    );


    mutex(&swapPoolSemaphore, FALSE);
    LDST(savedState);
}

/************************************************************************
 * This function inserts the correct page table entry into the TLB when 
 * there's a TLB miss (refill event). 
 ************************************************************************/
void uTLB_RefillHandler() {
    state_PTR savedState = (state_PTR) BIOSDATAPAGE;
    /* Find the missing page number. Typically the code is the same as in the main TLB handler. */
    int missingPN = ((savedState->s_entryHI & VPNMASK) >> VPNSHIFT) % PGTBLSIZE;

    /* We place the correct entryHI/entryLO into the TLB for that page. */
    setENTRYHI(currentProcess->p_supportStruct->sup_privatePgTbl[missingPN].entryHI);
    setENTRYLO(currentProcess->p_supportStruct->sup_privatePgTbl[missingPN].entryLO);

    TLBWR(); /* Write random entry to TLB */
    LDST(savedState);
}

/************************************************************************
 * This function terminates the current user process if a TLB modification 
 * exception or other program trap occurs in user-mode.
 ************************************************************************/
void ph3programTrapHandler() {
    schizoUserProcTerminate(NULL); 
}
