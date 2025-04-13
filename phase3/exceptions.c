/******************************** exceptions.c **********************************
 *
 * This module implements the Nucleus exception handling for Pandos. It directly
 * handles SYSCALL (1–8) requests, while all other exceptions (TLB, Program Trap,
 * or SYSCALL ≥ 9) are “passed up” to the Support Level if a Support Structure is 
 * defined, or the offending process (and its progeny) is terminated otherwise.
 *
 * In particular, this module:
 *   - Defines an entry point for SYSCALL exceptions (`syscallExceptionHandler`),
 *     which dispatches to specific handlers for SYS1–SYS8.
 *   - Defines handlers for Program Trap (`programTrapHandler`) and TLB exceptions
 *     (`tlbExceptionHandler`).
 *   - Provides the Pass Up or Die mechanism (`passUpOrDie`) for Program Trap,
 *     TLB, and illegal SYSCALL requests.
 *   - Supplies internal helper functions to manage new process creation, 
 *     process termination, Passeren/Verhogen, I/O waits, retrieving CPU time,
 *     waiting for the pseudo-clock, and retrieving a process’s Support Structure.
 *
 * Execution Flow:
 *   - The General Exception Handler (from `initial.c`) decodes `Cause.ExcCode` 
 *     and calls into one of the three entry points here: `syscallExceptionHandler`, 
 *     `programTrapHandler`, or `tlbExceptionHandler`.
 *   - If the exception was a valid SYSCALL [1..8], the appropriate routine is 
 *     invoked (e.g., createProcess). Otherwise, or if the calling process was 
 *     in user-mode, the request is handled as a Program Trap exception (illegal).
 *   - Certain SYSCALLs (SYS3, SYS5, SYS7) may block the calling process, after 
 *     which the Scheduler is invoked.
 *   - At the end of any non-blocking SYSCALL handling, this module updates CPU 
 *     usage for the Current Process and resumes it via LDST.
 *
 * Written by: Luka Bagashvili, Rosalie Lee
 ******************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initial.h"
#include "/usr/include/umps3/umps/libumps.h"

/* Function Prototypes (local to this module) */
HIDDEN void blockCurrentProcess(int *semAddr);
HIDDEN void createNewProcess(state_PTR stateSys, support_t *supportPtr);
HIDDEN void terminateProcessAndProgeny(pcb_PTR proc);
HIDDEN void passerenSyscall(int *semAddr);
HIDDEN void verhogenSyscall(int *semAddr);
HIDDEN void waitIODevice(int lineNum, int devNum, int isReadOperation);
HIDDEN void getCpuTimeSyscall();
HIDDEN void waitForClockSyscall();
HIDDEN void getSupportDataSyscall();

/* Global Variables (from this module’s perspective) */
int   syscallNumber;   /* Holds the system call code (a0) from the saved state */
cpu_t currentTOD;      /* Used to track CPU usage for the Current Process */


/************************************************************************
 * updateCurrentProcessState
 * 
 * Copies the saved CPU state (from the BIOS Data Page) into the Current 
 * Process’s PCB. This is typically used after an exception is raised, so 
 * that the Current Process has an up-to-date processor state in its PCB.
 ************************************************************************/
void updateCurrentProcessState() {
    moveState(savedExceptState, &(currentProcess->p_s));
}

void debugExc(int a, int b, int c, int d) {
    /* Debugging function to print values */
    int i;
    i = 42;
    i++;
}


/************************************************************************
 * blockCurrentProcess
 *
 * Blocks the Current Process on the given semaphore. Updates the 
 * accumulated CPU time for the Current Process, inserts the process in 
 * the ASL, and clears `currentProcess` so the Scheduler may find a new job.
 ************************************************************************/
HIDDEN void blockCurrentProcess(int *semAddr) {
    STCK(currentTOD);
    currentProcess->p_time += (currentTOD - startTOD);

    insertBlocked(semAddr, currentProcess);
    currentProcess = NULL; 
}


/************************************************************************
 * createNewProcess (SYS1 - CREATEPROCESS)
 *
 * Creates a new process by allocating a PCB, initializing its state from 
 * the state pointer provided in a1, and optionally assigning it a Support
 * Structure (a2). The new process is placed on the Ready Queue and made 
 * a child of the Current Process. If no PCB is available, returns -1 in 
 * v0; otherwise returns 0.
 ************************************************************************/
