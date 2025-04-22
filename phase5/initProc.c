/******************************** initProc.c **********************************
 * This file launches user-mode processes and gets placed on the Ready Queue.
 * 
 * Initialization:
 * The device semaphores for peripheral I/O devices.
 * The masterSemaphore for termination of user processes.
 * The Swap Pool table for paging.
 * 
 * Each new process gets:
 * A support structure with TLB and general exception contexts to handle TLB-refill and general exceptions. 
 * A private page table initialized for each user process’s address translation. 
 * A processor state set to user-mode with interrupts and the processor local timer enabled. 
 * 
 * Written by Rosalie Lee, Luka Bagashvili
 **************************************************************************/

 
#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/exceptions.h"  
#include "../h/vmSupport.h" 
#include "../h/sysSupport.h" 
#include "../h/delayDaemon.h"
#include "/usr/include/umps3/umps/libumps.h"

int p3devSemaphore[PERIPHDEVCNT]; /* Sharable peripheral I/O device, (Disk, Flash, Network, Printer): 4 classes × 8 devices = 32 semaphores 
                                                         (Terminal devices): 8 terminals × 2 semaphores = 16 semaphores*/
int masterSemaphore; /* Private semaphore for graceful conclusion/termination of test */


void test() {
    static support_t supportStruct[UPROCMAX + 1]; /* Initialize the support structure for the process */
    state_t u_procState = { 0 };
    int i; /* For Page table */
    int j; /* Set dev sema4 to 1*/
    int k; /* Perform P after launching all the U-procs*/
    int pid; /* Set the process ID (asid of u_proc) */
    masterSemaphore = 0;    
    int res; /* Result of the SYSCALL */

    initSwapStructs(); /* Initialize the Swap Pool table structures for paging */
    initADL();

    /* Initialize the semaphores to 1 indicating the I/O devices are available, for mutual exclusion */
    for(j = 0; j < MAXDEVICECNT - 1; j++) {
        p3devSemaphore[j] = 1; 
    }
    /* Set the program counter and s_t9 to the logical address for the start of the .text area */
    u_procState.s_pc = (memaddr) TEXTAREASTART;
    u_procState.s_t9 = (memaddr) TEXTAREASTART; 
    /* Set the status to enable Interrupts, enable PLT, User-mode */
    u_procState.s_status = ALLOFF | PANDOS_IEPBITON | TEBITON | USERPON | PANDOS_CAUSEINTMASK;
    u_procState.s_sp = (memaddr) STCKTOPEND; /* Set the stack pointer for the user process */

    /* Initialize and launch (SYS1) between 1 to 8 U-procs */
    for(pid = 1; pid < UPROCMAX + 1; pid++) {
        supportStruct[pid].sup_asid = pid; /* Assign process ID to asid of each u_proc */
        supportStruct[pid].sup_exceptContext[0].c_pc = (memaddr) supLvlTlbExceptionHandler; /* Set the TLB exception handler address for page fault exceptions */
        supportStruct[pid].sup_exceptContext[0].c_stackPtr = (memaddr) &(supportStruct[pid].sup_stackTLB[SUPSTCKTOP]); /* Set the stack pointer for TLB exceptions */
        supportStruct[pid].sup_exceptContext[0].c_status = ALLOFF | PANDOS_IEPBITON | PANDOS_CAUSEINTMASK | TEBITON; /* Enable Interrupts, enable PLT, Kernel-mode */

        supportStruct[pid].sup_exceptContext[1].c_pc = (memaddr) supLvlGenExceptionHandler; /* Set the general exception handler address for general exceptions */
        supportStruct[pid].sup_exceptContext[1].c_stackPtr = (memaddr) &(supportStruct[pid].sup_stackGen[SUPSTCKTOP]); /* Set the stack pointer for general exceptions */
        supportStruct[pid].sup_exceptContext[1].c_status = ALLOFF | PANDOS_IEPBITON | PANDOS_CAUSEINTMASK | TEBITON; /* Enable Interrupts, enable PLT, Kernel-mode */

        for (i = 0; i < PGTBLSIZE; i++) {
            supportStruct[pid].sup_privatePgTbl[i].entryHI = ALLOFF | ((KVSBEGIN + i) << VPNSHIFT) | (pid << ASIDSHIFT);
			supportStruct[pid].sup_privatePgTbl[i].entryLO = ALLOFF | DIRTYON; 
        } 

        u_procState.s_entryHI = KUSEG | (pid << ASIDSHIFT) | ALLOFF;  /* Set the entry HI for the user process */

        supportStruct[pid].sup_privatePgTbl[PGTBLSIZE - 1].entryHI = ALLOFF | (pid << ASIDSHIFT) | STCKPGVPN; /* Set the entry HI for the Page Table entry 31 */
        /* Phase 5: private semaphore starts at 0 */
       supportStruct[pid].sup_delaySem = 0;

        res = SYSCALL(CREATEPROCESS, (unsigned int) &(u_procState), (unsigned int) &(supportStruct[pid]), 0); /* Create a new process with the processor state and support structure */
        
        /* If the process creation failed, terminate the process */
        if(res != OK) {
            SYSCALL(TERMINATEPROCESS, 0, 0, 0); 
        }
    }
    /* After launching all the U-procs, the Nucleus scheduler will detect deadlock and invoke PANIC */
    /* Perform a P operation on the master semaphores for each U-proc */
    for(k = 0; k < UPROCMAX; k++) {
        SYSCALL(PASSEREN, (unsigned int) &masterSemaphore, 0, 0);
    }

    /* Terminate after all of its U-proc “children” processes conclude. Process Count becomes zero, trigger HALT by Nucleus */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0); 
}