#ifndef EXCEPTIONS
#define EXCEPTIONS

/**************************************************************************** 
 *
 * Declaration header file for Nucleus exception handling.
 *
 * Written by: Luka Bagashvili, Rosalie Lee
 *
 ****************************************************************************/
#include "../h/types.h"

/*
 * Functions used by other modules (initial.c, interrupts.c, etc.).
 * Make sure their signatures match the references in those .c files.
 */

/* Update the Current Process’s PCB with the CPU state from the BIOS Data Page */
extern void updateCurrPcb(void);

/* Entry points for handling exceptions: */
extern void sysTrapH(void);  /* SYSCALL exceptions */
extern void tlbTrapH(void);  /* TLB exceptions */
extern void pgmTrapH(void);  /* Program Trap exceptions */

/* Optional TLB-refill handler placeholder (if still used in initial.c) */
extern void uTLB_RefillHandler(void);

#endif
