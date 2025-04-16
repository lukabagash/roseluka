/******************************** interrupts.c **********************************
 *
 * This module handles all interrupt-related processing within the Nucleus.
 * 
 *   - Determines the highest-priority pending interrupt and dispatches the
 *     appropriate handler function (PLT, Interval Timer, or I/O).
 *   - If multiple interrupts are pending, they are handled one at a time in 
 *     descending priority order.
 *   - CPU time used while handling the interrupt is generally charged to the 
 *     process responsible for generating the interrupt, with specific rules:
 *       * I/O interrupt handling time is charged to the process that initiated 
 *         the I/O request.
 *       * PLT interrupt handling time is charged to the Current Process.
 *       * System-wide Interval Timer interrupt handling time is not charged to 
 *         any process, since the Current Process is not responsible for the 
 *         interrupt generation.
 *   - If a process is unblocked due to an I/O device interrupt, its return 
 *     status is stored in the v0 register, and it is placed on the Ready Queue.
 *   - Once interrupt processing completes, control returns to the Current 
 *     Process or the Scheduler is invoked if no Current Process exists.
 *
 *	Written by Rosalie Lee, Luka Bagashvili
 ****************************************************************************/

 #include "../h/asl.h"
 #include "../h/types.h"
 #include "../h/const.h"
 #include "../h/pcb.h"
 #include "../h/scheduler.h"
 #include "../h/interrupts.h"
 #include "../h/exceptions.h"
 #include "../h/initial.h"
 #include "/usr/include/umps3/umps/libumps.h"

 HIDDEN int findDeviceNum(int lineNumber); /* function to find the device number associated with the highest-priority interrupt */
 HIDDEN void IOInt(); /* function to handle I/O interrupts */
 HIDDEN void pltTimerInt(); /* function to handle PLT interrupts */
 HIDDEN void intTimerInt(); /* function to handle System-wide Interval Timer interrupts */

