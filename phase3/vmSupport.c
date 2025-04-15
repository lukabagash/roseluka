/******************************** vmSupport.c **********************************
 * 
 * Written by Rosalie Lee, Luka Bagashvili
 **************************************************************************/


#include "../h/vmSupport.h"
#include "../h/initial.h"
#include "../h/types.h"
#include "../h/sysSupport.h"
#include "../h/initProc.h"
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

void debugVM(int a, int b, int c, int d)
{
    int i = 40;
    i++;
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

static void performRW(int asid, int pageBlock, int frameAddr, unsigned int operation)
{
    int devIndex = ((FLASHINT - OFFSET) * DEVPERINT) + (asid - 1);
    int *devSem  = &p3devSemaphore[devIndex];
    devregarea_t *devReg = (devregarea_t *) RAMBASEADDR;
    device_t *flashDev = &(devReg->devreg[devIndex]);
    mutex(devSem, TRUE);
    flashDev->d_data0 = frameAddr;
    disableInterrupts(); 
    flashDev->d_command = (pageBlock << FLASCOMHSHIFT) | operation;

    SYSCALL(WAITIO, FLASHINT, (asid - 1), (operation == READBLK));

    enableInterrupts();

    int status = flashDev->d_status;
    mutex(devSem, FALSE);
    if ((operation == WRITEBLK && status == WRITEERR) || (operation == READBLK  && status == READERR))
    {
        /* Release the swap pool semaphore so we don't deadlock. */
        schizoUserProcTerminate(&swapPoolSemaphore); 
    }
}

/************************************************************************
 * TLB exception handler – the Pager:
 * handle TLB Invalid exceptions (page fault).
 ************************************************************************/
void supLvlTlbExceptionHandler() 
{
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_PTR savedState = &(sPtr->sup_exceptState[0]);

    unsigned int cause = savedState->s_cause;
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT;

    if (exc_code == TLBMODEXC) {
        ph3programTrapHandler();
    }
    mutex(&swapPoolSemaphore, TRUE);

    unsigned int entryHI = savedState->s_entryHI; /*  */
    int missingPN = ((entryHI & VPNMASK) >> VPNSHIFT) % 32; /* Hash the page number from the VPN of the missing TLB entry */

    static int frameNo;
    frameNo = (frameNo + 1) % (2 * UPROCMAX);

    int frameAddr = SWAPPOOLADDR + (frameNo * PAGESIZE);

    if (swapPool[frameNo].asid != -1) {
        /* occupant info */
        int occupantAsid = swapPool[frameNo].asid;
        int occupantVPN  = swapPool[frameNo].VPN;
        pte_entry_t *occPTEntry = swapPool[frameNo].pte;

        /* Atomically invalidate occupant’s PTE & TLB */
        disableInterrupts();
        occPTEntry->entryLO &= VALIDOFFTLB;

        /* Selectively update TLB if entry is cached */
        setENTRYHI(occPTEntry->entryHI);
        TLBP();
        if ((getINDEX() & 0x80000000) == 0) { 
            setENTRYLO(occPTEntry->entryLO);
            TLBWI();
        }
        enableInterrupts();

        /* Write occupant’s page out */
        performRW(
            occupantAsid,       /* occupant process ID */
            occupantVPN,        /* occupant’s block number */
            frameAddr,          /* frame index in swap pool */
            WRITEBLK            /* operation = write */
        );
    }

    performRW(
        sPtr->sup_asid,   /* current process ID */
        missingPN,        /* the missing page block # */
        frameAddr,          /* chosen frame index */
        READBLK           /* operation = read */
    );

    swapPool[frameNo].asid = sPtr->sup_asid;
    swapPool[frameNo].VPN  = missingPN;
    swapPool[frameNo].pte  = &(sPtr->sup_privatePgTbl[missingPN]);

    disableInterrupts();
    sPtr->sup_privatePgTbl[missingPN].entryLO = frameAddr | VALIDON | DIRTYON;

    /* Try to update existing TLB entry instead of clearing all */
    setENTRYHI(sPtr->sup_privatePgTbl[missingPN].entryHI);
    TLBP();
    debugVM(getINDEX().P, 0xDEAD, 0xDEAD, 0xDEAD);
    if ((getINDEX() & 0x80000000) == 0) {
        setENTRYLO(sPtr->sup_privatePgTbl[missingPN].entryLO);
        TLBWI();
    }
    enableInterrupts();


    mutex(&swapPoolSemaphore, FALSE);
    LDST(savedState);
}

void uTLB_RefillHandler()
{
    state_PTR savedState = (state_PTR) BIOSDATAPAGE;
    /* Find the missing page number. Typically the code is the same as in the main TLB handler. */
    int missingPN = ((savedState->s_entryHI & VPNMASK) >> VPNSHIFT) % PGTBLSIZE;

    /* We place the correct entryHI/entryLO into the TLB for that page. */
    setENTRYHI(currentProcess->p_supportStruct->sup_privatePgTbl[missingPN].entryHI);
    setENTRYLO(currentProcess->p_supportStruct->sup_privatePgTbl[missingPN].entryLO);

    TLBWR(); /* Write random entry to TLB */
    LDST(savedState);
}

void ph3programTrapHandler() {
    schizoUserProcTerminate(NULL); 
}
