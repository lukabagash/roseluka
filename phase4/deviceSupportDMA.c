/************************ deviceSupportDMA.c ************************
 *
 * Phase 4 / Level 5: Block‐Device DMA support for DISK and FLASH.
 * Migrated out of vmSupport.c to share flashOperation between
 * Pager and user syscalls SYS16/SYS17, and to implement SYS14/SYS15.
 *
 * Luka Bagashvili, Rosalie Lee
 **************************************************************************/

 #include "../h/types.h"
 #include "../h/const.h"
 #include "../h/vmSupport.h"     /* for copyUserToBuf, copyBufToUser */
 #include "../h/initial.h"       /* for external devSemaphore[] */
 #include "../h/interrupts.h"    /* for disableInterrupts(), enableInterrupts() */
 #include "/usr/include/umps3/umps/libumps.h"  /* for RAMBASEADDR, DISKINT, FLASHINT */
 #include "../h/sysSupport.h"    /* for SYSCALL and WAITIO */
 
 extern int devSemaphore[];     /* from initial.c */
 
 /* DMA buffers: 8 disks + 8 flashes = 16 × 4 KB frames */
 static char diskBuf[8][PAGESIZE];
 static char flashBuf[8][PAGESIZE];


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
 
 /*
  * dmaOp
  *   cmd   = the encoded COMMAND word (e.g. READBLK | (block<<BLKNUMSHIFT))
  *   line  = DISKINT or FLASHINT
  *   dev   = device number [0..7]
  *   buf   = pointer to DMA buffer for this device
  *
  * Returns the raw status from the device (1 = DEVREADY).
  */
 static int dmaOp(int cmd, int line, int dev, char *buf) {
     int idx = (line - DISKINT) * DEVPERINT + dev;
     /* P(devSemaphore[idx]) */
     mutex(&(devSemaphore[idx]), TRUE);
 
     /* tell the hardware where our buffer lives */
     ((devregarea_t *)RAMBASEADDR)->devreg[idx].d_data0 = (memaddr) buf;
 
     /* issue the command+SYS5 atomically */
     disableInterrupts();
     ((devregarea_t *)RAMBASEADDR)->devreg[idx].d_command = cmd;
     int st = SYSCALL(WAITIO, line, dev, /* isRead? */ (cmd & READBLK) != 0);
     enableInterrupts();
 
     /* V(devSemaphore[idx]) */
     mutex(&(devSemaphore[idx]), FALSE);
     return st;
 }
 
 /* SYS14: DISK PUT */
 int diskPut(char *virtAddr, int diskNo, int sectNo) {
     char *buf = diskBuf[diskNo];
     copyUserToBuf(virtAddr, buf);
     int cmd = (sectNo << DISKSHIFT) | WRITEBLK;
     int st = dmaOp(cmd, DISKINT, diskNo, buf);
     return (st == DEVREADY ? DEVREADY : -st);
 }
 
 /* SYS15: DISK GET */
 int diskGet(char *virtAddr, int diskNo, int sectNo) {
     char *buf = diskBuf[diskNo];
     int cmd = (sectNo << DISKSHIFT) | READBLK;
     int st = dmaOp(cmd, DISKINT, diskNo, buf);
     if (st == DEVREADY) {
         copyBufToUser(virtAddr, buf);
         return DEVREADY;
     } else {
         return -st;
     }
 }
 
 /* SYS16: FLASH PUT */
 int flashPut(char *virtAddr, int flashNo, int blockNo) {
     char *buf = flashBuf[flashNo];
     copyUserToBuf(virtAddr, buf);
     int cmd = (blockNo << BLKNUMSHIFT) | WRITEBLK;
     int st = dmaOp(cmd, FLASHINT, flashNo, buf);
     return (st == DEVREADY ? DEVREADY : -st);
 }
 
 /* SYS17: FLASH GET */
 int flashGet(char *virtAddr, int flashNo, int blockNo) {
     char *buf = flashBuf[flashNo];
     int cmd = (blockNo << BLKNUMSHIFT) | READBLK;
     int st = dmaOp(cmd, FLASHINT, flashNo, buf);
     if (st == DEVREADY) {
         copyBufToUser(virtAddr, buf);
         return DEVREADY;
     } else {
         return -st;
     }
 }
 