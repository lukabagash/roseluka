#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/exceptions.h"  /* For the exception handler */
#include "/usr/include/umps3/umps/libumps.h"

static support_t supportStruct[1]; /* Initialize the support structure for the process */

void main() {
    pcb_PTR u_proc; /* Pointer to the user process */
    allocPcb(); /* Allocate a PCB for the process */
    u_proc = allocPcb(); /* Allocate a PCB for the user process */

    state_PTR u_procState; /* Pointer to the user process state */
    /* Initialize the support structure for the process */
    int i;
    int asid;


    /* Initialize the private page table for the process */
    for(asid = 1; asid < 1 + 1; asid++) {
        supportStruct.sup_asid = asid; /* Set the process ID (asid) to 1 */
        supportStruct.sup_exceptContext[0].c_pc = (memaddr) tlbVExceptionHandler; /* Set the TLB exception handler address */
        supportStruct.sup_exceptContext[0].c_stackPtr = (memaddr) &supportStruct[asid].sup_stackTLB[499]; /* Set the stack pointer for TLB exceptions */
        supportStruct.sup_exceptContext[0].c_status = ALLOFF | PANDOS_IEPBITON | PANDOS_CAUSEINTMASK | TEBITON; /* Interrup On, Timer On, Kernel-mode On */

        supportStruct.sup_exceptContext[1].c_pc = (memaddr) genVExceptionHandler; /* Set the general exception handler address */
        supportStruct.sup_exceptContext[1].c_stackPtr = (memaddr) &supportStruct[asid].sup_stackGen[499]; /* Set the stack pointer for general exceptions */
        supportStruct.sup_exceptContext[1].c_status = ALLOFF | PANDOS_IEPBITON | PANDOS_CAUSEINTMASK | TEBITON; /* Interrup On, Timer On, Kernel-mode On */
        
        for (i = 1; i < USERPGTBLSIZE; i++) {
            supportStruct.sup_privatePgTbl[i].entryHI = ALLOFF | (i << VPNSHIFT) | supportStruct.sup_asid; /* Set entryHI with the page number and asid */
            supportStruct.sup_privatePgTbl[i].entryLO = ALLOFF | (i << PFNSHIFT) | DIRTYON | VALIDOFF | GLOBALOFF; /* Set entryLO with the page number and frame number */
            
        }

        u_procState->s_pc = (memaddr) 0x8000.00B0;
        u_procState->s_t9 = (memaddr) 0x8000.00B0; /* Set the program counter to the starting address of the user process */
        u_procState->s_status = ALLOFF | PANDOS_IEPBITON | TEBITON | USERPON;
        u_procState->s_sp = (memaddr)  0xC000.0000; /* Set the stack pointer for the user process */
        u_procState->s_entryHI = asid; /* Set the entry HI for the user process */

        createNewProcess(
            (state_PTR) u_procState, /* Pass the state of the user process */
            &supportStruct[asid] /* Pass the support structure */
        );
    }
}