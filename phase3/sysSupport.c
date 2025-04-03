/*This module implements the Support Level’s:
• general exception handler. [Section 4.6]
• SYSCALL exception handler. [Section 4.7]
• Program Trap exception handler. [Section 4.8]*/

#include "../h/sysSupport.h"
#include "../h/initial.h"
#include "../h/types.h"
#include "../h/exceptions.h"
#include "/usr/include/umps3/umps/libumps.h"

HIDDEN void schizoUserProcTerminate() {
    SYSCALL (2, 0, 0, 0)
}

HIDDEN void getTOD() {
    currentProcess->p_s.s_v0 = startTOD;
}

HIDDEN void writetoprinter(char *virtAddr, int len) {
    int charNum = 0;
    devregarea_t reg = RAMBASEADDR;
    device_t printerdev = reg->devreg[PRNTINT];
       
    for (int i = 0; i < len; i++) {
        // Write printer device's DATA0 field with printer device address (i.e., address of printer device)
        printerdev.data0 = virtAddr[i];
        printerdev.d_command = 2; /* PRINTCHR command code */
        
        /* do we issue sys5? to suspend u_proc */

        /* if not successfully written PRINTERRRR status code */
        if (printerdev.d_status = 4) {
            charNum = -printerdev.d_status;
            break;
        }
        charNum++;
    } 
    //Resume back to user mode with v0 updated accordingly:
    currentProcess->p_s.s_v0 = charNum;
}