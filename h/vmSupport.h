#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "types.h"

/* Initialize swap pool structures and semaphore */
void initSwapStructs(void);

/* Acquire or release mutex on a semaphore */
void mutex(int *sem, int operation); /* operation TRUE (P) or FALSE (V) */

/* TLB exception handler (pager) */
void supLvlTlbExceptionHandler(void);

/* TLB refill handler */
void uTLB_RefillHandler(void);

void programTrapHandler(void);

#endif /* VMSUPPORT_H */
