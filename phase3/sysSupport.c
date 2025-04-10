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

void debugSYS(int a, int b, int c, int d) {
    /* Debugging function to print values */
    int i;
    i = 42;
    i++;
}

HIDDEN void illegalCheck(int len) {
    /*
     * Ensure the length is valid, this should be in the range of 0 to 12.
     */
    if (len < 0 || len > MAXSTRINGLEN) {
        ph3programTrapHandler(); /* Terminate the process */
    }
}

void schizoUserProcTerminate(int *address) {
    /*
     * Causes the executing U-proc to cease to exist.
     */
    if (address != NULL) {
        mutex(address, FALSE);  /* Release the mutex if address is given */
    }
    SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* Mentally unstable uproc calls sys2 */
}


HIDDEN void getTOD() {
    /*
     * Returns the number of microseconds since the system was last booted/reset. 
     */
    currentProcess->p_s.s_v0 = startTOD;
}

HIDDEN void writePrinter(char *virtAddr, int len, int dnum) {
    /*
     * Causes the requesting U-proc to be suspended until a line of output (string of characters) has been transmitted 
     * to the printer device associated with the U-proc. 
     * If the write was successful, returns the number of characters transmitted. Otherwise, returns the negative of the device’s status value.
     */
    int charNum = 0;
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR;
    int i; /* For loop index */
    device_t printerdev = reg->devreg[(PRNTINT - DISKINT) * DEVPERINT + dnum]; /* Get the printer device register */

    illegalCheck(len); /* Ensure the length is valid, this should be in the range of 0 to 128. */
       
    for (i = 0; i < len; i++) {
        /* Write printer device's DATA0 field with printer device address (i.e., address of printer device) */
        printerdev.d_data0 = virtAddr[i];
        printerdev.d_command = PRINTCHR; /* PRINTCHR command code */
        
        SYSCALL(WAITIO, PRNTINT, dnum, FALSE); /* Suspend u_proc, wait for I/O to complete */

        /* if not successfully written*/
        if (printerdev.d_status != DEVREDY) {
            charNum = 0 - printerdev.d_status;
            break;
        }
        charNum++;
    } 
    /* Resume back to user mode with v0 updated accordingly */
    currentProcess->p_s.s_v0 = charNum;
}

HIDDEN void writeTerminal(char *virtAddr, int len, int dnum) {
    /* 
     * Causes the requesting U-proc to be suspended until a line of output (string of characters) has been transmitted 
     * to the terminal device associated with the U-proc.
     * If the write was successful, returns the number of characters transmitted. Otherwise, returns the negative of the device’s status value.
     */
    int charNum = 0;
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR;
    int i; /* For loop index */
    device_t terminaldev = reg->devreg[(TERMINT - DISKINT) * DEVPERINT + dnum]; /* Get the terminal device register */
    illegalCheck(len); /* Ensure the length is valid, this should be in the range of 0 to 128. */
       
    for (i = 0; i < len; i++) {
        /* Write printer device's DATA0 field with printer device address (i.e., address of printer device)*/
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
    /*Resume back to user mode with v0 updated accordingly:*/
    currentProcess->p_s.s_v0 = charNum;
}

HIDDEN void readTerminal(char *virtAddr, int dnum) {
    /*
     * Causes the requesting U-proc to be suspended until a line of input (string of characters) has been transmitted 
     * from the terminal device associated with the U-proc.
     * If the read was successful, returns the number of characters transmitted. Otherwise, returns the negative of the device’s status value.
     */
    int charNum = 0;
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR;
    device_t terminaldev = reg->devreg[(TERMINT - DISKINT) * DEVPERINT + dnum]; /* Get the terminal device register */
       
    while(*virtAddr != ENDOFLINE){
        /* Write printer device's DATA0 field with printer device address (i.e., address of printer device)*/
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

void supLvlGenExceptionHandler() {
    /*
     * The Support Level provides the exception handlers that the Nucleus “passes” handling “up” to; assuming the process was provided a non-NULL
     * value for its Support Structure.
     * This function handles non-TLB exceptions, for all SYSCALL exceptions numbered 9 and above, and all Program Trap exceptions.
     */
    debugSYS(0xFADE, 0, 0, 0);
    support_t *sPtr = (support_t *) SYSCALL (GETSUPPORTPTR, 0, 0, 0); /* Get the pointer to the Current Process’s Support Structure */
    int dnum = sPtr->sup_asid - 1; /*Each U-proc is associated with its own flash and terminal device. The ASID uniquely identifies the process and by extension, its devices*/
    unsigned int cause = sPtr->sup_exceptState[0].s_cause; /* Get the cause of the TLB exception */
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT; /* Extract the exception code from the cause register */

    if (exc_code != SYSCALLEXCPT) /* TLB-Modification Exception */
    {
        ph3programTrapHandler(); /* Handle the TLB modification exception by invoking the program trap handler */
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
                , dnum
            );
            break;

        case WRITETERMINAL:        /* SYS12 */
            writeTerminal(
                (char *) (savedExceptState->s_a1), /* virtual address of the string to print */
                (int) (savedExceptState->s_a2)    /* length of the string */
                , dnum
            );
            break;

        case READTERMINAL:         /* SYS13 */
            readTerminal(
                (char *) (savedExceptState->s_a1) /* virtual address of the buffer to store the read characters */
                , dnum
            ); 
            break;

        default:
            /* Should never come here if the syscallexc checks out*/
            ph3programTrapHandler();
            return;
    }
}