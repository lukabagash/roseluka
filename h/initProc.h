#ifndef INITPROC_H
#define INITPROC_H

#include "types.h"

/* External semaphore array for devices */
extern int p3devSemaphore[MAXDEVICECNT - 1];

/* External master semaphore for synchronization */
extern int masterSemaphore;

/* Test function to initialize the system and spawn user processes */
void test(void);

#endif /* INITPROC_H */
