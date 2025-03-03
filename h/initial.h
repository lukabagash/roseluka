#ifndef INITIAL
#define INITIAL

/**************************************************************************** 
 *
 * The externals declaration file for the Initial.c global variables.
 *
 * Written by: Luka Bagashvili, Rosalie Lee
 *
 ****************************************************************************/
#include "../h/const.h"
#include "../h/types.h"

/* 
 * Phase 2 Globals (as actually used in initial.c):
 *   - The ‘processCount’ and ‘softBlockedCount’ track active processes.
 *   - ‘currentProcess’ is the PCB of the process currently running.
 *   - ‘readyQueue’ is the tail pointer to the ready queue of PCBs.
 *   - ‘startTOD’ stores the Time of Day at which the Current Process started.
 *   - ‘devSemaphore[]’ holds one semaphore per external device + 1 pseudo-clock.
 *   - ‘savedExceptState’ stores the CPU state at the time of an exception.
 */

extern int       processCount;
extern int       softBlockedCount;
extern pcb_PTR   currentProcess;
extern pcb_PTR   readyQueue;
extern cpu_t     startTOD;
extern int       devSemaphore[MAXDEVICECNT];
extern state_PTR savedExceptState;

#endif
