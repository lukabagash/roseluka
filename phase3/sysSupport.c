/******************************** sysSupport.c **********************************
 * Support Level's general exception handler. 
 * Support Level's SYSCALL exception handler. 
 * Support Level's Program Trap exception handler.
 * Written by Rosalie Lee, Luka Bagashvili
 **************************************************************************/


#include "../h/sysSupport.h"
#include "../h/vmSupport.h"
#include "../h/initial.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/initProc.h"
#include "../h/exceptions.h"
#include "/usr/include/umps3/umps/libumps.h"

/************************************************************************
 * Helper Function
 * Ensure the length is valid, this should be in the range of 0 to 12(MAXSTRINGLEN).
 ************************************************************************/
HIDDEN void illegalCheck(int len) {
    if (len < 0 || len > MAXSTRINGLEN) {
        ph3programTrapHandler(); /* Terminate the process */
    }
}

/************************************************************************
 * SYS9: a user-mode “wrapper” for the kernel-mode restricted SYS2 service.
 * Causes the executing U-proc to cease to exist.
 ************************************************************************/
void schizoUserProcTerminate(int *address) {
    if (address != NULL) {
        mutex(address, FALSE);  /* Release the mutex if address is given */
    }
    SYSCALL(VERHOGEN, (unsigned int) &masterSemaphore, 0, 0); /* Perform a V operation on masterSemaphore for a graceful conclusion */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* SYS2 */
}

/************************************************************************
 * SYS10: Causes the number of microseconds since the system was last 
 * booted/reset to be placed/returned in the U-proc’s v0 register.
 ************************************************************************/
HIDDEN void getTOD(state_PTR savedState) {
    cpu_t currTOD; 
    STCK(currTOD); /* Store the current value on the Time of Day clock */
    savedState->s_v0 = currTOD; /* Place the current system time (since last booted) in v0 */
    LDST(savedState);
}

/************************************************************************
 * SYS11: causes the requesting U-proc to be suspended until a line of 
 * output (string of characters from the user buffer) has been 
 * transmitted to the printer device associated with the U-proc.
 * 
 * If the write was successful, returns the number of characters transmitted in v0. 
 * Otherwise, returns the negative of the device’s status value in v0.
 ************************************************************************/
HIDDEN void writePrinter(state_PTR savedState, char *virtAddr, int len, int dnum) {
    int charNum = 0; /* The number of characters that will be returned in v0 */
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR; /* Get register pointer of RAM base */
    device_t *printerdev = &(reg->devreg[((PRNTINT - DISKINT) * DEVPERINT) + dnum]); /* Get the printer device pointer from device register */

    illegalCheck(len); /* Check if valid length (0 to MAXSTRINGLEN) */

    mutex(&(p3devSemaphore[((PRNTINT - OFFSET) * DEVPERINT) + dnum]), TRUE); /* Gain mutual exclusion from the printer device semaphore */

    while (len > 0) {
        disableInterrupts(); /* Disable interrupts to atomically write data0 & command field, and call SYS5 */
        printerdev->d_data0 = *virtAddr; /* Put character into DATA0 */
        printerdev->d_command = PRINTCHR; /* Issue PRINTCHR command */
        unsigned int status = SYSCALL(WAITIO, PRNTINT, dnum, FALSE); /* Block I/O requests */
        enableInterrupts();

        unsigned int statusCode = status & TERMSTATUSMASK;
        /* If the write was not successful */
        if ((statusCode & TERMSTATUSMASK) != DEVREDY) {
            savedState->s_v0 = 0 - (status & TERMSTATUSMASK); /* Return negative error code */
            mutex(&(p3devSemaphore[((PRNTINT - OFFSET) * DEVPERINT) + dnum]), FALSE); /* Release mutual exclusion from the printer device semaphore */
            LDST(savedState);
        }

        /* If successfully transmitted, move to next character */
        virtAddr++;
        charNum++;
        len--;
    }
    savedState->s_v0 = charNum; /* Number of characters successfully transmitted */
    mutex(&(p3devSemaphore[((PRNTINT - OFFSET) * DEVPERINT) + dnum]), FALSE); /* Release mutual exclusion from the printer device semaphore */
    LDST(savedState);
}

/************************************************************************ 
 * SYS12: Causes the requesting U-proc to be suspended until a line of 
 * output (string of characters) has been transmitted to the terminal 
 * device associated with the U-proc.
 *
 * If the write was successful, returns the number of characters transmitted in v0. 
 * Otherwise, returns the negative of the device’s status value in v0.
 ************************************************************************/
