/************************ deviceSupportDMA.c ************************
 * Phase 4 / Level 5 DMA Sys14, Sys15, Sys16, Sys17
 * To perform a disk/flash read operation:
 *  1. The requested disk sector/flash block is read into the device’s DMA buffer.
 *  2. The data is copied from the DMA buffer into the requesting U-proc’s address space starting from the provided start address.
 * A write operation is isomorphic, only the two steps are reversed:
 *  1. The data is copied from the requesting U-proc’s address space into the device’s DMA buffer.
 *  2. The targeted disk sector/flash block is overwritten with the contents of theDMA buffer.
 * Luka Bagashvili, Rosalie Lee
 **************************************************************************/

#include "../h/deviceSupportDMA.h"
#include "../h/const.h"        
#include "../h/initProc.h"     /* p3devSemaphore[] */
#include "../h/vmSupport.h"    /* disableInterrupts/enableInterrupts, schizoUserProcTerminate() */
#include "../h/scheduler.h"    
#include "../h/sysSupport.h"   /* schizoUserProcTerminate() */
#include "../h/types.h"       /* devregarea_t, device_t, state_PTR */
#include "/usr/include/umps3/umps/libumps.h"

/* Sixteen 4 KB frames: [0..7]=disks, [8..15]=flash */
static char dmaBufs[TOTAL_DMA_BUFFS][PAGESIZE];

/* Copy 4KB from user-provided virtual address into the DMA buffer before disk/flash write */
static void copyUserToBuf(char *u, char *buf) {
int i;
    for ( i = 0; i < PAGESIZE; i++) buf[i] = u[i];
}
/* Copy 4KB from DMA buffer into user virtual address space after disk/flash read */
static void copyBufToUser(char *u, char *buf) {
int i;
    for ( i = 0; i < PAGESIZE; i++) u[i] = buf[i];
}

/*
* This function performs the actual disk operation (read/write) on the specified disk.
* First obtains the max number of sectors, heads, and cylinders from the data 1.
* It then calculates the exact cylinder, head, and sector number for the requested sector sectNo.
*
* operation: DISKREAD or DISKWRITE
* diskNo: the disk number (0-7)
* sectNo: the disk sector number to be read from or written to
* buffer: the buffer to read from or write to
*/
static int diskOperation(int operation, int diskNo, int sectNo, char *buffer) {

    int idx  = ((DISKINT - OFFSET) * DEVPERINT) + diskNo;   /* Get device index from diskNo */
    devregarea_t *regs = (devregarea_t *)RAMBASEADDR;   /* Pointer to device register base */
    device_t     *dev  = &regs->devreg[idx];    /* Pointer to specific disk device */

    /* For a given disk, the number of sectors = disk’s maxcyl ∗ maxhead ∗ maxsect, 
    which are found in the device’s DATA1 device register field. */
    unsigned geom    = regs->devreg[idx].d_data1;
    int maxSect      =  geom & MAXSECTMASK;                     /* obatin max sect bits from 0 to 7*/
    int maxHead      = (geom & MAXHEADMASK) >> DISKHEADSHIFT;   /* obtain max head bits from 8 to 15*/
    int maxCyl       =  geom >> DISKCYLSHIFT;                   /* obtain max cyl bits from 16 to 31*/
    long totalSects  = (long)maxCyl * maxHead * maxSect;

    /* Validate the number sectors */
    if (sectNo < 0 || sectNo > totalSects) {
        schizoUserProcTerminate(NULL);
    }

    /* lock the device & buffer */
    mutex(&p3devSemaphore[idx], TRUE);
    dev->d_data0 = (unsigned) buffer;   /* Provide DMA buffer address to device?? yes pandos:5.5.1*/

    /* break linear sector into (cyl, head, sec) */
    int cyl  = sectNo / (maxHead * maxSect); /* Since each cylinder contains maxHead * maxSect sectors, integer division gives the cylinder index */
    int tmp  = sectNo % (maxHead * maxSect); /* Obtain num of sect within our current cyl*/
    int head = tmp / maxSect; /* Obtain which head within our sect*/
    int sec  = tmp % maxSect; /* Obtain the sect within head*/

    /* A disk seek operation, and corresponding SYS5, done atomically. */
    disableInterrupts();

    dev->d_command = (cyl << DISKSHIFT) | DISKSEEK;
    int st = SYSCALL(WAITIO, DISKINT, diskNo, FALSE);

    enableInterrupts();

    /* Check status from seek */
    if (st != DEVREDY) {
        mutex(&p3devSemaphore[idx], FALSE);
        return -st;
    }

    /* Disk read (or write) and its corresponding SYS5, also done atomically.*/
    disableInterrupts();
    dev->d_command = (head << HEAD_SHIFT)
                | (sec  << DISKSHIFT)
                | operation;   /* operation == DISKREAD or DISKWRITE */
    st = SYSCALL(WAITIO, DISKINT, diskNo, operation == DISKREAD);
    enableInterrupts();

    mutex(&p3devSemaphore[idx], FALSE);
    return (st == DEVREDY ? DEVREDY : -st); /* if the device finished successfully, return 1. Otherwise, return the negative of the completion status */
}

