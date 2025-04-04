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
    static support_t supportStruct[UPROCMAX]; /* Initialize the support structure for the process */
    state_PTR u_procState; /* Pointer to the user process state */
    int i; /* For Page table */
    int j; /* Set dev sema4 to 1*/
    int k; /* Perform P after launching all the U-procs*/
    int asid; /* ASID for the process */
    masterSemaphore = 0;
    int res; /* Result of the SYSCALL */

    /* The Swap Pool table and Swap Pool semaphore. [Section 4.4.1] */
    initSwapStructs(); /* Initialize the swap structures for paging */
    for(j = 0; j < MAXDEVICECNT - 1; j++) {
        devSemaphore[j] = 1; /* Initialize all device semaphores to 1 */
    }

    /* Initialize and launch (SYS1) between 1 and 8 U-procs */
    for(asid = 1; asid < UPROCMAX + 1; asid++) {
        supportStruct.sup_asid = asid; /* Set the process ID (asid) to 1 */
        supportStruct.sup_exceptContext[0].c_pc = (memaddr) supLvlTlbExceptionHandler; /* Set the TLB exception handler address */
        supportStruct.sup_exceptContext[0].c_stackPtr = (memaddr) &supportStruct[asid].sup_stackTLB[SUPSTCKTOP]; /* Set the stack pointer for TLB exceptions */
        supportStruct.sup_exceptContext[0].c_status = ALLOFF | PANDOS_IEPBITON | PANDOS_CAUSEINTMASK | TEBITON; /* Interrup On, Timer On, Kernel-mode On */

        supportStruct.sup_exceptContext[1].c_pc = (memaddr) supLvlGenExceptionHandler; /* Set the general exception handler address */
        supportStruct.sup_exceptContext[1].c_stackPtr = (memaddr) &supportStruct[asid].sup_stackGen[SUPSTCKTOP]; /* Set the stack pointer for general exceptions */
        supportStruct.sup_exceptContext[1].c_status = ALLOFF | PANDOS_IEPBITON | PANDOS_CAUSEINTMASK | TEBITON; /* Interrup On, Timer On, Kernel-mode On */
        
        for (i = 0; i < USERPGTBLSIZE; i++) {
            supportStruct.sup_privatePgTbl[i].entryHI = ALLOFF | (i << VPNSHIFT) | supportStruct.sup_asid; /* Set entryHI with the page number and asid */
            supportStruct.sup_privatePgTbl[i].entryLO = ALLOFF | (i << PFNSHIFT) | DIRTYON | VALIDOFF | GLOBALOFF; /* Set entryLO with the page number and frame number */
        }

        u_procState->s_pc = (memaddr) 0x8000.00B0;
        u_procState->s_t9 = (memaddr) 0x8000.00B0; /* Set the program counter to the starting address of the user process */
        u_procState->s_status = ALLOFF | PANDOS_IEPBITON | TEBITON | USERPON;
        u_procState->s_sp = (memaddr)  0xC000.0000; /* Set the stack pointer for the user process */
        u_procState->s_entryHI = asid; /* Set the entry HI for the user process */

        res = SYSCALL(CREATEPROCESS, unsigned int u_procState, unsigned int &supportStruct[asid]); /* Call the SYSCALL to create a new process with the state and support structure */
        if(res != OK) {
            SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* If the process creation failed, terminate the process */
        }
    }
    supportStruct.sup_privatePgTbl[PGTBLSIZE].entryHI = ALLOFF | (PGTBLSIZE << VPNSHIFT) | 0xBFFFF000; /* Set the entry HI for the last page table entry */
    /*Perform a P (SYS3) operation on a private semaphore initialized to 0. 
    In this case, after all the U-proc “children” conclude, the Nucleus
    scheduler will detect deadlock and invoke PANIC. [Section 3.2]*/
    for(k = 0; k < 1; k++) {
        /* Perform a P operation on the master semaphore to wait for all user processes to complete */
        SYSCALL(PASSEREN, unsigned int &masterSemaphore, 0, 0);
    }

    /* Terminate (SYS2) after all of its U-proc “children” processes conclude. 
    This will drive Process Count to zero, triggering the Nucleus to invoke HALT. [Section 3.2] */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* After all processes conclude, HALT */
}