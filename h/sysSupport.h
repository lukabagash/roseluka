#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H

#include "types.h"

/* Terminate the user process, releasing mutex if needed */
extern void schizoUserProcTerminate(int *address);

/* Get the current Time of Day */
void getTOD(void);

/* Write a string to the printer device */
void writePrinter(char *virtAddr, int len);

/* Write a string to the terminal device */
void writeTerminal(char *virtAddr, int len);

/* Read a string from the terminal device */
void readTerminal(char *virtAddr);

/* Support-level general exception handler */
extern void supLvlGenExceptionHandler(void);

/* Check for illegal syscall string length */
void illegalCheck(int len);

#endif /* SYSSUPPORT_H */
