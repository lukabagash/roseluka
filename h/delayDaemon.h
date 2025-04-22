#ifndef DELAYDAEMON_H
#define DELAYDAEMON_H

#include "types.h"
#include "const.h"

/* Called once by the Instantiator (test()) to set up the ADL and launch the daemon */
void initADL(void);

/* Implements the support‐level handler for SYS18 */
void delaySyscall(state_t *savedState);

/* The Daemon process itself (infinite loop) */
void delayDaemon(void);

#endif /* DELAYDAEMON_H */
