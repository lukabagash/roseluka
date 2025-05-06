#ifndef CONSTS
#define CONSTS

/**************************************************************************** 
 *
 * This header file contains utility constants & macro definitions.
 * 
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE	        4096	    /* page size in bytes */
#define WORDLEN		        4		    /* word size in bytes */
#define MAXPROC             20
#define	MAXINT              214483647	/* 2^31 - 1: maximum value of a signed 32-bit integer */

/* Maximum number of external (sub)devices in UMPS3, plus one additional semaphore to support
the Pseudo-clock */
#define MAXDEVICECNT	    49
#define PANDOS_CAUSEMASK    0x0000007C  /* Extracts the ExcCode bits in the Cause reg */
#define EXCCODESHIFT        2           /* Shift right by 2 to isolate ExcCode */
#define INTEXCPT            0           /* Interrupt exception code */
#define TLBEXCPT            3           /* Upper bound for TLB exceptions */
#define SYSCALLEXCPT        8           /* SYSCALL exception code */
#define TLBMODEXC           1           /* TLB modification exception code */

#define INITPROCCOUNT       0           /* Initial value for processCount */
#define INITSOFTBLKCOUNT    0           /* Initial value for softBlockedCount */
#define DEVSEMINIT          0           /* Initial value for each device semaphore */

#define VPNSHIFT            12          
#define ASIDSHIFT           6
#define PFNSHIFT            12
#define DIRTYON             0x00000400  /* Dirty set to 1 */ 
#define VALIDOFF            0x0         /* Valid */
#define VALIDON             0x00000200
#define GLOBALOFF           0x0         /* Global */

#define UPROCMAX            1           /* User process count */          
#define PGTBLSIZE           32          /* Array size of page table */
#define SUPSTCKTOP          499         /* Top of the stack area for the process' exception handlers */
 
/* Stack pointer for Nucleus (used in Pass Up Vector) */
#define NUCLEUSSTACK        0x20001000

/* Interval timer set to 100 ms (100,000 microseconds) */
#define PANDOS_CLOCKINTERVAL    100000UL
/* Processor State--Status register constants */
#define ALLOFF			    0x0     	/* every bit in the Status register is set to 0; for bitwise-OR operations */
#define PANDOS_IEPBITON     0x00000004  /* Enable interrupts after LDST (IEp) */
#define TEBITON             0x08000000  /* Enable processor Local Timer (PLT) */
#define PANDOS_CAUSEINTMASK 0x0000FF00  /* Full interrupt mask bits on */
#define IECON			    0x00000001	/* Enable the global interrupt bit (i.e., IEc (bit 0) = 1) */
#define IECOFF              0xFFFFFFFE  /* Enable the global interrupt bit */
/* Constant to help determine the index in deviceSemaphores/devSemaphores and in the Interrupt Devices Bitmap that a particular device is located at. 
This constant is subtracted from the line number (or 4, in the case of backing store management), since interrupt lines 3-7 are used for peripheral devices  */
#define	OFFSET			    3
/* Constants to help determine which device the highest-priority interrupt occurred on */
#define	DEV0INT			    0x00000001	/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 0, which is tied to device 0 interrupts */
/* Constants for the different device numbers an interrupt may occur on */
#define	DEV0			    0			
#define	DEV1			    1			
#define	DEV2			    2			
#define	DEV3			    3			
#define	DEV4			    4			
#define	DEV5			    5			
#define	DEV6			    6			
#define	DEV7			    7			
/* Constant defining a large value to load the PLT with in switchProcess() when the Process Count and Soft Block Count are both greater than zero */
#define NEVER			    0xFFFFFFFF
/* Value to */
#define PCLOCKIDX           (MAXDEVICECNT - 1)
/* Constant representing the initial value of the Pseudo-clock semaphore */
#define	INITIALPCSEM	    0
/* Cause register constants to help determine which line the highest-priority interrupt is located at */
#define	LINE1INT		    0x00000200	/* setting 1 at bit 9 for line 1 interrupts */
#define	LINE2INT		    0x00000400	/* setting 1 at bit 10 for line 2 interrupts */
#define	LINE3INT		    0x00000800	/* setting 1 at bit 11 for line 3 interrupts */
#define	LINE4INT		    0x00001000	/* setting 1 at bit 12 for line 4 interrupts */
#define	LINE5INT		    0x00002000	/* setting 1 at bit 13 for line 5 interrupts */
#define	LINE6INT		    0x00004000	/* setting 1 at bit 14 for line 6 interrupts */
#define	LINE7INT		    0x00008000	/* setting 1 at bit 15 for line 7 interrupts */

