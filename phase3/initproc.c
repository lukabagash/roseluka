#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/exceptions.h"  /* For the exception handler */
#include "../h/vmSupport.h" /* For the initSwapStructs function */
#include "/usr/include/umps3/umps/libumps.h"

int devSemaphore[MAXDEVICECNT - 1]; /* Sharable peripheral I/O device, (Disk, Flash, Network, Printer): 4 classes × 8 devices = 32 semaphores 
                                                         (Terminal devices): 8 terminals × 2 semaphores = 16 semaphores*/
int masterSemaphore; /* Private semaphore for graceful conclusion/termination of test */

void test() {
    pcb_PTR u_proc; /* Pointer to the user process */
    static support_t supportStruct[UPROCMAX + 1]; /* Initialize the support structure for the process */
    state_PTR u_procState; /* Pointer to the processor state for u_proc */
    int i; /* For Page table */
    int j; /* Set dev sema4 to 1*/
    int k; /* Perform P after launching all the U-procs*/
    int pid; /* Set the process ID (asid of u_proc) */
    masterSemaphore = 0;    
    int res; /* Result of the SYSCALL */

    /* The Swap Pool table and Swap Pool semaphore. [Section 4.4.1] */
    initSwapStructs(); /* Initialize the swap structures for paging */
    for(j = 0; j < MAXDEVICECNT - 1; j++) {
        devSemaphore[j] = 1; /* Initialize the semaphores to 1 indicating the I/O devices are available, for mutual exclusion */
    }

    /* Initialize and launch (SYS1) between 1 and 8 U-procs */
    for(pid = 1; pid < UPROCMAX + 1; pid++) {
        supportStruct.sup_asid = pid; /* Assign process ID to asid of each u_proc */
        supportStruct.sup_exceptContext[0].c_pc = (memaddr) supLvlTlbExceptionHandler; /* Set the TLB exception handler address for page fault exceptions */
        supportStruct.sup_exceptContext[0].c_stackPtr = (memaddr) &supportStruct[pid].sup_stackTLB[SUPSTCKTOP]; /* Set the stack pointer for TLB exceptions */
        supportStruct.sup_exceptContext[0].c_status = ALLOFF | PANDOS_IEPBITON | PANDOS_CAUSEINTMASK | TEBITON; /* Enable Interrupts, enable PLT, Kernel-mode */

        supportStruct.sup_exceptContext[1].c_pc = (memaddr) supLvlGenExceptionHandler; /* Set the general exception handler address for general exceptions */
        supportStruct.sup_exceptContext[1].c_stackPtr = (memaddr) &supportStruct[pid].sup_stackGen[SUPSTCKTOP]; /* Set the stack pointer for general exceptions */
        supportStruct.sup_exceptContext[1].c_status = ALLOFF | PANDOS_IEPBITON | PANDOS_CAUSEINTMASK | TEBITON; /* Enable Interrupts, enable PLT, Kernel-mode */

        for (i = 0; i < PGTBLSIZE; i++) {
            supportStruct.sup_privatePgTbl[i].entryHI = ALLOFF | (KUSEG + i << VPNSHIFT) | (pid << ASIDSHIFT); /* Set entryHI with the page number and asid */
            supportStruct.sup_privatePgTbl[i].entryLO = ALLOFF | (i << PFNSHIFT) | DIRTYON | VALIDOFF | GLOBALOFF; /* Set entryLO with the frame number and write enabled, private to the specific ASID, and not valid */
        }

        /* Set the program counter and s_t9 to the logical address for the start of the .text area */
        u_procState->s_pc = (memaddr) TEXTAREASTART;
        u_procState->s_t9 = (memaddr) TEXTAREASTART; 
        /* Set the status to enable Interrupts, enable PLT, User-mode */
        u_procState->s_status = ALLOFF | PANDOS_IEPBITON | TEBITON | USERPON;
        u_procState->s_sp = (memaddr) STCKTOPEND; /* Set the stack pointer for the user process */
        u_procState->s_entryHI = pid; /* Set the entry HI for the user process */

        supportStruct.sup_privatePgTbl[PGTBLSIZE - 1].entryHI = ALLOFF | (pid << ASIDSHIFT) | (STCKPGVPN << VPNSHIFT); /* Set the entry HI for the Page Table entry 31 */

        res = SYSCALL(CREATEPROCESS, (unsigned int) u_procState, (unsigned int) &(supportStruct[pid]), 0); /* Call SYS1 to create a new process with the processor state and support structure */
        
        if(res != OK) {
            SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* If the process creation failed, terminate the process */
            SYSCALL(VERHOGEN, (unsigned int) &masterSemaphore, 0, 0); /* Nucleus terminate them instead of blocking test on a semaphore and forcing a PANIC */
        }
    }
    /*After launching all the U-procs, the Nucleus scheduler will detect deadlock and invoke PANIC. [Section 3.2]*/
    for(k = 0; k < UPROCMAX; k++) {
        /* Perform a P operation on the master semaphore to wait for all user processes to complete */
        SYSCALL(VERHOGEN, (unsigned int) &masterSemaphore, 0, 0);
    }

    /* Terminate (SYS2) after all of its U-proc “children” processes conclude. 
    This will drive Process Count to zero, triggering the Nucleus to invoke HALT. [Section 3.2] */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* After all processes conclude, HALT by the Nucleus*/
}
