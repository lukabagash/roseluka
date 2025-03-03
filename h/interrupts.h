#ifndef INTERRUPTS
#define INTERRUPTS

/**************************************************************************** 
 *
 * Declaration header for the Nucleus interrupt handler module.
 *
 * Written by: Luka Bagashvili, Rosalie Lee
 *
 ****************************************************************************/
#include "../h/types.h"

/*
 * The only externally visible function from interrupts.c is intTrapH(),
 * which is the entry point for handling all device/timer interrupts.
 */
extern void intTrapH(void);

#endif
