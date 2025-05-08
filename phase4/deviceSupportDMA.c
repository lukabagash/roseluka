/******************************** deviceSupportDMA.c **********************************
 * deviceSupportDMA.c
 * Written by: Luka Bagashvili, Rosalie Lee
 ******************************************************************************/
           
#include "../h/deviceSupportDMA.h"
#include "../h/const.h"
#include "../h/vmSupport.h"    /* for disableInterrupts/enableInterrupts and mutex() */
#include "../h/initProc.h"     /* for extern p3devSemaphore[] */
#include "../h/sysSupport.h"     /* for schizoUserProcTerminate() */
#include "/usr/include/umps3/umps/libumps.h"

/* Sixteen 4 KB frames for DMA: 8 disks then 8 flashes */
static char dmaBufs[TOTAL_DMA_BUFFS][PAGESIZE];

void debugDMA(int a, int b, int c, int d) {
    int i = 0;
    i++;
}

/* Copy 4 KB from user virtual address into our DMA buffer */
static void copyUserToBuf(char *u, char *buf) {
    int i;
    for (i = 0; i < PAGESIZE; i++) {
        buf[i] = u[i];
    }
}
/* Copy 4 KB from DMA buffer back into user virtual address */
static void copyBufToUser(char *u, char *buf) {
    int i;
    for (i = 0; i < PAGESIZE; i++) {
        u[i] = buf[i];
    }
}

/* Core “seek+I/O” for a DMA device */
static int dmaOperation(int operation,
                        int devLine,       /* DISKINT or FLASHINT */
                        int devNo,         /* 0..7 */
                        char *buffer)
{
    /* build device‐index into p3devSemaphore[] and regfile */
    debugDMA(1, 0xBEEF, 0xBEEF, 0xBEEF);
    int idx = ((devLine - OFFSET) * DEVPERINT) + devNo;
    devregarea_t *regs = (devregarea_t *)RAMBASEADDR;
    device_t *dev = &regs->devreg[idx];
    int status;

    /* mutual‐exclusion over both buffer & device registers */
    mutex(&p3devSemaphore[idx], TRUE);
    debugDMA(2, 0xBEEF, 0xBEEF, 0xBEEF);

    /* tell device where our DMA buffer lives */
    dev->d_data0 = (unsigned int)buffer;

    /* atomically issue the command + block ourselves */
    debugDMA(3, devLine, devNo, operation);

    disableInterrupts();
    dev->d_command = operation;
    SYSCALL(WAITIO, devLine, devNo, (operation == READBLK));
    enableInterrupts();
    debugDMA(4, 0xBEEF, 0xBEEF, 0xBEEF);

    /* release the mutex */
    mutex(&p3devSemaphore[idx], FALSE);

    /* final device status */
    status = dev->d_status;
    if ((operation == WRITEBLK && status == WRITEERR) || (operation == READBLK  && status == READERR)) {
        /* Release the swap pool semaphore so we don't deadlock */
        SYSCALL(TERMINATE, 0, 0, 0); /* terminate the process */
    }
    return (status == DEVREDY ? status : -status);
}

int diskPut(char *virtAddr, int diskNo, int sectNo) {
    debugDMA(0xBEEF, 0xBEEF, 0xBEEF, 0xBEEF);
    char *buf = dmaBufs[diskNo];
    /* 1. copy from user into DMA buffer */
    copyUserToBuf(virtAddr, buf);
    /* 2. seek + write */
    /* combine sector# and operation in one command word */
    int op = (sectNo << 8) | WRITEBLK;
    return dmaOperation(op, DISKINT, diskNo, buf);
}

int diskGet(char *virtAddr, int diskNo, int sectNo) {
    debugDMA(0xDEAD, 0xDEAD, 0xDEAD, 0xDEAD);
    char *buf = dmaBufs[diskNo];
    int op = (sectNo << 8) | READBLK;
    int st = dmaOperation(op, DISKINT, diskNo, buf);
    if (st > 0) {
        /* 3. copy from DMA buffer back to user */
        copyBufToUser(virtAddr, buf);
    }
    return st;
}

int flashPut(char *virtAddr, int flashNo, int blockNo) {
    /* flash buffers live after the 8 disk buffers */
    char *buf = dmaBufs[DISK_DMA_COUNT + flashNo];
    copyUserToBuf(virtAddr, buf);
    int op = (blockNo << FLASCOMHSHIFT) | WRITEBLK;
    return dmaOperation(op, FLASHINT, flashNo, buf);
}

int flashGet(char *virtAddr, int flashNo, int blockNo) {
    char *buf = dmaBufs[DISK_DMA_COUNT + flashNo];
    int op = (blockNo << FLASCOMHSHIFT) | READBLK;
    int st = dmaOperation(op, FLASHINT, flashNo, buf);
    if (st > 0) {
        copyBufToUser(virtAddr, buf);
    }
    return st;
}