HIDDEN void createNewProcess(state_PTR stateSys, support_t *supportPtr) {
    debugExc(0x60D, 0x60D, 0x60D, 0x60D);
    pcb_PTR newPcb = allocPcb();
    if (newPcb != NULL) {
        debugExc(4,4,4,4);
        /* Populate fields of the new PCB */
        debugExc(0x60d, stateSys->s_entryHI, 0x60d, 0x60d);
        moveState(stateSys, &(newPcb->p_s));
        debugExc(0x60d, newPcb->p_s.s_entryHI, 0x60d, 0x60d);
        newPcb->p_supportStruct = supportPtr;
        insertProcQ(&readyQueue, newPcb);

        insertChild(currentProcess, newPcb);

        newPcb->p_time   = INITIALACCTIME;
        newPcb->p_semAdd = NULL;

        processCount++;
        currentProcess->p_s.s_v0 = OK;  /* success */
    } else {
        debugExc(9,9,9,9);
        currentProcess->p_s.s_v0 = FAIL; /* error (no more PCBs) */
    }

    /* Charge the time spent handling this SYSCALL to the Current Process */
    STCK(currentTOD);
    currentProcess->p_time += (currentTOD - startTOD);

    /* Resume execution of the Current Process */
    loadProcessorState(currentProcess);
}


/************************************************************************
 * terminateProcessAndProgeny (SYS2 - TERMINATEPROCESS or “Die”)
 *
 * Recursively terminates the specified process and all of its offspring. 
 * Removes the process from either the Ready Queue or ASL (if blocked), 
 * releases its PCB, and decrements the process count. If the process was 
 * blocked on a non-device semaphore, increments that semaphore.
 ************************************************************************/
HIDDEN void terminateProcessAndProgeny(pcb_PTR proc) {
    int *semAddr = proc->p_semAdd;
    
    /* Recursively kill all children */
    while (!emptyChild(proc)) {
        terminateProcessAndProgeny(removeChild(proc));
    }

    /* Now remove this process itself */
    if (proc == currentProcess) {
        outChild(proc);
    } 
    else if (semAddr != NULL) {
        outBlocked(proc);
        /* If it wasn’t a device semaphore, increment it. Otherwise, 
           device semaphores get incremented by the eventual device interrupt. */
        if (!(semAddr >= &devSemaphore[FIRSTDEVINDEX] && 
              semAddr <= &devSemaphore[PCLOCKIDX])) {
            (*semAddr)++;
        } else {
            softBlockedCount--;
        }
    } else {
        /* Must be on the Ready Queue */
        outProcQ(&readyQueue, proc);
    }

    freePcb(proc);
    processCount--;
}


/************************************************************************
 * passerenSyscall (SYS3 - PASSEREN)
 *
 * Performs a P operation on the semaphore pointed to by a1. If the 
 * semaphore is negative after decrement, the Current Process becomes 
 * blocked and the Scheduler is invoked. Otherwise, control resumes.
 ************************************************************************/
HIDDEN void passerenSyscall(int *semAddr) {
    (*semAddr)--;
    if ((*semAddr) < SEMA4THRESH) {
        blockCurrentProcess(semAddr);
        switchProcess();
    }

    /* Non-blocking: charge CPU time and resume */
    STCK(currentTOD);
    currentProcess->p_time += (currentTOD - startTOD);
    loadProcessorState(currentProcess);
}


/************************************************************************
 * verhogenSyscall (SYS4 - VERHOGEN)
 *
 * Performs a V operation on the semaphore pointed to by a1. If this V 
 * unblocks a waiting process, that process is moved from the ASL to the 
 * Ready Queue. Control resumes with the Current Process.
 ************************************************************************/
HIDDEN void verhogenSyscall(int *semAddr) {
    (*semAddr)++;
    if ((*semAddr) <= SEMA4THRESH) {
        pcb_PTR unblocked = removeBlocked(semAddr);
        if (unblocked != NULL) {
            insertProcQ(&readyQueue, unblocked);
        }
    }

    /* Charge CPU time and resume */
    STCK(currentTOD);
    currentProcess->p_time += (currentTOD - startTOD);
    loadProcessorState(currentProcess);
}


/************************************************************************
 * waitIODevice (SYS5 - WAITIO)
 *
 * Requests synchronous I/O on the specified device line (a1), device 
 * number (a2), and sub-device type (a3 indicates TRUE if it is a terminal
 * read). This call always blocks the Current Process. The device’s status 
 * is later placed in the unblocked process’s v0 register by the interrupt
 * handler.
 ************************************************************************/
HIDDEN void waitIODevice(int lineNum, int devNum, int isReadOperation) {
    int devIndex = (lineNum - OFFSET) * DEVPERINT + devNum;

    if (lineNum == LINE7 && (isReadOperation == FALSE)) {
        /* Terminal write sub-device is offset by DEVPERINT from read sub-device */
        devIndex += DEVPERINT;
    }

    softBlockedCount++;
    devSemaphore[devIndex]--;
    blockCurrentProcess(&devSemaphore[devIndex]);
    switchProcess();  /* Never returns here */
}


