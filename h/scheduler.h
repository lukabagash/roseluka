#ifndef SCHEDULER
#define SCHEDULER

/**************************************************************************** 
 *
 * The externals declaration file for the Scheduler module
 * 
 * Written by: Luka Bagashvili, Rosalie Lee 
 * 
 ****************************************************************************/

extern void switchProcess ();
extern void loadProcessorState (pcb_PTR curr_proc);
extern void moveState (state_PTR source, state_PTR dest);

#endif