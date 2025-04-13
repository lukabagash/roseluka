#include "../h/vmSupport.h"
#include "../h/initial.h"
#include "../h/types.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/* Each swap_t structure can hold info about a frame, who owns it, and which page number it corresponds to. */
HIDDEN swap_t swapPool[2 * UPROCMAX];  /* Swap Pool table */
int swapPoolSemaphore;                /* Controls mutual exclusion over swapPool */

void debugVM(int a, int b, int c, int d) {
    int x = 0;
    x++;
}

void initSwapStructs() {
    int i;
    for (i = 0; i < 2 * UPROCMAX; i++) {
        swapPool[i].asid = -1; /* Mark as free */
    }
    swapPoolSemaphore = 1;
}

void mutex(int *sem, int acquire) {
    if (acquire) {
        SYSCALL(PASSEREN, (unsigned int)sem, 0, 0);
    } else {
        SYSCALL(VERHOGEN, (unsigned int)sem, 0, 0);
    }
}

void disableInterrupts() {
    setSTATUS(getSTATUS() & IECOFF);
}
void enableInterrupts() {
    setSTATUS(getSTATUS() | IECON);
}

static void performFlashOperation(int asid, int pageBlock, int frameNo, unsigned int operation)
{
    int devIndex = (FLASHINT - OFFSET)*DEVPERINT + (asid - 1);
    int *devSem  = &p3devSemaphore[devIndex];
    int frameAddr = FRAMEPOOLSTART + (frameNo * PAGESIZE);

    devregarea_t *devReg = (devregarea_t *) RAMBASEADDR;
    device_t *flashDev   = &(devReg->devreg[devIndex]);
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
        mutex(&swapPoolSemaphore, FALSE);
        schizoUserProcTerminate(NULL); 
    }
}

void supLvlTlbExceptionHandler() 
{
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_PTR savedState = &(sPtr->sup_exceptState[0]);

    unsigned int cause    = savedState->s_cause;
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT;

    if (exc_code == TLBMODEXC) {
        ph3programTrapHandler();
    }

    unsigned int entryHI = savedState->s_entryHI;
    int missingPN = ((entryHI & VPNMASK) >> VPNSHIFT) % 32;

    static int frameNo;
    frameNo = (frameNo + 1) % (2 * UPROCMAX);

    mutex(&swapPoolSemaphore, TRUE);

    if (swapPool[frameNo].asid != -1) {
        /* occupant info */
        int occupantAsid = swapPool[frameNo].asid;
        int occupantVPN  = swapPool[frameNo].VPN;
        pte_entry_t *occPTEntry = swapPool[frameNo].pte;

        /* (a) Atomically invalidate occupant’s PTE & TLB */
        disableInterrupts();
        occPTEntry->entryLO &= VALIDOFFTLB;  /* turn off V bit */
        TLBCLR();
        enableInterrupts();

        /* (b) Write occupant’s page out */
        performFlashOperation(
            occupantAsid,       /* occupant process ID */
            occupantVPN,        /* occupant’s block number */
            frameNo,            /* frame index in swap pool */
            WRITEBLK            /* operation = write */
        );
    }

    performFlashOperation(
        sPtr->sup_asid,   /* current process ID */
        missingPN,        /* the missing page block # */
        frameNo,          /* chosen frame index */
        READBLK           /* operation = read */
    );

    swapPool[frameNo].asid = sPtr->sup_asid;
    swapPool[frameNo].VPN  = missingPN;
    swapPool[frameNo].pte  = &(sPtr->sup_privatePgTbl[missingPN]);

    /* Atomically set V + D bits in the page table entry, then TLBCLR */
    disableInterrupts();
    sPtr->sup_privatePgTbl[missingPN].entryLO = ((frameNo << PFNSHIFT) | VALIDON | DIRTYON);
    TLBCLR();
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

    TLBWR();       /* Write random entry to TLB */
    LDST(savedState);
}

void ph3programTrapHandler() {
    schizoUserProcTerminate(NULL); 
}
