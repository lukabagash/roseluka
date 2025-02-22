#ifndef CONSTS
#define CONSTS

/**************************************************************************** 
 *
 * This header file contains utility constants & macro definitions.
 * 
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE		  4096			/* page size in bytes	*/
#define WORDLEN			  4				  /* word size in bytes	*/
#define MAXPROC           20
#define	MAXINT            214483647	/* 2^31-1: maximum value of a signed 32-bit integer */

/* Maximum number of external (sub)devices in UMPS3, plus one additional semaphore to support
the Pseudo-clock */
#define MAXDEVICECNT	49
#define GETEXCEPCODE	0x0000007C	/* constant for setting all bits to 0 in the Cause register except for the ExcCode field */
#define CAUSESHIFT		2			/* number of bits needed to shift the ExcCode field over to the right so that we can read the ExcCode directly */
#define INTCONST		0			/* exception code signaling an interrupt occurred */
#define TLBCONST		3			/* upper bound on the exception codes that signal a TLB exception occurred */
#define SYSCONST		8			/* exception code signaling a SYSCALL occurred */
#define	INITIALPROCCNT		0			/* the initial value of procCnt */
#define	INITIALSFTBLKCNT	0			/* the initial value of softBlockCnt */
#define	INITIALDEVSEMA4		0			/* the initial value of the device semaphores */
/* Address for initializing Process 0's Pass Up Vector's fields for the address of handling general exceptions and TLB-Refill events */
#define PROC0STACKPTR	0x20001000
/* Value that the system-wide Interval Timer is initialized to 100ms (100,000 microseconds) */
#define INITIALINTTIMER	100000
/* Processor State--Status register constants */
#define ALLOFF			0x0     	/* every bit in the Status register is set to 0; this will prove helpful for bitwise-OR operations */
#define IECON			0x00000001	/* constant for enabling the global interrupt bit (i.e., IEc (bit 0) = 1) */
#define IEPON			0x00000004	/* constant for enabling interrupts after LDST (i.e., IEp (bit 2) = 1) */
#define PLTON			0x08000000	/* constant for enabling PLT (i.e., TE (bit 27) = 1) */
#define IMON			0x0000FF00	/* constant for setting the Interrupt Mask bits to on so interrupts are fully enabled */
#define IECON			0x00000001	/* constant for enabling the global interrupt bit (i.e., IEc (bit 0) = 1) */
/* Constant to help determine the index in deviceSemaphores/devSemaphores and in the Interrupt Devices Bitmap that a particular device is located at. 
This constant is subtracted from the line number (or 4, in the case of backing store management), since interrupt lines 3-7 are used for peripheral devices  */
#define	OFFSET			3
/* Constants to help determine which device the highest-priority interrupt occurred on */
#define	DEV0INT			0x00000001		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 0, which is tied to device 0 interrupts */
/* Constants for the different device numbers an interrupt may occur on */
#define	DEV0			0				/* constant representing device 0 */
#define	DEV1			1				/* constant representing device 1 */
#define	DEV2			2				/* constant representing device 2 */
#define	DEV3			3				/* constant representing device 3 */
#define	DEV4			4				/* constant representing device 4 */
#define	DEV5			5				/* constant representing device 5 */
#define	DEV6			6				/* constant representing device 6 */
#define	DEV7			7				/* constant representing device 7 */
/* Constant defining a large value to load the PLT with in switchProcess() when the Process Count and Soft Block Count are both greater than zero */
#define NEVER			0xFFFFFFFF
/* Value to */
#define PCLOCKIDX       (MAXDEVICECNT - 1)
/* Constant representing the initial value of the Pseudo-clock semaphore */
#define	INITIALPCSEM	0
/* Cause register constants to help determine which line the highest-priority interrupt is located at */
#define	LINE1INT		0x00000200		/* constant for setting all bits to 0 in the Cause register except for bit 9, which is tied to line 1 interrupts */
#define	LINE2INT		0x00000400		/* constant for setting all bits to 0 in the Cause register except for bit 10, which is tied to line 2 interrupts */
#define	LINE3INT		0x00000800		/* constant for setting all bits to 0 in the Cause register except for bit 11, which is tied to line 3 interrupts */
#define	LINE4INT		0x00001000		/* constant for setting all bits to 0 in the Cause register except for bit 12, which is tied to line 4 interrupts */
#define	LINE5INT		0x00002000		/* constant for setting all bits to 0 in the Cause register except for bit 13, which is tied to line 5 interrupts */
#define	LINE6INT		0x00004000		/* constant for setting all bits to 0 in the Cause register except for bit 14, which is tied to line 6 interrupts */
#define	LINE7INT		0x00008000		/* constant for setting all bits to 0 in the Cause register except for bit 15, which is tied to line 7 interrupts */

