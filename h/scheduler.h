#ifndef SCHEDULER
#define SCHEDULER

/**************************************************************************** 
 *
 * Externals for the Scheduler module.
 *
 * Written by: Luka Bagashvili, Rosalie Lee
 *
 ****************************************************************************/
#include "../h/types.h"
#include "../h/pcb.h"

/* 
 * The scheduler provides:
 *   - switchProcess(): Preemptive round-robin scheduling.
 *   - loadProcessorState(): Set currentProc & load state.
 *   - moveState(): Copy CPU state from one area to another.
 */
extern void switchProcess(void);
extern void loadProcessorState(pcb_PTR curr_proc);
extern void moveState(state_PTR source, state_PTR dest);

#endif
