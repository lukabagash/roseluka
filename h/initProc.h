#ifndef INITPROC
#define INITPROC

/**************************************************************************** 
 *
 * The externals declaration file for the module that implements the test()
 * function and declares and initializes the phase 3 global variables, which
 * include an array of device semaphores and the masterSemaphore responsible
 * for ensuring test() comes to a more graceful conclusion by calling HALT()
 * instead of PANIC()
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 * 
 ****************************************************************************/

#include "../h/const.h"
#include "../h/types.h"

extern int masterSemaphore; /* Private semaphore for graceful conclusion/termination of the test function */
extern int devSemaphores[48]; /* array of device semaphores for the various peripheral I/O devices (Disk, Flash, Network, Printer, Terminals) */
/* This array contains 48 semaphores: 32 for the peripheral I/O devices (4 classes × 8 devices) and 16 for the terminal devices (8 terminals × 2 semaphores) */
extern void test();

#endif