/* Global Variables (from this module’s perspective) */
cpu_t interruptTOD; /* the value on the Time of Day clock when the Interrupt Handler module is first entered */
cpu_t remainingTime; /* the amount of time left on the Current Process' quantum when the interrupt was generated */

 /************************************************************************
  * findDeviceNum - Identifies the device number for the highest-priority 
  *                 pending interrupt on a given interrupt line.
  *
  *   - Examines the Interrupt Devices Bit Map associated with the specified
  *     line to detect which device (0-7) is responsible for the interrupt.
  *   - Returns the device number (0 through 7) corresponding to the highest-
  *     priority pending interrupt on that line.
  ************************************************************************/
 HIDDEN int findDeviceNum(int lineNumber) {
	 devregarea_t *temp; /* device register area to help find device number */
	 unsigned int bitMap; /* bit map for the specified interrupt line */
 
	 /* Initialize temp and bitMap to determine which device number is associated with the highest-priority interrupt */
	 temp = (devregarea_t *) RAMBASEADDR; 
	 bitMap = temp->interrupt_dev[lineNumber - OFFSET];
 
	 /* Determine which device number is associated with the highest-priority interrupt based on the address of bitMap */
	 if ((bitMap & DEV0INT) != ALLOFF) {
		 return DEV0;
	 }
	 if ((bitMap & DEV1INT) != ALLOFF) {
		 return DEV1;
	 }
	 if ((bitMap & DEV2INT) != ALLOFF) {
		 return DEV2;
	 }
	 if ((bitMap & DEV3INT) != ALLOFF) {
		 return DEV3;
	 }
	 if ((bitMap & DEV4INT) != ALLOFF) {
		 return DEV4;
	 }
	 if ((bitMap & DEV5INT) != ALLOFF) {
		 return DEV5;
	 }
	 if ((bitMap & DEV6INT) != ALLOFF) {
		 return DEV6;
	 }
	 /* default to device 7 if not found above */
	 return DEV7;
 }
 
 /************************************************************************
  * IOInt - Handles I/O interrupts occurring on interrupt lines 3-7. 
  *
  *   - Determines the line number (3-7) and the specific device (0-7) 
  *     that caused the highest-priority interrupt.
  *   - Stores off the device status code and acknowledges the interrupt 
  *     by writing the acknowledge command to the device register.
  *   - Performs a V operation on the semaphore for the device, unblocking 
  *     the waiting process (if any), placing its status code in v0, and 
  *     moving it to the Ready Queue.
  *   - Decrements the softBlockedCount when a process is unblocked.
  *   - Charges the CPU time spent handling the interrupt to the process
  *     responsible for generating the I/O request if that process exists.
  ************************************************************************/
 HIDDEN void IOInt() {
	 cpu_t currentTOD;
	 int lineNum;			/* the line number that the highest-priority interrupt occurred on */
	 int devNum;				/* the device number that the highest-priority interrupt occurred on */
	 int index;				/* the index in device register of the device associated with the highest-priority interrupt */
	 devregarea_t *temp;		/* Device register area that we can use to determine the status code */
	 int statusCode;			/* The status code from the device register associated with the device that corresponds to the highest-priority interrupt */
	 pcb_PTR unblockedPcb;	/* pcb originally initiated the I/O request */
 
	 /* Determine exactly which line number the interrupt occurred, to initialize lineNum */
	 if (((savedExceptState->s_cause) & LINE3INT) != ALLOFF) {
		 lineNum = LINE3;
	 }
	 else if (((savedExceptState->s_cause) & LINE4INT) != ALLOFF) {
		 lineNum = LINE4;
	 }
	 else if (((savedExceptState->s_cause) & LINE5INT) != ALLOFF) {
		 lineNum = LINE5;
	 }
	 else if (((savedExceptState->s_cause) & LINE6INT) != ALLOFF) {
		 lineNum = LINE6;
	 }
	 else {
		 lineNum = LINE7;
	 }
 
	 devNum = findDeviceNum(lineNum); /* Initialize with the device number associated with the highest-priority interrupt */	
	 index = ((lineNum - OFFSET) * DEVPERINT) + devNum; /* Initialize the index in devSemaphore of the device associated with the highest-priority interrupt */
	 temp = (devregarea_t *) RAMBASEADDR; /* initialization of temp */
	 
	 /* if the highest-priority interrupt occurred on line 7 and if the device's status code is not 1, the device is not "Ready" (write interrupt) */
	 if ((lineNum == LINE7) && (((temp->devreg[index].t_transm_status) & STATUSON) != READY)){
		 /* Terminal write interrupt */ 
		 statusCode = temp->devreg[index].t_transm_status;
		 temp->devreg[index].t_transm_command = ACK;

		 /* Perform V operation on the "write" semaphore (index + DEVPERINT) */
		 unblockedPcb = removeBlocked(&devSemaphore[index + DEVPERINT]);
		 devSemaphore[index + DEVPERINT]++; 
	 }
	 /* Otherwise, the highest-priority interrupt either did not occur on a terminal device or it was a read interrupt on a terminal device */
	 else{
		 /* Terminal read interrupt, or a non-terminal device interrupt */ 
		 statusCode = temp->devreg[index].t_recv_status;
		 temp->devreg[index].t_recv_command = ACK;

		 /* Perform V operation on the "read" semaphore (index) */
		 unblockedPcb = removeBlocked(&devSemaphore[index]);
		 devSemaphore[index]++;
	 }
	 
	 /* If no process was blocked on this device, just return to current or next process. */
	 if (unblockedPcb == NULL){
		 if (currentProcess != NULL){
			 updateCurrentProcessState();
			 currentProcess->p_time = currentProcess->p_time + (interruptTOD - startTOD); /* charge CPU time to Current Process */
			 setTIMER(remainingTime);
			 loadProcessorState(currentProcess); /* Return control to the Current Process */
		 }
		 switchProcess(); /* Execute the next process on the Ready Queue if there is no Current Process */
	 }
	 
	 /* unblockedPcb is not NULL */
	 unblockedPcb->p_s.s_v0 = statusCode; /* Place the status code in the newly unblocked pcb's v0 register */
	 insertProcQ(&readyQueue, unblockedPcb); /* Turn "blocked" state to a "ready" state */
	 softBlockedCount--; 
	 /* if there is a Current Process to return control to */
	 if (currentProcess != NULL){ 
		 updateCurrentProcessState(); /* Update the Current Process' processor state before resuming process' execution */
		 setTIMER(remainingTime); /* Set the PLT to the remaining time left on the Current Process' quantum when the interrupt handler was first entered */
		 currentProcess->p_time = currentProcess->p_time + (interruptTOD - startTOD); /* Update the accumulated processor time used by the Current Process */
		 STCK(currentTOD); /* Store the current value on the Time of Day clock into currentTOD */
		 unblockedPcb->p_time = unblockedPcb->p_time + (currentTOD - interruptTOD); /* Charge the process associated with the I/O interrupt with the CPU time needed */
		 loadProcessorState(currentProcess); /* Return control to the Current Process */
	 }
	 switchProcess(); /* Execute the next process on the Ready Queue if there is no Current Process */
 }
 
 /************************************************************************
  * pltTimerInt - Handles Processor Local Timer (PLT) interrupts on line 1.
  *
  *   - Copies the saved processor state into the Current Process’s PCB.
  *   - Adds the CPU time from when the process began executing to now 
  *     into the Current Process’s p_time.
  *   - Places the Current Process back on the Ready Queue, as its quantum 
  *     has expired.
  *   - Invokes the Scheduler to select another process to execute.
  *   - If there is no Current Process, the system triggers a PANIC.
  ************************************************************************/
 HIDDEN void pltTimerInt() {
	 cpu_t currentTOD;
 
	 /* if there was a running process when the interrupt was generated */
	 if (currentProcess != NULL) {
		 setTIMER(NEVER);	/* PLT will generate an interrupt on interrupt line 1, so it doesn't call PLT again */
		 updateCurrentProcessState();	/* Move the updated exception state from the BIOS Data Page into the Current Process' processor state */
		 STCK(currentTOD);		/* Store the current value on the Time of Day clock into currentTOD */
		 currentProcess->p_time += (currentTOD - startTOD);	/* Update the accumulated processor time */
		 insertProcQ(&readyQueue, currentProcess);	/* Place back the Current Process on the Ready Queue */
		 currentProcess = NULL;	/* No process currently executing */
		 switchProcess();	/* Call scheduler */
	 }
	 PANIC();	/* if current process is NULL */
 }
 
 /************************************************************************
  * intTimerInt - Handles the System-wide Interval Timer (line 2).
  *
  *   - Reloads the Interval Timer with 100ms (CLOCKINTERVAL).
  *   - Performs a V operation on the pseudo-clock semaphore, unblocking 
  *     all processes waiting on it.
  *   - Resets the pseudo-clock semaphore to 0.
  *   - Returns to the Current Process with its remaining quantum, or 
  *     calls switchProcess() if no Current Process is available.
  *   - Time spent handling this interrupt is not charged to any process.
  ************************************************************************/
 HIDDEN void intTimerInt() {
	 pcb_PTR temp; /* Pointer to a pcb in the Pseudo-Clock semaphore's process queue that we want to unblock and insert into the Ready Queue */
	 
	 LDIT(PANDOS_CLOCKINTERVAL); /* Place 100 milliseconds back on the Interval Timer for the next Pseudo-clock tick */
	 
	 /* unblocking all pcbs blocked on the Pseudo-Clock semaphore */
	 while (headBlocked(&devSemaphore[PCLOCKIDX]) != NULL){ 
		 temp = removeBlocked(&devSemaphore[PCLOCKIDX]); /* Unblock from the first pcb from the Pseudo-Clock semaphore's process queue */
		 insertProcQ(&readyQueue, temp); /* Place the unblocked pcb back on the Ready Queue */
		 softBlockedCount--; 
	 }
	 devSemaphore[PCLOCKIDX] = INITIALPCSEM; /* Reset the Pseudo-clock semaphore */
	 /* if there is a Current Process to return control to */
	 if (currentProcess != NULL){ 
		 setTIMER(remainingTime); /* The remaining time left on the Current Process' quantum */
		 updateCurrentProcessState(); /* Update before resuming process' execution */
		 currentProcess->p_time = currentProcess->p_time + (interruptTOD - startTOD); /* Update the accumulated processor time used by the Current Process */
		 loadProcessorState(currentProcess); /* Return control to the Current Process */
	 }
	 switchProcess(); /* call scheduler if there is no Current Process to return control to */
 }
 
 
 /************************************************************************
  * intTrapH - Entry point for the Nucleus interrupt handler.
  *
  *   - Initializes module-level globals: interruptTOD, remainingTime, 
  *     and savedExceptState.
  *   - Determines which interrupt line has the highest priority pending 
  *     interrupt.
  *   - Dispatches to the appropriate handler function:
  *       * pltTimerInt() for line 1
  *       * intTimerInt() for line 2
  *       * IOInt() for lines 3-7
  ************************************************************************/
 void intTrapH(){
	 STCK(interruptTOD); /* Store when the Interrupt Handler module is first entered into interruptTOD */
	 remainingTime = getTIMER(); /* Store the remaining time left on the Current Process' quantum */
	 savedExceptState = (state_PTR) BIOSDATAPAGE; /* Initialize to the state stored at the start of the BIOS Data Page */
 
	 /* calling the interrupt handler function based on interrupt type that has the highest priority (i.e., an interrupt occurred on lines 1-2)*/
	 if (((savedExceptState->s_cause) & LINE1INT) != ALLOFF){ 
		 pltTimerInt(); 
	 }
	 if (((savedExceptState->s_cause) & LINE2INT) != ALLOFF){ 
		 intTimerInt(); 
	 }
	 /* otherwise, an I/O interrupt occurred (i.e., an interrupt occurred on lines 3-7) */
	 IOInt(); 
 }
 