/* Constants for the different line numbers an interrupt may occur on */
#define	LINE1			    1			/* Processor Local Timer */
#define	LINE2			    2			/* Interval Timer (Bus) */
#define	LINE3			    3			/* Disk devices */
#define	LINE4			    4			/* Flash Devices */
#define	LINE5			    5			/* Network (Ethernet) Devices */
#define	LINE6			    6			/* Printer devices */
#define	LINE7			    7			/* Terminal devices */

/* Constant that represents when the first four bits in a terminal device's device register's status field are turned on */
#define	STATUSON		    0x0F
/* Constants to help determine which device the highest-priority interrupt occurred on */
#define	DEV0INT			    0x00000001	/* setting 1 at bit 0 for device 0 interrupts */
#define	DEV1INT			    0x00000002	/* setting 1 at bit 1 for device 1 interrupts */
#define	DEV2INT			    0x00000004	/* setting 1 at bit 2 for device 2 interrupts */
#define	DEV3INT			    0x00000008	/* setting 1 at bit 3 for device 3 interrupts */
#define	DEV4INT			    0x00000010	/* setting 1 at bit 4 for device 4 interrupts */
#define	DEV5INT			    0x00000020	/* setting 1 at bit 5 for device 5 interrupts */
#define	DEV6INT			    0x00000040	/* setting 1 at bit 6 for device 6 interrupts */
#define	DEV7INT			    0x00000080	/* setting 1 at bit 7 for device 7 interrupts */

/* Value that the processor's Local Timer (PLT) is intialized to 5 milliseconds (5,000 microseconds) */
#define INITIALPLT		    5000

#define INITIALACCTIME	    0           /* initial value for the accumulated time field for a process that is instantiated */
/* Constants for returning values in v0 to the caller */
#define FAIL                -1			/* an error occurred in the caller's request */
#define OK                  0			/* the caller's request completed successfully */

/* Constants signifying the first index of the deviceSemaphores array (We can get the last index of the array by saying MAXDEVICECNT - 1.) */
#define	FIRSTDEVINDEX	    0
/* Constant representing the lower bound on which we unblock semaphores and remove them from the ASL */
#define	SEMA4THRESH		    0
#define USERPON			    0x00000008  /* setting the user-mode on after LDST (i.e., KUp (bit 3) = 1) */
/* Cause register constant for setting all bits to 1 in the Cause register except for the ExcCode field, which is set to 10 for the RI code */
#define	RESINSTRCODE	    0xFFFFFF28
/* Constants representing the Syscall Numbers in Pandos */
#define	CREATEPROCESS       1		
#define	TERMINATEPROCESS    2
#define	PASSEREN            3
#define	VERHOGEN            4
#define	WAITIO			    5
#define	GETCPUTIME          6
#define	WAITCLOCK           7
#define	GETSUPPORTPTR       8
#define TERMINATE           9           
#define GETTOD              10          
#define WRITEPRINTER        11          
#define WRITETERMINAL       12          
#define READTERMINAL        13  
#define DISKPUT             14
#define DISKGET             15
#define FLASHPUT            16
#define FLASHGET            17  
#define DELAY               18      

#define PRINTERROR          4           /* Printer Device Status Code: Error during character transmission */
#define PRINTCHR            2           /* Printer Device Command Code: Transmit the character in DATA0 over the line */
#define RECEIVEERROR        4           /* Terminal Device Status Code: Receive Error */
#define TRANSMISERROR       4           /* Terminal Device Status Code: Transmission Error */
#define RECEIVECHAR         2           /* Terminal Device Command Code: Receive the character over the line */
#define TRANSMITCHAR        2           /* Terminal Device Command Code: Transmit the character over the line*/

/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		    0x10000000
#define RAMBASESIZE		    0x10000004
#define TODLOADDR		    0x1000001C
#define INTERVALTMR		    0x10000020	
#define TIMESCALEADDR	    0x10000024


