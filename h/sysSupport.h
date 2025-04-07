#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H

#include "types.h"

/* Terminate the user process, releasing mutex if needed */
extern void schizoUserProcTerminate(int *address);

/* Support-level general exception handler */
extern void supLvlGenExceptionHandler(void);

#endif /* SYSSUPPORT_H */
