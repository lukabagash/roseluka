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

HIDDEN void writePrinter(state_PTR savedState, char *virtAddr, int len, int dnum) {
    /*
     * Causes the requesting U-proc to be suspended until a line of output (string of characters) has been transmitted 
     * to the printer device associated with the U-proc. 
     * If the write was successful, returns the number of characters transmitted. Otherwise, returns the negative of the device’s status value.
     */
    debugSYS(0x11, 0x60D,0x60D,0x60D );

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

HIDDEN void writeTerminal(state_PTR savedState, char *virtAddr, int len, int dnum) {
    /* 
     * Causes the requesting U-proc to be suspended until a line of output (string of characters) has been transmitted 
     * to the terminal device associated with the U-proc.
     * If the write was successful, returns the number of characters transmitted. Otherwise, returns the negative of the device’s status value.
     */
    debugSYS(0x12, 0x60D,0x60D,0x60D );

    int charNum = 0;
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR;
    device_t *terminaldev = &(reg->devreg[(TERMINT - DISKINT) * DEVPERINT + dnum]); /* Get the terminal device register */
    illegalCheck(len); /* Ensure the length is valid, this should be in the range of 0 to 128. */
    
    while (len > 0) {
        /* Write printer device's DATA0 field with printer device address (i.e., address of printer device)*/
        /* terminaldev.d_status = ALLOFF | terminaldev.d_status | (virtAddr[i] << 8); */
        debugSYS(0xACE55, 0xACE55, 0xACE55, 0xACE55);
        disableInterrupts();
        terminaldev->t_transm_command = ((*virtAddr) << 8) | TRANSMITCHAR; /* Send the character */
        SYSCALL(WAITIO, TERMINT, dnum, FALSE);  /* suspend u_proc */
        enableInterrupts();

        /* if not successfully written Receive Error status code */
        if ((terminaldev->t_transm_status & TERMSTATUSMASK) != CHARTRANSMITTED) {
            debugSYS(0xDEAD, 0xDEAD, 0xDEAD, 0xDEAD);
            savedState->s_v0 = 0 - (terminaldev->d_status & TERMSTATUSMASK); /* Return negative error code */
            LDST(savedState);
        }
        virtAddr++;  /* Move to next character in user buffer */
        charNum++;
        len--;       /* Decrement remaining length */
    } 
    /*Resume back to user mode with v0 updated accordingly:*/
    currentProcess->p_s.s_v0 = charNum;
    LDST(savedState);
}

HIDDEN void readTerminal(state_PTR savedState, char *virtAddr, int dnum) {
    /*
     * Causes the requesting U-proc to be suspended until a line of input (string of characters) has been transmitted 
     * from the terminal device associated with the U-proc.
     * If the read was successful, returns the number of characters transmitted. Otherwise, returns the negative of the device’s status value.
     */
    debugSYS(0x13, 0x60D,0x60D,0x60D );
    int charNum = 0;
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR;
    device_t *terminaldev = &(reg->devreg[(TERMINT - DISKINT) * DEVPERINT + dnum]); /* Get the terminal device register */
    debugSYS(0xACE55, 0xACE55, 0xACE55, 0xACE55);
       
    while(1){
        /* Write printer device's DATA0 field with printer device address (i.e., address of printer device)*/

        disableInterrupts();
        terminaldev->t_recv_command = RECEIVECHAR;
        
        SYSCALL(WAITIO, TERMINT, dnum, TRUE);
        enableInterrupts();

        /* if not successfully written Transmission Error status code */
        if ((terminaldev->t_recv_status & TERMSTATUSMASK) != CHARRECIVED) {
            savedState->s_v0 = 0 - (terminaldev->d_status & TERMSTATUSMASK); /* Return negative error code */
            LDST(savedState);
        }
        /* Retrieve the received character */
        char receivedChar = (terminaldev->t_recv_status >> 8) & 0xFF; /* Upper byte contains the character */

        *virtAddr = receivedChar; /* Store into user buffer */
        virtAddr++;
        charNum++;
    
        /* Stop reading if newline (ENDOFLINE) received */
        if (receivedChar == ENDOFLINE) {
            break;
        }
    } 
    /* Return the number of char read */
    savedState->s_v0 = charNum;
    LDST(savedState);
}

void supLvlGenExceptionHandler() {
    /*
     * The Support Level provides the exception handlers that the Nucleus “passes” handling “up” to; assuming the process was provided a non-NULL
     * value for its Support Structure.
     * This function handles non-TLB exceptions, for all SYSCALL exceptions numbered 9 and above, and all Program Trap exceptions.
     */
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_PTR savedState = &(sPtr->sup_exceptState[1]);

    unsigned int cause    = savedState->s_cause;
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT;
    int dnum = sPtr->sup_asid - 1; /*Each U-proc is associated with its own flash and terminal device. The ASID uniquely identifies the process and by extension, its devices*/
    debugSYS(0x1, exc_code, dnum, 0);
    if (exc_code != SYSCALLEXCPT) /* TLB-Modification Exception */
    {
        ph3programTrapHandler(); /* Handle the TLB modification exception by invoking the program trap handler */
    }
    
    /* Handle other general exceptions */
    int syscallNumber = savedState->s_a0;
    debugSYS(0x2, syscallNumber, dnum, 0);
    switch (syscallNumber) {
        case TERMINATE:            /* SYS9 */
            schizoUserProcTerminate(NULL); /* Terminate the current process */
            break;

        case GETTOD:               /* SYS10 */
            getTOD(); /* Get the current time of day */
            break;

        case WRITEPRINTER:         /* SYS11 */
            writePrinter(
                savedState,
                (char *) (savedState->s_a1), /* virtual address of the string to print */
                (int) (savedState->s_a2)    /* length of the string */
                , dnum
            );
            break;

        case WRITETERMINAL:        /* SYS12 */
            writeTerminal(
                savedState,
                (char *) (savedState->s_a1), /* virtual address of the string to print */
                (int) (savedState->s_a2)    /* length of the string */
                , dnum
            );
            break;

        case READTERMINAL:         /* SYS13 */
            readTerminal(
                savedState,
                (char *) (savedState->s_a1) /* virtual address of the buffer to store the read characters */
                , dnum
            ); 
            break;

        default:
            /* Should never come here if the syscallexc checks out*/
            ph3programTrapHandler();
            return;
    }
}