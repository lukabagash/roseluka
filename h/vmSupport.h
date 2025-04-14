#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "types.h"

/* Initialize swap pool structures and semaphore */
extern void initSwapStructs();

/* Acquire or release mutex on a semaphore */
extern void mutex(int *sem, int operation); /* operation TRUE (P) or FALSE (V) */

/* TLB exception handler (pager) */
extern void supLvlTlbExceptionHandler(void);

/* TLB refill handler */
extern void uTLB_RefillHandler(void);

extern void ph3programTrapHandler(void);

/* Disable interrupts */
extern void disableInterrupts(void);

/* Enable interrupts */
extern void enableInterrupts(void);

#endif /* VMSUPPORT_H */