/* utility constants */
#define	TRUE			    1
#define	FALSE			    0
#define HIDDEN			    static
#define EOS				    '\0'

#define NULL 			    ((void *)0xFFFFFFFF)

/* device interrupts */
#define DISKINT			    3
#define FLASHINT 		    4
#define NETWINT 		    5
#define PRNTINT 		    6
#define TERMINT			    7

#define DEVINTNUM		    5		    /* interrupt lines used by devices */
#define DEVPERINT		    8		    /* devices per interrupt line */
#define DEVREGLEN		    4		    /* device register field length in bytes, and regs per dev */	
#define DEVREGSIZE	        16 		    /* device register size in bytes */

/* device register field number for non-terminal devices */
#define STATUS			    0
#define COMMAND			    1
#define DATA0			    2
#define DATA1			    3

/* device register field number for terminal devices */
#define RECVSTATUS  	    0
#define RECVCOMMAND 	    1
#define TRANSTATUS  	    2
#define TRANCOMMAND 	    3

/* device common STATUS codes */
#define UNINSTALLED		    0
#define READY			    1
#define BUSY			    3

/* device common COMMAND codes */
#define RESET			    0
#define ACK				    1

/* Memory related constants */
#define KSEG0               0x00000000
#define KSEG1               0x20000000
#define KSEG2               0x40000000
#define KUSEG               0x80000000
#define RAMSTART            0x20000000
#define BIOSDATAPAGE        0x0FFFF000
#define	PASSUPVECTOR	    0x0FFFF900

#define TEXTAREASTART       0x800000B0
#define STCKPGVPN           0xBFFFF000
#define STCKTOPEND          0xC0000000

#define FRAMEPOOLSTART      0x20001040  /* In RAM, the top of kernel stack (0x2000.1000) + OS area (64) */

/* Exceptions related constants */
#define	PGFAULTEXCEPT	    0
#define GENERALEXCEPT	    1

/* Constants for the TLB exception handling */
#define VALIDOFFTLB         0xFFFFFDFF /* Mask to clear the VALID bit in the EntryLO field of a TLB entry */
#define WRITEBLK            3          /* Copy the 4 KB of RAM starting at the address in DATA0 into the block located at (BLOCKNUMBER)*/
#define FLASCOMHSHIFT       8          /* Shift to get the block number from the command sent to the flash device */
#define WRITEERR            5          /* Flash device status code for write error */
#define READBLK             2          /* Copy the block located at (BLOCKNUMBER) into the RAM starting at the address in DATA0 */
#define READERR             4          /* Flash device status code for read error */
#define VPNMASK             0xFFFFF000 /* Mask to extract the VPN from the EntryHI field of a TLB entry */
#define ENDOFLINE           0x0000000A /* End of line character for terminal devices (newline) */
#define TERMSTATUSMASK      0x0000000FF /* Mask to extract the status bits from the device register's status field for terminal devices */
#define CHARRECIVED         5          /* Terminal device status code for character received successfully */
#define CHARTRANSMITTED     5          /* Terminal device status code for character transmitted successfully */
#define MAXSTRINGLEN        128        /* Maximum length of a string that can be passed to devices*/
#define DEVREDY             1          /* Device is ready for I/O operations */
#define PERIPHDEVCNT        48         /* Total number of peripheral devices (Disk, Flash, Network, Printer): 4 classes × 8 devices = 32 semaphores and (Terminal devices): 8 terminals × 2 semaphores = 16 semaphores */
#define	SWAPPOOLADDR	    0x20020000
#define INDEXPMASK          0x80000000 /* Index p for tlb */
#define RECCHARSTATSHIFT    8
#define RECCHARSTATMASK     0xFF /* Mask to extract the received character from the terminal device's status field */
#define TRANSCHARSTATSHIFT  8 /* Shift to get the transmitted character from the terminal device's status field */
#define KVSBEGIN            0x80000 

/* operations */
#define	MIN(A,B)		    ((A) < (B) ? A : B)
#define MAX(A,B)		    ((A) < (B) ? B : A)
#define	ALIGNED(A)		    (((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T)	            ((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 

/* Macro to read the TOD clock */
#define STCK(T)             ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))

#endif