HIDDEN void writeTerminal(state_PTR savedState, char *virtAddr, int len, int dnum) {
    int charNum = 0; /* The number of characters that will be returned in v0 */
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR; /* Get register pointer of RAM base */
    device_t *terminaldev = &(reg->devreg[(TERMINT - DISKINT) * DEVPERINT + dnum]); /* Get the terminal device pointer from device register */

    illegalCheck(len); /* Check if valid length (0 to MAXSTRINGLEN) */

    mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum + DEVPERINT]), TRUE); /* Gain mutual exclusion from the terminal device semaphore */

    while (len > 0) {
        disableInterrupts(); /* Disable interrupts to atomically write command field and call SYS5 */
        terminaldev->t_transm_command = ((*virtAddr) << 8) | TRANSMITCHAR; /* Issue transmit command */
        unsigned int status = SYSCALL(WAITIO, TERMINT, dnum, FALSE); /* Capture the return value from SYS5 */
        enableInterrupts();

        unsigned int statusCode = status & TERMSTATUSMASK; /* Extract the status bits */
        /* If the write was not successful */
        if (statusCode != CHARTRANSMITTED) {
            savedState->s_v0 = 0 - statusCode; /* Return negative error code */
            mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum + DEVPERINT]), FALSE); /* Release mutual exclusion from the terminal device semaphore */
            LDST(savedState);
        }

        /* If write was successful, move to next character */
        virtAddr++;  
        charNum++;
        len--;       
    }

    savedState->s_v0 = charNum; /* Return number of characters successfully transmitted */
    mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum + DEVPERINT]), FALSE); /* Release mutual exclusion from the terminal device semaphore */
    LDST(savedState);
}

/************************************************************************
 * SYS13: Causes the requesting U-proc to be suspended until a line of 
 * input (string of characters) has been transmitted from the terminal 
 * device associated with the U-proc.
 * 
 * If the read was successful, returns the number of characters transmitted in v0. 
 * Otherwise, returns the negative of the device’s status value in v0.
 ************************************************************************/
HIDDEN void readTerminal(state_PTR savedState, char *virtAddr, int dnum) {
    int charNum = 0; /* The number of characters that will be returned in v0 */
    devregarea_t *reg = (devregarea_t *) RAMBASEADDR; /* Get register pointer of RAM base */
    device_t *terminaldev = &(reg->devreg[(TERMINT - DISKINT) * DEVPERINT + dnum]); /* Get the terminal device pointer from device register */

    /* Gain mutual exclusion from the terminal device semaphore */
    mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum]), TRUE); /* NOTE: no +DEVPERINT because it's receiver (t_recv) */

    while (1) {
        disableInterrupts(); /* Disable interrupts to atomically write command field and call SYS5 */
        terminaldev->t_recv_command = RECEIVECHAR; /* Issue receive command */
        unsigned int status = SYSCALL(WAITIO, TERMINT, dnum, TRUE); /* Capture the return value from SYS5 */
        enableInterrupts();

        unsigned int statusCode = status & TERMSTATUSMASK; /* Mask to extract status bits */
        /* If the read was not successful */
        if (statusCode != CHARRECIVED) {
            savedState->s_v0 = 0 - statusCode; /* Return negative error code */
            mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum]), FALSE); /* Release mutual exclusion from the terminal device semaphore */
            LDST(savedState);
        }

        /* If the read was successful, etrieve received character */
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
    mutex(&(p3devSemaphore[((TERMINT - OFFSET) * DEVPERINT) + dnum]), FALSE); /* Release mutual exclusion from the terminal device semaphore */
    LDST(savedState);
}

/************************************************************************
 * The Support Level provides the exception handlers that the Nucleus 
 * “passes” handling “up” to; assuming the process was provided a 
 * non-NULL value for its Support Structure.
 * 
 * This function handles non-TLB exceptions, for all SYSCALL exceptions 
 * numbered 9 and above, and all Program Trap exceptions.
 ************************************************************************/
void supLvlGenExceptionHandler() {
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_PTR savedState = &(sPtr->sup_exceptState[1]);

    unsigned int cause    = savedState->s_cause;
    unsigned int exc_code = (cause & PANDOS_CAUSEMASK) >> EXCCODESHIFT;
    int dnum = sPtr->sup_asid - 1; /* Each U-proc is associated with its own flash and terminal device. The ASID uniquely identifies the process and by extension, its devices*/

    if (exc_code != SYSCALLEXCPT) /* TLB-Modification Exception */
    {
        ph3programTrapHandler(); /* Handle the TLB modification exception by invoking the program trap handler */
    }
    
    int syscallNumber = savedState->s_a0; /* Extract Syscall number to handle other general exceptions */

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
            /* Should never enter if the syscallexc checks out */
            ph3programTrapHandler();
            return;
    }
}