/************************************************************************
 * getCpuTimeSyscall (SYS6 - GETCPUTIME)
 *
 * Places the accumulated CPU time of the Current Process (including time 
 * since last dispatch) into v0. Then resumes execution.
 ************************************************************************/
HIDDEN void getCpuTimeSyscall() {
    STCK(currentTOD);
    currentProcess->p_s.s_v0 = currentProcess->p_time + (currentTOD - startTOD);
    currentProcess->p_time += (currentTOD - startTOD);

    loadProcessorState(currentProcess);
}


/************************************************************************
 * waitForClockSyscall (SYS7 - WAITCLOCK)
 *
 * Performs a P operation on the pseudo-clock semaphore. This always 
 * blocks, since the pseudo-clock semaphore is used for “sleeping” until 
 * a 100ms interrupt. The Current Process transitions to the blocked state,
 * then the Scheduler is invoked.
 ************************************************************************/
HIDDEN void waitForClockSyscall() {
    devSemaphore[PCLOCKIDX]--;
    blockCurrentProcess(&devSemaphore[PCLOCKIDX]);
    softBlockedCount++;
    switchProcess();  /* Never returns here */
}


/************************************************************************
 * getSupportDataSyscall (SYS8 - GETSUPPORTPTR)
 *
 * Returns the address of the Current Process’s Support Structure in v0, 
 * or NULL if none was provided when the process was created. 
 * Resumes execution afterward.
 ************************************************************************/
HIDDEN void getSupportDataSyscall() {
    debugExc(currentProcess->p_supportStruct->sup_privatePgTbl[0].entryHI, currentProcess->p_supportStruct->sup_privatePgTbl[0].entryLO, 0xDEADBEEF, 0);
    currentProcess->p_s.s_v0 = (int) currentProcess->p_supportStruct;

    STCK(currentTOD);
    currentProcess->p_time += (currentTOD - startTOD);
    loadProcessorState(currentProcess);
}


/************************************************************************
 * passUpOrDie
 *
 * Implements the “Pass Up or Die” mechanism for TLB exceptions, Program 
 * Traps, or SYSCALLs ≥ 9. If the Current Process has a non-NULL Support 
 * Structure, the saved exception state is copied into the appropriate 
 * sup_exceptState area, and a LDCXT is performed using the sup_exceptContext. 
 * Otherwise, the Current Process is terminated.
 ************************************************************************/
void passUpOrDie(int exceptionType) {
    debugExc(0xBABE, 0xBEEF, 0xDEAD, 0xF00D);
    if (currentProcess->p_supportStruct != NULL) {
        debugExc(savedExceptState->s_pc, savedExceptState->s_entryHI, savedExceptState->s_cause, 0xBABE);
        debugExc(0xB16BEEF, currentProcess->p_supportStruct->sup_privatePgTbl[0].entryHI, currentProcess->p_supportStruct->sup_privatePgTbl[0].entryLO, 0);

        moveState(savedExceptState, 
                  &(currentProcess->p_supportStruct->sup_exceptState[exceptionType]));

        STCK(currentTOD);
        currentProcess->p_time += (currentTOD - startTOD);
        LDCXT(
            currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_stackPtr,
            currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_status,
            currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_pc
        );
    } else {
        /* No Support Structure => terminate this process and its children */
        terminateProcessAndProgeny(currentProcess);
        currentProcess = NULL;
        switchProcess(); 
    }
}


/************************************************************************
 * syscallExceptionHandler
 *
 * Entry point for SYSCALL exceptions (if a0 ∈ [1..8] and in kernel-mode).
 * - Increments the saved state’s PC by 4 to avoid repeated SYSCALL loops.
 * - Checks if the call was issued in user-mode. If so, treat it as a 
 *   Program Trap.
 * - Otherwise, updates Current Process’s PCB state and routes to the 
 *   appropriate handler for SYS1..SYS8. For invalid calls (≥9), 
 *   treat as Program Trap or pass up to the Support Level.
 ************************************************************************/
