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

HIDDEN void illegalCheck(int len) {
    /*
     * Ensure the length is valid, this should be in the range of 0 to 12.
     */
    if (len < 0 || len > MAXSTRINGLEN) {
        schizoUserProcTerminate(NULL); /* Terminate the process */
    }
}

HIDDEN void schizoUserProcTerminate(int *address) {
    if (address != NULL) {
        mutex(address, FALSE);  /* Release the mutex if address is given */
    }
    SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* Mentally unstable uproc calls sys2 */
}


HIDDEN void getTOD() {
    currentProcess->p_s.s_v0 = startTOD;
}

HIDDEN void writePrinter(char *virtAddr, int len) {
    int charNum = 0;
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR;
    int dnum = 1; /* Temporary dnum */
    device_t printerdev = reg->devreg[(PRNTINT - DISKINT) * DEVPERINT];

    illegalCheck(len); /* Ensure the length is valid, this should be in the range of 0 to 128. */
       
    for (int i = 0; i < len; i++) {
        // Write printer device's DATA0 field with printer device address (i.e., address of printer device)
        printerdev.data0 = virtAddr[i];
        printerdev.d_command = PRINTCHR; /* PRINTCHR command code */
        
        SYSCALL(WAITIO, PRNTINT, dnum, FALSE); /* suspend u_proc, wait for I/O to complete */

        /* if not successfully written*/
        if (printerdev.d_status != DEVREDY) {
            charNum = 0 - printerdev.d_status;
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
    int dnum = 1; /* Device number for the terminal device */
    device_t terminaldev = reg->devreg[(TERMINT - DISKINT) * DEVPERINT + dnum]; /* Get the terminal device register */
    illegalCheck(len); /* Ensure the length is valid, this should be in the range of 0 to 128. */
       
    for (int i = 0; i < len; i++) {
        // Write printer device's DATA0 field with printer device address (i.e., address of printer device)
        /* terminaldev.d_status = ALLOFF | terminaldev.d_status | (virtAddr[i] << 8); */
        terminaldev.t_transm_command = (virtAddr[i] << 8) | TRANSMITCHAR; /* PRINTCHR command code */

        SYSCALL(WAITIO, TERMINT, dnum, FALSE);  /* suspend u_proc */

        /* if not successfully written Receive Error status code */
        if ((terminaldev.t_transm_status & TERMSTATUSMASK) != CHARTRANSMITTED) {
            charNum = 0 - terminaldev.d_status;
            break;
        }
        charNum++;
    } 
    //Resume back to user mode with v0 updated accordingly:
    currentProcess->p_s.s_v0 = charNum;
}

HIDDEN void readTerminal(char *virtAddr){
    int charNum = 0;
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR;
    int dnum = 1; /* Device number for the terminal device */
    device_t terminaldev = reg->devreg[(TERMINT - DISKINT) * DEVPERINT + dnum]; /* Get the terminal device register */
       
    while(*virtAddr != ENDOFLINE){
        // Write printer device's DATA0 field with printer device address (i.e., address of printer device)
        terminaldev.t_recv_command = RECEIVECHAR;;
        
        SYSCALL(WAITIO, TERMINT, dnum, TRUE);

        /* if not successfully written Transmission Error status code */
        if ((terminaldev.t_recv_status & TERMSTATUSMASK) != CHARRECIVED) {
            charNum = 0 - (terminaldev.d_status & TERMSTATUSMASK); /* Set charNum to negative of the status code to indicate an error */
            break;
        }
        charNum++;
        virtAddr++; /* Move to the next character in the buffer */
    } 
    /* Return the number of char read */
    currentProcess->p_s.s_v0 = charNum;
}

void supLvlGenExceptionHandler()
{
    support_t *sPtr = SYSCALL (GETSUPPORTPTR, 0, 0, 0);
    unsigned int cause = sPtr->sup_exceptState[0].s_cause; /* Get the cause of the TLB exception */
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT; /* Extract the exception code from the cause register */
    if (exc_code != SYSCALLEXCPT) /* TLB-Modification Exception */
    {
        programTrapHandler(); /* Handle the TLB modification exception by invoking the program trap handler */
    }
    
    /* Handle other general exceptions */
    state_PTR savedExceptState = (state_PTR) BIOSDATAPAGE;
    int syscallNumber = savedExceptState->s_a0;

    switch (syscallNumber) {
        case TERMINATE:            /* SYS9 */
            schizoUserProcTerminate(NULL); /* Terminate the current process */
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
            writeTerminal(
                (char *) (savedExceptState->s_a1), /* virtual address of the string to print */
                (int) (savedExceptState->s_a2)    /* length of the string */
            );
            break;

        case READTERMINAL:         /* SYS13 */
            readTerminal(
                (char *) (savedExceptState->s_a1) /* virtual address of the buffer to store the read characters */
            ); 
            break;

        default:
            /* Should never come here if the syscallexc checks out*/
            programTrapHandler();
            return;
    }
}