/* SYS 14 - This service causes the requesting U-proc to be suspended until 
 * the disk write operation (both the seek and the write) has concluded. 
 * 
 * savedState: to place the completion status of the disk operation
 * virtAddr: the logical address of the 4KB area to be written to the disk
 * diskNo: the disk number ([0. . .7]) 
 * sectNo: the disk sector number
 */
void diskPut(state_PTR savedState, char *virtAddr, int diskNo, int sectNo) {
    char *buf = dmaBufs[diskNo];

    /* Firat copy user memory into DMA buffer and then perform disk write */
    copyUserToBuf(virtAddr, buf);   
    int st = diskOperation(DISKWRITE, diskNo, sectNo, buf); 

    savedState->s_v0 = st;  /* Place the completion status of the disk operation */
    LDST(savedState);
}


/* SYS 15 - This service causes the requesting U-proc to be suspended until 
 * the disk read operation (both the seek and the read) has concluded.
 * 
 * savedState: to place the completion status of the disk operation
 * virtAddr: the logical address of the 4KB area to contain the data from the disk
 * diskNo: the disk number ([0. . .7]) 
 * sectNo: the disk sector number to be read from
 */
void diskGet(state_PTR savedState, char *virtAddr, int diskNo, int sectNo) {
    char *buf = dmaBufs[diskNo];    

    /* First perform disk read and then copy to user memory if successful */
    int st = diskOperation(DISKREAD, diskNo, sectNo, buf);  
    if (st == DEVREDY) copyBufToUser(virtAddr, buf);    

    savedState->s_v0 = st;  /* Place the completion status of the disk operation */
    LDST(savedState);
}
 
/* Migrated from vmSupport, modified to return DEVEREDY or negative status for dma compatibility.
 * To accomodate the modification and migration, vmSupport has been updated to capture anything besides DEVREDY and terminate.
 * Flash operation function to perform read/write operations on flash devices.
 * 
 * asid: the ASID of the requesting U-proc
 * pageBlock: the block number to be read/written
 * frameAddr: the address of the 4KB area in RAM
 * operation: READBLK (2) or WRITEBLK (3)
 */
int flashOperation(int asid, int pageBlock, int frameAddr, unsigned int operation)
{
    /* Identify the flash device with ASID */
    int idx = ((FLASHINT - OFFSET) * DEVPERINT) + (asid - 1);
    devregarea_t *devReg = (devregarea_t *) RAMBASEADDR;    /* Pointer to device register base */
    device_t *flashDev = &(devReg->devreg[idx]);   /* Pointer to flash device register */

    mutex(&p3devSemaphore[idx], TRUE); /* Gain mutual exclusion from the device semaphore */

    flashDev->d_data0 = frameAddr; /* Write the frame address to d_data0 */
    disableInterrupts(); /* Disable interrupts to atomically write the command field, and call SYS5 */
    flashDev->d_command = (pageBlock << FLASCOMHSHIFT) | operation;
    SYSCALL(WAITIO, FLASHINT, (asid - 1), (operation == READBLK));
    enableInterrupts();

    mutex(&p3devSemaphore[idx], FALSE); /* Release mutual exclusion from the device semaphore */

    int st = flashDev->d_status;

    return (st == DEVREDY ? DEVREDY : -st); /* if the device finished successfully, return 1. Otherwise, return the negative of the completion status */
}

