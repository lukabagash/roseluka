/*This module implements the Support Level’s:
• general exception handler. [Section 4.6]
• SYSCALL exception handler. [Section 4.7]
• Program Trap exception handler. [Section 4.8]*/

#include "../h/sysSupport.h"
#include "../h/vmSupport.h"
#include "../h/initial.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/initProc.h"
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
    SYSCALL(VERHOGEN, (unsigned int) &masterSemaphore, 0, 0); /* performing a V operation on masterSemaphore, to come to a more graceful conclusion */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* Mentally unstable uproc calls sys2 */
}


HIDDEN void getTOD(state_PTR savedState) {
    cpu_t currTOD; 

    STCK(currTOD); /* storing the current value on the Time of Day clock into currTOD */
    savedState->s_v0 = currTOD; /* placing the current system time (since last reboot) in v0 */
    LDST(savedState);
}

HIDDEN void writePrinter(state_PTR savedState, char *virtAddr, int len, int dnum) {
    /*
     * Writes a line of output from the user buffer to the printer device.
     * Returns the number of characters successfully transmitted, or a negative error code.
     */
    int charNum = 0;
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR;
    device_t *printerdev = &(reg->devreg[(PRNTINT - DISKINT) * DEVPERINT + dnum]); /* Get the printer device */

    illegalCheck(len); /* Validate length (0 to MAXSTRINGLEN) */

    /* Lock the printer device semaphore */
    mutex(&(devSemaphores[((PRNTINT - OFFSET) * DEVPERINT) + dnum]), TRUE);

    while (len > 0) {
        disableInterrupts();
        printerdev->d_data0 = *virtAddr; /* Put character into DATA0 */
        printerdev->d_command = PRINTCHR; /* Issue PRINTCHR command */
        enableInterrupts();

        /* Capture the return value from SYS5 */
        unsigned int status = SYSCALL(WAITIO, PRNTINT, dnum, FALSE);

        if ((status & TERMSTATUSMASK) != DEVREDY) {
            savedState->s_v0 = 0 - (status & TERMSTATUSMASK); /* Return negative error code */
            mutex(&(devSemaphores[((PRNTINT - OFFSET) * DEVPERINT) + dnum]), FALSE);
            LDST(savedState);
        }

        virtAddr++;
        charNum++;
        len--;
    }

    savedState->s_v0 = charNum; /* Number of characters printed */
    mutex(&(devSemaphores[((PRNTINT - OFFSET) * DEVPERINT) + dnum]), FALSE);
    LDST(savedState);
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
    device_t *terminaldev = &(reg->devreg[(TERMINT - DISKINT) * DEVPERINT + dnum]);

    illegalCheck(len); /* Ensure the length is valid (0 to MAXSTRINGLEN) */

    /* Lock the device semaphore */
    mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum + DEVPERINT]), TRUE); /* NOTE: +DEVPERINT to select transmitter */

    while (len > 0) {
        debugSYS(0xACE55, 0xACE55, 0xACE55, 0xACE55);
        disableInterrupts();
        terminaldev->t_transm_command = ((*virtAddr) << 8) | TRANSMITCHAR; /* Issue transmit command */

        /* Capture the return value from SYS5 */
        unsigned int status = SYSCALL(WAITIO, TERMINT, dnum, FALSE);
        enableInterrupts();

        unsigned int statusCode = status & TERMSTATUSMASK; /* Mask to extract only the status bits */

        if (statusCode != CHARTRANSMITTED) {
            debugSYS(0xDEAD, 0xDEAD, 0xDEAD, 0xDEAD);
            savedState->s_v0 = 0 - statusCode; /* Negative error code */
            mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum + DEVPERINT]), FALSE);
            LDST(savedState);
        }

        virtAddr++;  /* Move to next character */
        charNum++;
        len--;       /* Decrement remaining length */
    }

    savedState->s_v0 = charNum; /* Return number of characters successfully transmitted */
    mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum + DEVPERINT]), FALSE);
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
    device_t *terminaldev = &(reg->devreg[(TERMINT - DISKINT) * DEVPERINT + dnum]); /* Get the terminal device */

    /* Lock the device semaphore */
    mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum]), TRUE); /* NOTE: no +DEVPERINT because it's receiver (t_recv) */

    while (1) {
        disableInterrupts();
        terminaldev->t_recv_command = RECEIVECHAR; /* Issue receive command */

        /* Capture the return value from SYS5 */
        unsigned int status = SYSCALL(WAITIO, TERMINT, dnum, TRUE);
        enableInterrupts();


        unsigned int statusCode = status & TERMSTATUSMASK; /* Mask to extract status bits */

        if (statusCode != CHARRECIVED) {
            savedState->s_v0 = 0 - statusCode; /* Return negative error code */
            mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum]), FALSE);
            LDST(savedState);
        }

        /* Retrieve received character */
        char receivedChar = (status >> 8) & 0xFF; /* Upper byte contains the character */

        *virtAddr = receivedChar; /* Store into user buffer */
        virtAddr++;
        charNum++;

        /* Stop reading if newline (ENDOFLINE) received */
        if (receivedChar == ENDOFLINE) {
            break;
        }
    }

    savedState->s_v0 = charNum; /* Number of characters received */
    mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum]), FALSE);
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
            getTOD(savedState); /* Get the current time of day */
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