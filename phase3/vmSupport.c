#include "../h/vmSupport.h"
#include "../h/initial.h"
#include "../h/types.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/* Each swap_t structure can hold info about a frame, who owns it, and which page number it corresponds to. */
HIDDEN swap_t swapPool[2 * UPROCMAX];  /* Swap Pool table */
int swapPoolSemaphore;                /* Controls mutual exclusion over swapPool */

void debugVM(int a, int b, int c, int d) {
    /* Debugging function to print values */
    int i;
    i = 42;
    i++;
}

void initSwapStructs() {
    /* Initialize the Swap Pool table */
    int i;
    for (i = 0; i < 2 * UPROCMAX; i++) {
        swapPool[i].asid = -1; /* Mark as free */
    }

    /* Initialize the semaphore (used for mutual exclusion on the swap pool) */
    swapPoolSemaphore = 1;
}

/* Simple wrapper to acquire/release the swapPool semaphore */
void mutex(int *sem, int acquire) {
    if (acquire == TRUE) {
        SYSCALL(PASSEREN, (unsigned int)sem, 0, 0);  /* P operation */
    } else {
        SYSCALL(VERHOGEN, (unsigned int)sem, 0, 0);  /* V operation */
    }
}

/* ----------------------------------------------------------------------------
 * Helper: disableInterrupts() / enableInterrupts()
 *   Turn off or on the IE bit in the Status register (Pandos 4.5.3).
 *   This ensures TLB updates and Page Table changes happen atomically.
 * ---------------------------------------------------------------------------*/
static inline void disableInterrupts() {
    setSTATUS(getSTATUS() & IECOFF);  /* Clear the IE bit */
}

static inline void enableInterrupts() {
    setSTATUS(getSTATUS() | IECON);   /* Set the IE bit */
}

/* ----------------------------------------------------------------------------
 * Helper: occupantSwapOut()
 *   - If frame <frameNo> is occupied, write occupant's page back to occupant's 
 *     backing store (Steps 7–8 in *Pandos*, Section 4.4.2).
 *   - occupant’s device is identified by occupantAsid; occupant’s block = occupantVPN.
 *   - Occupant’s page table is invalidated atomically with TLBCLR (4.5.3).
 * ---------------------------------------------------------------------------*/
static void occupantSwapOut(int frameNo) {
    if (swapPool[frameNo].asid == -1) {
        /* Frame is free; nothing to swap out */
        return;
    }

    /* occupant info */
    int occupantAsid = swapPool[frameNo].asid;
    int occupantVPN  = swapPool[frameNo].VPN;
    pte_entry_t *occPTEntry = swapPool[frameNo].pte;

    /* occupant’s flash device index (Pandos 4.5.1) */
    int occupantDevNum = (FLASHINT - OFFSET) * DEVPERINT + (occupantAsid - 1);

    /* occupant’s frame physical address */
    int frameAddr = FRAMEPOOLSTART + frameNo * PAGESIZE;

    /* Access occupant’s flash device register */
    devregarea_t *devReg = (devregarea_t *) RAMBASEADDR;
    device_t *flashDev   = &(devReg->devreg[occupantDevNum]);

    /* (a) Atomically invalidate occupant's page table entry & TLB */
    disableInterrupts();  
    occPTEntry->entryLO &= VALIDOFFTLB;  /* Turn off V bit */
    TLBCLR();
    enableInterrupts();

    /* (b) Write occupant’s page out to occupant’s block number = occupantVPN */
    flashDev->d_data0 = frameAddr;
    flashDev->d_command = (occupantVPN << FLASCOMHSHIFT) | WRITEBLK;
    SYSCALL(WAITIO, FLASHINT, (occupantAsid - 1), FALSE);

    /* Check for write error */
    if (flashDev->d_status == WRITEERR) {
        schizoUserProcTerminate(&swapPoolSemaphore); 
    }
}

/* ----------------------------------------------------------------------------
 * Helper: newPageSwapIn()
 *   - Read the newly missing page from the current process’s backing store 
 *     into frame <frameNo> (Step 9 in *Pandos*, Section 4.4.2).
 * ---------------------------------------------------------------------------*/