/* SYS 16 - This service causes the requesting U-proc to be suspended until 
 * the flash write operation has concluded.

 * savedState: to place the completion status of the flash operation
 * virtAddr: the logical address of the 4KB area to be written to the flash device
 * diskNo: the flash device number ([0. . .7]) 
 * blcohNo: the block number to be written onto
 */
void flashPut(state_PTR savedState, char *virtAddr, int flashNo, int blockNo) {
    /* terminate if write to (read from) a disk device from (into) an address outside of the 
    requesting U-proc’s logical address space is an error */
    if ((unsigned int)virtAddr < KUSEG || (unsigned int)virtAddr >= STCKTOPEND) {
        schizoUserProcTerminate(NULL); 
    }
    int idx = ((FLASHINT - OFFSET) * DEVPERINT) + (flashNo);   /* Get device index from diskNo */
    
    devregarea_t *devReg = (devregarea_t *) RAMBASEADDR;    /* Pointer to device register base */
    device_t *flashDev = &(devReg->devreg[idx]);   /* Pointer to flash device register */

    unsigned int maxblock = flashDev->d_data1 && 0x00000fff;
    
    /* terminate if write to (read from) a block outside of [32..(MAXBLOCK- 1)] */
    if (blockNo < 32 || blockNo > maxblock) {
        schizoUserProcTerminate(NULL);
    }
    char *buf = dmaBufs[DISK_DMA_COUNT + flashNo];

    /* First copy user memory into DMA buffer and then perform flash write */
    copyUserToBuf(virtAddr, buf);
    int st = flashOperation(flashNo + 1, blockNo, (int)buf, WRITEBLK);

    savedState->s_v0 = st;
    LDST(savedState);
}

/* SYS 17 - This service causes the requesting U-proc to be suspended until 
 * the flash read op- eration has concluded.
 * 
 * savedState: to place the completion status of the flash operation
 * virtAddr: the logical address of the 4KB area to contain the data from the flash device
 * diskNo: the flash device number ([0. . .7]) 
 * blcohNo: the block number to be read from
 */
void flashGet(state_PTR savedState, char *virtAddr, int flashNo, int blockNo) {
    /* terminate if write to (read from) a disk device from (into) an address outside of the 
    requesting U-proc’s logical address space is an error */
    if ((unsigned int)virtAddr < KUSEG || (unsigned int)virtAddr >= STCKTOPEND) {
        schizoUserProcTerminate(NULL); 
    }
    int idx = ((FLASHINT - OFFSET) * DEVPERINT) + (flashNo);

    devregarea_t *devReg = (devregarea_t *) RAMBASEADDR;    /* Pointer to device register base */
    device_t *flashDev = &(devReg->devreg[idx]);   /* Pointer to flash device register */

    unsigned int maxblock = flashDev->d_data1 && 0x00000fff;
    
    /* terminate if write to (read from) a block outside of [32..(MAXBLOCK- 1)] */
    if (blockNo < 32 || blockNo > maxblock) {
        schizoUserProcTerminate(NULL);
    }
    char *buf = dmaBufs[DISK_DMA_COUNT + flashNo];

    /* First perform flash read and then copy to user memory if successful */
    int st = flashOperation(flashNo + 1, blockNo, (int)buf, READBLK);
    if (st == DEVREDY) copyBufToUser(virtAddr, buf);

    savedState->s_v0 = st;
    LDST(savedState);
}
