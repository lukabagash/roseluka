/*This module implements the Support Level’s:
• general exception handler. [Section 4.6]
• SYSCALL exception handler. [Section 4.7]
• Program Trap exception handler. [Section 4.8]*/

#include "../h/sysSupport.h"
#include "../h/vmSupport.h"
#include "../h/initial.h"
#include "../h/types.h"
#include "../h/exceptions.h"
#include "/usr/include/umps3/umps/libumps.h"

HIDDEN void schizoUserProcTerminate(address) {
    
    if address == NULL
        mutex((int *) &devSemaphore, FALSE);
    get supportstruct

    SYSCALL (TERMINATEPROCESS, 0, 0, 0)

}

HIDDEN void getTOD() {
    currentProcess->p_s.s_v0 = startTOD;
}

HIDDEN void writePrinter(char *virtAddr, int len) {
    int charNum = 0;
    devregarea_t reg = RAMBASEADDR;
    device_t printerdev = reg->devreg[(PRNTINT - DISKINT) * DEVPERINT];

       
    for (int i = 0; i < len; i++) {
        // Write printer device's DATA0 field with printer device address (i.e., address of printer device)
        printerdev.data0 = virtAddr[i];
        printerdev.d_command = PRINTCHR; /* PRINTCHR command code */
        
        /* do we issue sys5? to suspend u_proc */
        SYSCALL(5, )

        /* if not successfully written PRINTERROR status code */
        if (printerdev.d_status = PRINTERROR) {
            charNum = -printerdev.d_status;
            break;
        }
        charNum++;
    } 
    //Resume back to user mode with v0 updated accordingly:
    currentProcess->p_s.s_v0 = charNum;
}

HIDDEN void writeTerminal(char *virtAddr, int len) {
    int charNum = 0;
    devregarea_t reg = RAMBASEADDR;
    device_t terminaldev = reg->devreg[(TERMINT - DISKINT) * DEVPERINT];
       
    for (int i = 0; i < len; i++) {
        // Write printer device's DATA0 field with printer device address (i.e., address of printer device)
        /* terminaldev.d_status = ALLOFF | terminaldev.d_status | (virtAddr[i] << 8); */
        terminaldev.d_command = RECEIVECHAR; /* PRINTCHR command code */

        SYSCALL(WAITIO, TERMINT, TRUE, 0);  /* suspend u_proc */
        va = supportstruct.state.s_a1
        devicesem = (PRNTINT - DISKINT) * DEVPERINT + (pid-1)

        /* if not successfully written Receive Error status code */
        if ((terminaldev.d_status & 0xFF)= RECEIVEERROR) {
            charNum = -terminaldev.d_status;
            break;
        }
        charNum++;
    } 
    //Resume back to user mode with v0 updated accordingly:
    currentProcess->p_s.s_v0 = charNum;
}

HIDDEN void readTerminal(char *virtAddr){
    int charNum = 0;
    devregarea_t reg = RAMBASEADDR;
    device_t printerdev = reg->devreg[(TERMINT - DISKINT) * DEVPERINT];
       
    for (int i = 0; i < len; i++) {
        // Write printer device's DATA0 field with printer device address (i.e., address of printer device)
        printerdev.t_transm_command = ALLOFF | printerdev.d_data0 | (virtAddr[i] << 8);
        
        status = SYSCALL(WAITIO, 0, 0, 0);

        /* if not successfully written Transmission Error status code */
        if (status == TRANSMISERROR) {
            charNum = 0 - status;
            break;
        }
        charNum++;
    } 
    //Resume back to user mode with v0 updated accordingly:
    currentProcess->p_s.s_v0 = charNum;
}

void supLvlGenExceptionHandler()
{
    support_t *sPtr = SYSCALL (GETSUPPORTPTR, 0, 0, 0);
    unsigned int cause = sPtr->sup_exceptState[0].s_cause; /* Get the cause of the TLB exception */
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT; /* Extract the exception code from the cause register */
    if (exc_code == someshit) /* TLB-Modification Exception */
    {
        programTrapHandler(); /* Handle the TLB modification exception by invoking the program trap handler */
    }
    
    /* Handle other general exceptions */
    savedExceptState = (state_PTR) BIOSDATAPAGE;
    syscallNumber = savedExceptState->s_a0;

    switch (syscallNumber) {
        case TERMINATE:            /* SYS9 */
            schizoUserProcTerminate();
            break;

        case GETTOD:               /* SYS10 */
            getTOD(); /* Get the current time of day */
            break;

        case WRITEPRINTER:         /* SYS11 */
            writePrinter(
                (char *) (savedExceptState->s_a1), /* virtual address of the string to print */
                (int) (savedExceptState->s_a2)    /* length of the string */
            );
            break;

        case WRITETERMINAL:        /* SYS12 */
            writeTerminal();
            break;

        case READTERMINAL:         /* SYS13 */
            readTerminal(); 
            break;

        default:
            /* Should never get here if [1..8] was properly checked */
            programTrapHandler();
            return;
    }
}