/* Constants for the different line numbers an interrupt may occur on */
#define	LINE1			1				/* constant representing line 1 */
#define	LINE2			2				/* constant representing line 2 */
#define	LINE3			3				/* constant representing line 3 */
#define	LINE4			4				/* constant representing line 4 */
#define	LINE5			5				/* constant representing line 5 */
#define	LINE6			6				/* constant representing line 6 */
#define	LINE7			7				/* constant representing line 7 */
/* Constant to help determine the index in deviceSemaphores/devSemaphores and in the Interrupt Devices Bitmap that a particular device is located at. 
This constant is subtracted from the line number (or 4, in the case of backing store management), since interrupt lines 3-7 are used for peripheral devices  */
#define	OFFSET			3
/* Constant that represents when the first four bits in a terminal device's device register's status field are turned on */
#define	STATUSON		0x0F
/* Constants to help determine which device the highest-priority interrupt occurred on */
#define	DEV0INT			0x00000001		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 0, which is tied to device 0 interrupts */
#define	DEV1INT			0x00000002		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 1, which is tied to device 1 interrupts */
#define	DEV2INT			0x00000004		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 2, which is tied to device 2 interrupts */
#define	DEV3INT			0x00000008		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 3, which is tied to device 3 interrupts */
#define	DEV4INT			0x00000010		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 4, which is tied to device 4 interrupts */
#define	DEV5INT			0x00000020		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 5, which is tied to device 5 interrupts */
#define	DEV6INT			0x00000040		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 6, which is tied to device 6 interrupts */
#define	DEV7INT			0x00000080		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 7, which is tied to device 7 interrupts */

/* Value that the processor's Local Timer (PLT) is intialized to 5 milliseconds (5,000 microseconds) */
#define INITIALPLT		5000

#define INITIALACCTIME	  0     /* Value to set the accumulated time field for a process that is instantiated */
/* Constants for returning values in v0 to the caller */
#define ERRORCONST		-1			/* constant denoting an error occurred in the caller's request */
#define SUCCESSCONST	0			/* constant denoting that the caller's request completed successfully */

/* Constants signifying the first index of the deviceSemaphores array (We can get the last index of the array by saying MAXDEVICECNT - 1.) */
#define	FIRSTDEVINDEX	0
/* Constant representing the lower bound on which we unblock semaphores and remove them from the ASL */
#define	SEMA4THRESH		0
#define USERPON			0x00000008	/* constant for setting the user-mode on after LDST (i.e., KUp (bit 3) = 1) */
/* Cause register constant for setting all bits to 1 in the Cause register except for the ExcCode field, which is set to 10 for the RI code */
#define	RESINSTRCODE	0xFFFFFF28
/* Constants representing the Syscall Numbers in Pandos */
#define	SYS1NUM			1		
#define	SYS2NUM			2
#define	SYS3NUM			3
#define	SYS4NUM			4
#define	SYS5NUM			5
#define	SYS6NUM			6
#define	SYS7NUM			7
#define	SYS8NUM			8



/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		0x10000000
#define RAMBASESIZE		0x10000004
#define TODLOADDR		  0x1000001C
#define INTERVALTMR		0x10000020	
#define TIMESCALEADDR	0x10000024


/* utility constants */
#define	TRUE			    1
#define	FALSE			    0
#define HIDDEN			  static
#define EOS				    '\0'

#define NULL 			    ((void *)0xFFFFFFFF)

/* device interrupts */
#define DISKINT			  3
#define FLASHINT 		  4
#define NETWINT 		  5
#define PRNTINT 		  6
#define TERMINT			  7

#define DEVINTNUM		  5		  /* interrupt lines used by devices */
#define DEVPERINT		  8		  /* devices per interrupt line */
#define DEVREGLEN		  4		  /* device register field length in bytes, and regs per dev */	
#define DEVREGSIZE	  16 		/* device register size in bytes */

/* device register field number for non-terminal devices */
#define STATUS			  0
#define COMMAND			  1
#define DATA0			    2
#define DATA1			    3

/* device register field number for terminal devices */
#define RECVSTATUS  	0
#define RECVCOMMAND 	1
#define TRANSTATUS  	2
#define TRANCOMMAND 	3

/* device common STATUS codes */
#define UNINSTALLED		0
#define READY			    1
#define BUSY			    3

/* device common COMMAND codes */
#define RESET			    0
#define ACK				    1

/* Memory related constants */
#define KSEG0           0x00000000
#define KSEG1           0x20000000
#define KSEG2           0x40000000
#define KUSEG           0x80000000
#define RAMSTART        0x20000000
#define BIOSDATAPAGE    0x0FFFF000
#define	PASSUPVECTOR	  0x0FFFF900

/* Exceptions related constants */
#define	PGFAULTEXCEPT	  0
#define GENERALEXCEPT	  1


/* operations */
#define	MIN(A,B)		((A) < (B) ? A : B)
#define MAX(A,B)		((A) < (B) ? B : A)
#define	ALIGNED(A)		(((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 

/* Macro to read the TOD clock */
#define STCK(T) ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))

#endif
