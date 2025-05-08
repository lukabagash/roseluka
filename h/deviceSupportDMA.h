#ifndef _DEVICE_SUPPORT_DMA_H_
#define _DEVICE_SUPPORT_DMA_H_

#include "types.h"

/* SYS14 */
void diskPut(state_PTR savedState, char *virtAddr, int diskNo, int sectNo);
/* SYS15 */
int diskGet(char *virtAddr, int diskNo, int sectNo);
/* SYS16 */
int flashPut(char *virtAddr, int flashNo, int blockNo);
/* SYS17 */
int flashGet(char *virtAddr, int flashNo, int blockNo);

#endif /* _DEVICE_SUPPORT_DMA_H_ */
