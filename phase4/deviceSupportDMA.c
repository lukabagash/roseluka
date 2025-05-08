/************************ deviceSupportDMA.c ************************
 * Phase 4 / Level 5 DMA support with correct bit‐fields for SEEK/READ/WRITE
 * Luka Bagashvili, Rosalie Lee + ChatGPT corrections
 **************************************************************************/

 #include "../h/deviceSupportDMA.h"
 #include "../h/const.h"        /* DISKINT, FLASHINT, DISKSHIFT, DISKSEEK, DISKREAD, DISKWRITE,
                                  FLASCOMHSHIFT, READBLK, WRITEBLK, DEVREDY, LOWERMASK,
                                  HEADMASK, CYLADDRSHIFT, HEADADDRSHIFT */
 #include "../h/initProc.h"     /* p3devSemaphore[] */
 #include "../h/vmSupport.h"    /* disableInterrupts/enableInterrupts, schizoUserProcTerminate() */
 #include "../h/scheduler.h"    /* SYSCALL */
 #include "../h/sysSupport.h"   /* schizoUserProcTerminate() */
 #include "/usr/include/umps3/umps/libumps.h"

 void debugDMA(int a, int b, int c, int d) {
     int i =0;
     i++;
 }
 
 /* head is bits 23–16, sector bits 15–8 */
 #define HEAD_SHIFT (DISKSHIFT + 8)  /* 8+8 = 16 */
 
 /* Sixteen 4 KB frames: [0..7]=disks, [8..15]=flash */
 static char dmaBufs[TOTAL_DMA_BUFFS][PAGESIZE];
 
 /* Copy 4 KB from user into DMA buffer */
 static void copyUserToBuf(char *u, char *buf) {
    int i;
     for ( i = 0; i < PAGESIZE; i++) buf[i] = u[i];
 }
 /* Copy 4 KB from DMA buffer back to user */
 static void copyBufToUser(char *u, char *buf) {
    int i;
     for ( i = 0; i < PAGESIZE; i++) u[i] = buf[i];
 }
 
 /*
  * diskDmaOp
  *   1) SEEKCYL to (cylinder)
  *   2) READBLK or WRITEBLK to (head,sector)
  *
  * Bits layout (Figure 5.4):
  *   [31..24]=–, [23..16]=HEADNUM, [15..8]=SECTNUM, [7..0]=opcode
  * SEEKCYL: [23..8]=CYLNUM, [7..0]=2
  */
 static int diskOperation(int operation, int diskNo, int sectNo, char *buffer) {
     int idx  = ((DISKINT - OFFSET) * DEVPERINT) + diskNo;
     devregarea_t *regs = (devregarea_t *)RAMBASEADDR;
     device_t     *dev  = &regs->devreg[idx];
    
     unsigned geom    = regs->devreg[idx].d_data1;
     int maxSect      =  geom & MAXSECTMASK;
     int maxHead      = (geom & MAXHEADMASK) >> DISKHEADSHIFT;
     int maxCyl       =  geom >> DISKCYLSHIFT;
     long totalSects  = (long)maxCyl * maxHead * maxSect;
     if (sectNo < 0 || sectNo > totalSects) {
         schizoUserProcTerminate(NULL);
     }
 
     /* lock the device & buffer */
     mutex(&p3devSemaphore[idx], TRUE);
     dev->d_data0 = (unsigned) buffer;
 
     /* break linear sector into (cyl, head, sec) */
     int cyl  = sectNo / (maxHead * maxSect);
     int tmp  = sectNo % (maxHead * maxSect);
     int head = tmp / maxSect;
     int sec  = tmp % maxSect;
 
     /* --- Phase 1: SEEKCYL (opcode = DISKSEEK == 2) --- */
     disableInterrupts();
     dev->d_command = (cyl << DISKSHIFT) | DISKSEEK;
     int st = SYSCALL(WAITIO, DISKINT, diskNo, /* read? */ FALSE);
     enableInterrupts();
     if (st != DEVREDY) {
         mutex(&p3devSemaphore[idx], FALSE);
         return -st;
     }
 
     /* --- Phase 2: READBLK (3) or WRITEBLK (4) --- */
     disableInterrupts();
     dev->d_command = (head << HEAD_SHIFT)
                    | (sec  << DISKSHIFT)
                    | operation;   /* operation == DISKREAD or DISKWRITE */
     st = SYSCALL(WAITIO, DISKINT, diskNo, operation == DISKREAD);
     enableInterrupts();
 
     mutex(&p3devSemaphore[idx], FALSE);
     return (st == DEVREDY ? DEVREDY : -st);
 }
 
 void diskPut(state_PTR savedState, char *virtAddr, int diskNo, int sectNo) {
     char *buf = dmaBufs[diskNo];
     copyUserToBuf(virtAddr, buf);
     int status = diskOperation(DISKWRITE, diskNo, sectNo, buf);
     savedState->s_v0 = status;
     LDST(savedState);
 }
 
 int diskGet(char *virtAddr, int diskNo, int sectNo) {
     char *buf = dmaBufs[diskNo];
     int st = diskOperation(DISKREAD, diskNo, sectNo, buf);
     if (st == DEVREDY) copyBufToUser(virtAddr, buf);
     return st;
 }
 
 /*
  * flashDmaOp  — single‐phase DMA
  * Bits [31..8]=BLOCKNUM, [7..0]=opcode (READBLK=2 or WRITEBLK=3)
  */
 static int flashOperation(int operation, int flashNo, int blockNo, char *buffer) {
     int idx  = ((FLASHINT - OFFSET) * DEVPERINT) + flashNo;
     devregarea_t *regs = (devregarea_t *)RAMBASEADDR;
     device_t     *dev  = &regs->devreg[idx];
 
     mutex(&p3devSemaphore[idx], TRUE);
     dev->d_data0 = (unsigned) buffer;
 
     disableInterrupts();
     dev->d_command = (blockNo << FLASCOMHSHIFT) | operation;
     int st = SYSCALL(WAITIO, FLASHINT, flashNo, operation == READBLK);
     enableInterrupts();
 
     mutex(&p3devSemaphore[idx], FALSE);
     return (st == DEVREDY ? DEVREDY : -st);
 }
 
 int flashPut(char *virtAddr, int flashNo, int blockNo) {
     char *buf = dmaBufs[DISK_DMA_COUNT + flashNo];
     copyUserToBuf(virtAddr, buf);
     return flashOperation(WRITEBLK, flashNo, blockNo, buf);
 }
 
 int flashGet(char *virtAddr, int flashNo, int blockNo) {
     char *buf = dmaBufs[DISK_DMA_COUNT + flashNo];
     int st = flashOperation(READBLK, flashNo, blockNo, buf);
     if (st == DEVREDY) copyBufToUser(virtAddr, buf);
     return st;
 }
 