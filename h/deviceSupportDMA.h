// h/deviceSupportDMA.h
#ifndef _DEVICE_SUPPORT_DMA_H_
#define _DEVICE_SUPPORT_DMA_H_

/* SYS14 */
int diskPut(char *virtAddr, int diskNo, int sectNo);
/* SYS15 */
int diskGet(char *virtAddr, int diskNo, int sectNo);
/* SYS16 */
int flashPut(char *virtAddr, int flashNo, int blockNo);
/* SYS17 */
int flashGet(char *virtAddr, int flashNo, int blockNo);

#endif /* _DEVICE_SUPPORT_DMA_H_ */
