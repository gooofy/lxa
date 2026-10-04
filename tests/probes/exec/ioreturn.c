/*
 * Probe (Phase 237): the full 32-bit d0 of OpenDevice(), DoIO(), WaitIO()
 * and CheckIO().  Old programs test d0 as a LONG (Fred Fish AmigaMonitor:
 * OpenDevice() returned garbage in the upper bytes on lxa and the program
 * took its error path).
 */
#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static LONG lvo_a0_d0_a1_d1(LONG lvo, APTR a0, LONG d0, APTR a1, LONG d1)
{
    /* d0 is the unit for OpenDevice(); for DoIO()/WaitIO() it is free and
       filled with garbage so that a BYTE-sized return shows */
    if (lvo != -444)
        d0 = 0x12345600;
    register LONG r_d0 __asm("d0") = d0;
    register LONG r_d1 __asm("d1") = d1;
    register APTR r_a0 __asm("a0") = a0;
    register APTR r_a1 __asm("a1") = a1;
    register APTR r_a2 __asm("a2") = (APTR)((UBYTE *)SysBase + lvo);
    __asm__ __volatile__ ("move.l a6,-(sp)\n\tmove.l 4.w,a6\n\tjsr (a2)\n\tmove.l (sp)+,a6"
                          : "+r" (r_d0), "+r" (r_d1), "+r" (r_a0), "+r" (r_a1), "+r" (r_a2) : : "cc", "memory");
    return r_d0;
}

int main(void)
{
    struct MsgPort *port = CreateMsgPort();
    struct timerequest *tr = (struct timerequest *)CreateIORequest(port, sizeof(struct timerequest));
    LONG rc;

    rc = lvo_a0_d0_a1_d1(-444, (APTR)TIMERNAME, UNIT_VBLANK, tr, 0);
    P_HEX("OpenDevice(timer) d0", rc);
    if (rc == 0) {
        tr->tr_node.io_Command = TR_ADDREQUEST;
        tr->tr_time.tv_secs = 0;
        tr->tr_time.tv_micro = 1;
        P_HEX("DoIO d0", lvo_a0_d0_a1_d1(-456, NULL, 0, tr, 0));
        tr->tr_node.io_Command = 0x7fff;     /* invalid command */
        P_HEX("DoIO(invalid) d0", lvo_a0_d0_a1_d1(-456, NULL, 0, tr, 0));
        P_HEX("WaitIO d0", lvo_a0_d0_a1_d1(-474, NULL, 0, tr, 0));
        CloseDevice((struct IORequest *)tr);
    }
    P_HEX("OpenDevice(timer unit 99) d0", lvo_a0_d0_a1_d1(-444, (APTR)TIMERNAME, 99, tr, 0));
    P_HEX("OpenDevice(no.device) d0", lvo_a0_d0_a1_d1(-444, (APTR)"no.device", 0, tr, 0));
    DeleteIORequest((struct IORequest *)tr);
    DeleteMsgPort(port);
    return 0;
}
