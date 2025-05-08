#ifndef _DEVICE_SUPPORT_DMA_H_
#define _DEVICE_SUPPORT_DMA_H_

#include "types.h"

/* SYS14 */
void diskPut(state_PTR savedState, char *virtAddr, int diskNo, int sectNo);
/* SYS15 */
void diskGet(state_PTR savedState, char *virtAddr, int diskNo, int sectNo);
/* SYS16 */
void flashPut(state_PTR savedState, char *virtAddr, int flashNo, int blockNo);
/* SYS17 */
void flashGet(state_PTR savedState, char *virtAddr, int flashNo, int blockNo);

extern int flashOperation(int operation, int flashNo, int blockNo, char *buffer);

#endif /* _DEVICE_SUPPORT_DMA_H_ */
