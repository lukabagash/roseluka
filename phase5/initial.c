/******************************** initial.c **********************************
 *
 * This file sets up the Pandos Nucleus (Phase 2). It:
 *   1. Defines and initializes Phase 2 global variables.
 *   2. Initializes the Pass Up Vector for Processor 0.
 *   3. Creates an initial process (to run test()).
 *   4. Provides a general exception handler that delegates:
 *      - Interrupts to the device interrupt handler.
 *      - TLB exceptions to the TLB handler.
 *      - Program traps to the Program Trap handler.
 *      - SYSCALL exceptions to the SYSCALL handler.
 *
 *  Written by Rosalie Lee, Luka Bagashvili
 **************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/vmSupport.h"
#include "../h/delayDaemon.h"
#include "/usr/include/umps3/umps/libumps.h"

/* External function declarations */
extern void test();              /* The "main" test function (see p2test.c) */
HIDDEN void genExceptionHandler();/* Internal function for all general exceptions */

/* Global variables for Phase 2 */
pcb_PTR readyQueue;      /* Tail pointer to the "ready" queue of pcbs */
pcb_PTR currentProcess;  /* Pointer to the currently running process */
int processCount;        /* Number of created but not yet terminated processes */
int softBlockedCount;    /* Number of created but not yet terminated processes 
                            in a blocked (waiting) state */
int devSemaphore[MAXDEVICECNT];  /* One semaphore per external device + 1 pseudo-clock */
cpu_t startTOD;                  /* Time-of-day value when currentProcess starts */
state_PTR savedExceptState;      /* Saved exception state pointer */


/************************************************************************
 * genExceptionHandler - Handles all exceptions except TLB-refill
 *
 *   - Interrupts go to the device interrupt handler (intTrapH()).
 *   - TLB exceptions go to the TLB handler (tlbTrapH()).
 *   - SYSCALL exceptions go to sysTrapH().
 *   - Program traps go to pgmTrapH().
 ************************************************************************/
HIDDEN void genExceptionHandler() {
    state_t *oldState;
    int exCode;
    
    /* Get saved exception state from BIOS Data Page */
    oldState = (state_t *) BIOSDATAPAGE;
    
    /* Extract the exception code */
    exCode = ((oldState->s_cause) & PANDOS_CAUSEMASK) >> EXCCODESHIFT;

    if (exCode == INTEXCPT) {
        /* Handle device/timer interrupts */
        intTrapH();
    } 
    else if (exCode <= TLBEXCPT) {
        /* Handle TLB exceptions (1..3) */
        tlbExceptionHandler();
    } 
    else if (exCode == SYSCALLEXCPT) {
        /* Handle SYSCALL exceptions (8) */
        syscallExceptionHandler();
    } 
    else {
        /* All other exceptions: program traps, etc. */
        programTrapHandler();
    }
}

/************************************************************************
 * main - Pandos Nucleus entry point for Phase 2
 *
 *   1. Set Pass Up Vector for Processor 0 (TLB-refill and general exceptions).
 *   2. Init Phase 1 data structures (pcbFree list & ASL).
 *   3. Initialize Phase 2 global variables.
 *   4. Load system-wide Interval Timer (e.g., 100 ms).
 *   5. Create a single test process and start the scheduler.
 ************************************************************************/
int main() {
    pcb_PTR p;
    passupvector_t *passUpPtr;
    memaddr ramTop;
    devregarea_t *deviceRegArea;
    int i;

    /* Set Pass Up Vector */
    passUpPtr = (passupvector_t *) PASSUPVECTOR;
    passUpPtr->tlb_refll_handler = (memaddr) uTLB_RefillHandler;
    passUpPtr->tlb_refll_stackPtr = NUCLEUSSTACK;
    passUpPtr->exception_handler = (memaddr) genExceptionHandler;
    passUpPtr->exception_stackPtr = NUCLEUSSTACK;

    /* Init Phase 1 structures (pcbFree list & ASL) */
    initPcbs();
    initASL();


    /* Init Phase 2 global variables */
    processCount = INITPROCCOUNT;
    softBlockedCount = INITSOFTBLKCOUNT;
    readyQueue = mkEmptyProcQ();
    currentProcess = NULL;

    for (i = 0; i < MAXDEVICECNT; i++) {
        devSemaphore[i] = DEVSEMINIT;
    }

    /* Load system-wide Interval Timer (100 ms) */
    LDIT(PANDOS_CLOCKINTERVAL);

    /* Create a single process to start the scheduler */
    p = allocPcb();
    if (p != NULL) {
        /* Get top of RAM */
        deviceRegArea = (devregarea_t *) RAMBASEADDR;
        ramTop = deviceRegArea->rambase + deviceRegArea->ramsize;

        /* Initialize p's processor state */ 
        p->p_s.s_status = ALLOFF | PANDOS_IEPBITON | TEBITON | PANDOS_CAUSEINTMASK;   /* Enabling interrupts and PLT, and setting kernel-mode to on */
        p->p_s.s_sp = ramTop;
        p->p_s.s_pc = (memaddr) initADL;
        p->p_s.s_t9 = (memaddr) initADL;

        insertProcQ(&readyQueue, p);
        processCount++;

        /* Begin scheduling */
        switchProcess();
        return 0; /* Not reached if scheduler runs properly */
    }

    PANIC(); /* If no PCB is available, panic */
    return 0;
}
