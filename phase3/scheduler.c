/******************************** scheduler.c **********************************
 *
 * This module implements the Scheduler and the Deadlock Detector.
 *   1. Implements a preemptive round-robin scheduling algorithm with a 
 *      time slice of five milliseconds.
 *   2. Ensures every ready process gets an opportunity to execute.
 *   3. If the Ready Queue is not empty, the scheduler removes the PCB at the 
 *      head of the Ready Queue and assigns it to the Current Process field.
 *   4. Loads five milliseconds on the processor’s Local Timer before performing 
 *      an LDST on the processor state stored in the PCB of the Current Process.
 *   5. If the Ready Queue is empty:
 *      - If Process Count is zero, invokes the HALT BIOS instruction.
 *      - If Process Count > 0 and Soft-block Count > 0, enters a Wait State.
 *      - If Process Count > 0 and Soft-block Count == 0, invokes PANIC BIOS 
 *        instruction to handle deadlock.
 *   6. Provides utility functions such as:
 *      - moveState(): Copies the processor state from one location to another.
 *      - loadProcessorState(): Loads the processor state of the Current Process.
 *
 *  Written by Luka Bagashvili, Rosalie Lee
 **************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "../h/initial.h"
#include "../h/initProc.h"
#include "/usr/include/umps3/umps/libumps.h"


void debugSch(int a, int b, int c, int d) {
	/* Debugging function to print values */
	int i;
	i = 42;
	i++;
}

/************************************************************************
 * moveState - Copies a processor state from source to destination.
 *
 *   - Copies key fields including entryHI, cause, status, and PC.
 *   - Copies all general-purpose registers.
 *   - Useful for handling non-blocking SYSCALL exceptions and interrupts.
 ************************************************************************/
void moveState(state_PTR source, state_PTR dest) {
	int i;
	debugSch(0x5d, source->s_entryHI, source->s_cause, source->s_status);
	dest->s_entryHI = source->s_entryHI;
	dest->s_cause = source->s_cause;
	dest->s_status = source->s_status;
	dest->s_pc = source->s_pc;

	/* Copy all general-purpose registers */
	for (i = 0; i < STATEREGNUM; i++) {
		dest->s_reg[i] = source->s_reg[i];
	}
}

/************************************************************************
 * loadProcessorState - Sets Current Process and loads its processor state.
 *
 *   - Updates the Current Process.
 *   - Captures the start time from the Time of Day clock.
 *   - Performs an LDST to load the processor state.
 ************************************************************************/
void loadProcessorState(pcb_PTR curr_proc) {
	currentProcess = curr_proc;
	STCK(startTOD);  /* Store Time of Day when process starts execution */
	LDST(&(curr_proc->p_s));  /* Load processor state for execution */
}

/************************************************************************
 * switchProcess - Implements a preemptive round-robin scheduling algorithm.
 *
 *   - Removes the PCB at the head of the Ready Queue.
 *   - Loads five milliseconds on the processor’s Local Timer (PLT).
 *   - Calls loadProcessorState() to perform an LDST.
 *   - If the Ready Queue is empty:
 *     - If Process Count == 0, calls HALT().
 *     - If Process Count > 0 and Soft-block Count > 0, enters Wait State.
 *     - If Process Count > 0 and Soft-block Count == 0, calls PANIC().
 ************************************************************************/
void switchProcess() {
currentProcess = removeProcQ(&readyQueue);
	if (currentProcess != NULL) {
		setTIMER(INITIALPLT);  /* Load five milliseconds on the PLT */
		loadProcessorState(currentProcess);  /* Load process state for execution */
	}

	/* Ready Queue is empty */
	if (processCount == INITPROCCOUNT) {
		HALT();  /* Halt system if no active processes */
	}

	/* The processor is not executing instructions, but waiting for a device interrupt to occur */
	if ((processCount > INITPROCCOUNT) && (softBlockedCount > INITSOFTBLKCOUNT)) {
		setSTATUS(ALLOFF | PANDOS_CAUSEINTMASK | IECON); /* Enable interrupts for the Status register so we can execute the WAIT instruction */
		setTIMER(NEVER);  /* Set a high timer value to wait for device interrupt */
		WAIT();  /* Enter wait state */
	}

	/* Deadlock detected: (processCount > 0) && (softBlockedCount == 0) */
	PANIC();  /* Halt system with a warning message */
}