static void newPageSwapIn(support_t *sPtr, int frameNo, int missingPN) {
    /* current process info */
    int curAsid = sPtr->sup_asid;
    int curDevNum = (FLASHINT - OFFSET) * DEVPERINT + (curAsid - 1);

    /* frame's physical address */
    int frameAddr = FRAMEPOOLSTART + frameNo * PAGESIZE;

    /* device register for current process's flash dev */
    devregarea_t *devReg = (devregarea_t *) RAMBASEADDR;
    device_t *flashDev   = &(devReg->devreg[curDevNum]);

    /* read from block = missingPN (Step 9) */
    flashDev->d_data0 = frameAddr;
    flashDev->d_command = (missingPN << FLASCOMHSHIFT) | READBLK;
    SYSCALL(WAITIO, FLASHINT, (curAsid - 1), TRUE);

    /* Check for read error */
    if (flashDev->d_status == READERR) {
        schizoUserProcTerminate(&swapPoolSemaphore);
    }
}

/* ----------------------------------------------------------------------------
 * Helper: updateSwapPoolAndPageTable()
 *   - Update swapPool entry with the new occupant info (Steps 10–11).
 *   - Mark page as valid and dirty (ensuring R/W access).
 *   - Atomically update TLB (Step 12).
 * ---------------------------------------------------------------------------*/
static void updateSwapPoolAndPageTable(support_t *sPtr, int frameNo, int missingPN) {
    swapPool[frameNo].VPN  = missingPN;
    swapPool[frameNo].asid = sPtr->sup_asid;
    swapPool[frameNo].pte  = &(sPtr->sup_privatePgTbl[missingPN]);

    /* Atomically set the new page table entry as valid & dirty, then TLBCLR */
    disableInterrupts();
    sPtr->sup_privatePgTbl[missingPN].entryLO = ((frameNo << PFNSHIFT) | VALIDON | DIRTYON);
    TLBCLR();
    enableInterrupts();
}

/* ----------------------------------------------------------------------------
 * Main TLB-Exception Handler (Level 4, Section 4.4.2)
 * ---------------------------------------------------------------------------*/
void supLvlTlbExceptionHandler() {
    debugVM(0x1, 0xBADBABE, 0xBEEF, 0xDEADBEEF);

    /* (1) Get pointer to Support Structure (SYS8) */
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_PTR savedState = &(sPtr->sup_exceptState[0]);

    /* (2) Determine cause (TLBL / TLBS / Mod) */
    unsigned int cause    = savedState->s_cause;
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT;

    /* (3) If TLB-Modification, treat as program trap (pandos 4.8) */
    if (exc_code == TLBMODEXC) {
        ph3programTrapHandler();
    }

    /* (5) Determine the missing page number */
    unsigned int entryHI  = savedState->s_entryHI;
    int missingPN = ((entryHI & VPNMASK) >> VPNSHIFT) % 32; 

    /* (6) Choose a frame from the Swap Pool; simple round-robin here */
    static int frameNo;
    frameNo = (frameNo + 1) % (2 * UPROCMAX);

    /* (4) Gain mutual exclusion over Swap Pool table */
    mutex(&swapPoolSemaphore, TRUE);

    /* (7–8) If occupied, write occupant out */
    occupantSwapOut(frameNo);

    /* (9) Read new occupant’s page into the selected frame */
    newPageSwapIn(sPtr, frameNo, missingPN);

    /* (10–12) Update Swap Pool & Page Table, TLB flush */
    updateSwapPoolAndPageTable(sPtr, frameNo, missingPN);

    /* (13) Release mutual exclusion */
    mutex(&swapPoolSemaphore, FALSE);

    /* (14) LDST back to user to retry instruction */
    LDST(savedState);
}

/* uTLB-Refill Handler */
void uTLB_RefillHandler() {
    state_PTR savedState = (state_PTR) BIOSDATAPAGE;
    int missingPN = ((savedState->s_entryHI & VPNMASK) >> VPNSHIFT) % PGTBLSIZE; /* Extract the missing page number from Entry HI */

    setENTRYHI(currentProcess->p_supportStruct->sup_privatePgTbl[missingPN].entryHI);
    setENTRYLO(currentProcess->p_supportStruct->sup_privatePgTbl[missingPN].entryLO);

    TLBWR();
    debugVM(0x3, 0, 0, 0);
    LDST(savedState);
}

/* Program Trap Handler */
void ph3programTrapHandler() {
    debugVM(0x4, 0, 0, 0);
    schizoUserProcTerminate(NULL); 
}
