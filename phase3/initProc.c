#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/exceptions.h"  /* For the exception handler */
#include "../h/vmSupport.h" /* For the initSwapStructs function */
#include "../h/sysSupport.h" /* For the syscall support functions */
#include "/usr/include/umps3/umps/libumps.h"

HIDDEN void initProcessorState(state_PTR newState);

int p3devSemaphore[PERIPHDEVCNT]; /* Sharable peripheral I/O device, (Disk, Flash, Network, Printer): 4 classes × 8 devices = 32 semaphores 
                                                         (Terminal devices): 8 terminals × 2 semaphores = 16 semaphores*/
int masterSemaphore; /* Private semaphore for graceful conclusion/termination of test */

void debugFR(int a, int b, int c, int d) {
    /* Debugging function to print values */
    int i;
    i = 42;
    i++;
}

void initProcessorState(state_PTR newState){
	newState->s_pc = newState->s_t9 = (memaddr) TEXTAREASTART; /* initializing the U-proc's PC (and the contents of the U-proc's t9 register) to 0x800000B0 */
	newState->s_sp = (memaddr) STCKTOPEND; /* initializing the U-proc's stack pointer to 0xC0000000 */
	newState->s_status = ALLOFF | PANDOS_IEPBITON | TEBITON | USERPON | PANDOS_CAUSEINTMASK; /* initializing the U-procs' status register so that interrupts are enabled, user-mode is on and the PLT is enabled */
}


void test() {
    static support_t supportStruct[UPROCMAX + 1]; /* Initialize the support structure for the process */
    state_t u_procState; /* Pointer to the processor state for u_proc */
    int i; /* For Page table */
    int j; /* Set dev sema4 to 1*/
    int k; /* Perform P after launching all the U-procs*/
    int pid; /* Set the process ID (asid of u_proc) */
    masterSemaphore = 0;    
    int res; /* Result of the SYSCALL */
    debugFR(0x1, 0,0,0);
    /* The Swap Pool table and Swap Pool semaphore. [Section 4.4.1] */
    for(j = 0; j < MAXDEVICECNT - 1; j++) {
        p3devSemaphore[j] = 1; /* Initialize the semaphores to 1 indicating the I/O devices are available, for mutual exclusion */
    }
    initSwapStructs(); /* Initialize the swap structures for paging */
    initProcessorState(&u_procState);
    debugFR(0x2, 0,0,0);
    /* Initialize and launch (SYS1) between 1 and 8 U-procs */
    for(pid = 1; pid < UPROCMAX + 1; pid++) {
        supportStruct[pid].sup_asid = pid; /* Assign process ID to asid of each u_proc */
        supportStruct[pid].sup_exceptContext[0].c_pc = (memaddr) supLvlTlbExceptionHandler; /* Set the TLB exception handler address for page fault exceptions */
        supportStruct[pid].sup_exceptContext[0].c_stackPtr = (memaddr) &(supportStruct[pid].sup_stackTLB[SUPSTCKTOP]); /* Set the stack pointer for TLB exceptions */
        supportStruct[pid].sup_exceptContext[0].c_status = ALLOFF | PANDOS_IEPBITON | PANDOS_CAUSEINTMASK | TEBITON; /* Enable Interrupts, enable PLT, Kernel-mode */

        supportStruct[pid].sup_exceptContext[1].c_pc = (memaddr) supLvlGenExceptionHandler; /* Set the general exception handler address for general exceptions */
        supportStruct[pid].sup_exceptContext[1].c_stackPtr = (memaddr) &(supportStruct[pid].sup_stackGen[SUPSTCKTOP]); /* Set the stack pointer for general exceptions */
        supportStruct[pid].sup_exceptContext[1].c_status = ALLOFF | PANDOS_IEPBITON | PANDOS_CAUSEINTMASK | TEBITON; /* Enable Interrupts, enable PLT, Kernel-mode */
        debugFR(0x3, supportStruct[pid].sup_exceptContext[0].c_pc,supportStruct[pid].sup_exceptContext[1].c_pc, supportStruct[pid].sup_exceptContext[0].c_stackPtr);
        for (i = 0; i < PGTBLSIZE; i++) {
            supportStruct[pid].sup_privatePgTbl[i].entryHI = ALLOFF | (((KUSEG >> VPNSHIFT) + i) << VPNSHIFT) | (pid << ASIDSHIFT); /* Set entryHI with the page number and asid */
            supportStruct[pid].sup_privatePgTbl[i].entryLO = ALLOFF | (i << PFNSHIFT) | 0x00000400 | VALIDOFF | GLOBALOFF; /* Set entryLO with the frame number and write enabled, private to the specific ASID, and not valid */
        } 

        u_procState.s_entryHI = (pid << ASIDSHIFT) | ALLOFF;  /* Set the entry HI for the user process */
        debugFR(0x4, u_procState.s_entryHI, 0, 0);

        supportStruct[pid].sup_privatePgTbl[PGTBLSIZE - 1].entryHI = ALLOFF | (pid << ASIDSHIFT) | STCKPGVPN; /* Set the entry HI for the Page Table entry 31 */

        res = SYSCALL(CREATEPROCESS, (unsigned int) &(u_procState), (unsigned int) &(supportStruct[pid]), 0); /* Call SYS1 to create a new process with the processor state and support structure */
        if(res != OK) {
            debugFR(0xBABE, 0xCAFE, 0, 0);
            SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* If the process creation failed, terminate the process */
            /* SYSCALL(VERHOGEN, (unsigned int) &masterSemaphore, 0, 0);  Nucleus terminate them instead of blocking test on a semaphore and forcing a PANIC */
        }
        debugFR(0x5, supportStruct[pid].sup_privatePgTbl[0].entryHI, supportStruct[pid].sup_privatePgTbl[PGTBLSIZE - 1].entryHI, 0);
    }
    /* After launching all the U-procs, the Nucleus scheduler will detect deadlock and invoke PANIC. [Section 3.2] */
    for(k = 0; k < UPROCMAX; k++) {
        /* Perform a P operation on the master semaphore */
        SYSCALL(PASSEREN, (unsigned int) &masterSemaphore, 0, 0);
    }
    debugFR(0x5, masterSemaphore, 0, 0);
    /* Terminate (SYS2) after all of its U-proc “children” processes conclude. 
    This will drive Process Count to zero, triggering the Nucleus to invoke HALT. [Section 3.2] */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* After all processes conclude, HALT by the Nucleus */
}