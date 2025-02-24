#ifndef EXCEPTIONS
#define EXCEPTIONS

/**************************************************************************** 
 *
 * The externals declaration file for the TLB-Refill exception handler module
 * 
 * Written by: Luka Bagashvili, Rosalie Lee
 * 
 ****************************************************************************/

#include "../h/types.h"

extern void sysTrapH ();
extern void tlbTrapH ();
extern void pgmTrapH ();
extern void updateCurrPcb();
extern void uTLB_RefillHandler();

#endif