void syscallExceptionHandler() {

    savedExceptState = (state_PTR) BIOSDATAPAGE;
    syscallNumber = savedExceptState->s_a0;
    

    /* Avoid an infinite loop of re-executing SYSCALL */
    savedExceptState->s_pc += WORDLEN;

    /* If the calling process was in user-mode, treat this request as illegal */
    if ((savedExceptState->s_status & USERPON) != ALLOFF) {
        /* Force a Program Trap by setting Cause.ExcCode = RI */
        debugExc(0xDEAD, 0xDEAD, 0xDEAD, 0xDEAD);
        savedExceptState->s_cause = (savedExceptState->s_cause & RESINSTRCODE);
        programTrapHandler();
		return; /* Ensures no return to the killed process */
    }

    /* If SYSCALL code is outside 1..8, handle as Program Trap (illegal) */
    if (syscallNumber < CREATEPROCESS || syscallNumber > GETSUPPORTPTR) {
        debugExc(0xBEEF, 0xBEEF, 0xBEEF, 0xBEEF);
        programTrapHandler();
		return; /* Same reason as above */
    }
    debugExc(0xEEFEFEF, 0xA55555, 0xBEEEEEEF, 0xBAAAAAD);
    debugExc(0xB16BEEF, syscallNumber, 0, 0);

    /* Update the Current Process's PCB to reflect the saved state */
    debugExc(0xBEEF, savedExceptState->s_entryHI, 0xBEEF, 0xBEEF);
    updateCurrentProcessState();
    debugExc(0xCAFE, savedExceptState->s_entryHI, 0xCAFE, 0xCAFE);

    switch (syscallNumber) {
        case CREATEPROCESS:    /* SYS1 */
            createNewProcess(
                (state_PTR) currentProcess->p_s.s_a1,
                (support_t *) currentProcess->p_s.s_a2
            );
            break;

        case TERMINATEPROCESS: /* SYS2 */
            terminateProcessAndProgeny(currentProcess);
            currentProcess = NULL;
            switchProcess();
            break;

        case PASSEREN:         /* SYS3 */
            passerenSyscall((int *) currentProcess->p_s.s_a1);
            break;

        case VERHOGEN:         /* SYS4 */
            verhogenSyscall((int *) currentProcess->p_s.s_a1);
            break;

        case WAITIO:           /* SYS5 */
            waitIODevice(
                currentProcess->p_s.s_a1, 
                currentProcess->p_s.s_a2, 
                currentProcess->p_s.s_a3
            );
            break;

        case GETCPUTIME:       /* SYS6 */
            getCpuTimeSyscall();
            break;

        case WAITCLOCK:        /* SYS7 */
            waitForClockSyscall();
            break;

        case GETSUPPORTPTR:    /* SYS8 */
            getSupportDataSyscall();
            break;

        default:
            /* Should never get here if [1..8] was properly checked */
            programTrapHandler();
            return;;
    }
}


/************************************************************************
 * programTrapHandler
 *
 * Handles Program Trap exceptions (e.g., invalid instructions, reserved
 * instructions, etc.). Invokes passUpOrDie with GENERALEXCEPT.
 ************************************************************************/
void programTrapHandler() {
    passUpOrDie(GENERALEXCEPT);
}


/************************************************************************
 * tlbExceptionHandler
 *
 * Handles TLB exceptions (1..3). Invokes passUpOrDie with PGFAULTEXCEPT.
 ************************************************************************/
void tlbExceptionHandler() {
    passUpOrDie(PGFAULTEXCEPT);
}

void uTLB_RefillHandler(){
    support_t *sPtr = (support_t *) SYSCALL (GETSUPPORTPTR, 0, 0, 0); /* Get the pointer to the Current Process’s Support Structure */
    state_PTR savedState = (state_PTR) BIOSDATAPAGE; /* Get the saved exception state from the BIOS Data Page */
    /*savedState = &(sPtr->sup_exceptState[PGFAULTEXCEPT]);  update to the state from the Current Process' Support Structure  */
    int missingPN = ((savedState->s_entryHI & VPNMASK) >> VPNSHIFT) % PGTBLSIZE; /* Extract the missing page number from Entry HI */
    debugExc(0x2, missingPN, savedState->s_entryHI, sPtr->sup_privatePgTbl[missingPN].entryHI);
    pte_entry_t entry = sPtr->sup_privatePgTbl[missingPN];  /* Get the Page Table entry for page number of the Current Process */
    /* Write this Page Table entry into the TLB */
    /*debugVM(0xCAFE, entry.entryHI, savedState->s_entryHI, entry.entryLO);*/

    setENTRYHI(currentProcess->p_supportStruct->sup_privatePgTbl[missingPN].entryHI);
    setENTRYLO(currentProcess->p_supportStruct->sup_privatePgTbl[missingPN].entryLO);

    TLBWR();
    debugExc(0x3, 0, 0, 0);
    LDST(savedState);   /* Return control to the Current Process to retry the instruction that caused the TLB-Refill event */
}
