/******************************** deviceSupportDMA.c **********************************
 * deviceSupportDMA.c
 * Written by: Luka Bagashvili, Rosalie Lee
 ******************************************************************************/
           
 #include "../h/initProc.h"
 #include "../h/const.h"         
 #include "/usr/include/umps3/umps/libumps.h"
 

/* DMA buffer for each device (16 4Kb RAM frames need to be allocated to support these DMA devices) */
extern unsigned int *dmaBuffer[16]; /* size 4 KB each entry */

/************************************************************************
 * Helper Functions
 * Disable/Enable interrupts to process atomically.
 ************************************************************************/
void disableInterrupts() {
    setSTATUS(getSTATUS() & IECOFF);
}
void enableInterrupts() {
    setSTATUS(getSTATUS() | IECON);
}

/************************************************************************
 * Helper Function
 * Gain/Release mutual exclusion from the designated semaphore.
 ************************************************************************/
void mutex(int *sem, int acquire) {
    /* If 1 is passed into the acquire, call SYS3 to gain mutex */
    if (acquire) {
        SYSCALL(PASSEREN, (unsigned int)sem, 0, 0);
    } 
    /* If 0 is passed into the acquire, call SYS4 to release mutex */
    else {
        SYSCALL(VERHOGEN, (unsigned int)sem, 0, 0);
    }
}

/* Initiating I/O Operations - */
static int diskOperation(unsigned int operation, int diskNo, unsigned int sectNo)
{
    int devIndex = ((DISKINT - OFFSET) * DEVPERINT) + (asid - 1);  /* Identify the disk device with ASID */
    int *devSem  = &p3devSemaphore[devIndex];           /* not sure if we use p3devSemaphore, because "µMPS3 can support up to eight disk devices and eight flash devices. Hence, sixteen 4Kb RAM frames need to be allocated to support these DMA devices." */

    mutex(devSem, TRUE); /* Gain mutual exclusion from the device semaphore */

    devregarea_t *devReg = (devregarea_t *) RAMBASEADDR;
    device_t *diskDev = &(devReg->devreg[devIndex]);

    diskDev->d_data0 = 0x20020000; /* starting address of the appropriate DMA buffer(immediately before the swap pool) */
    
    disableInterrupts(); /* Disable interrupts to atomically write the command field, and call SYS5 */

    flashDev->d_command = (sectNo << DISKSHIFT) | operation;    /* DISKSHIFT=0 */
    SYSCALL(WAITIO, DISKINT, diskNo, (operation == WRITEBLK));           /* SYS5 to block the I/O requesting U-proc until seek done */

    enableInterrupts();

    mutex(devSem, FALSE); /* Gain mutual exclusion from the device semaphore */

}
