/*
 * Probe (Phase 237): scratch registers (d0/d1/a0/a1) after exec I/O calls.
 * They are not preserved by contract, but 1.x programs rely on what
 * Kickstart leaves there (Fred Fish settime: CloseDevice(a1) right after
 * DoIO(a1) without reloading a1).
 */
#include <exec/types.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;
static ULONG out[4];

static void call(LONG lvo, ULONG d0, ULONG d1, APTR a0, APTR a1)
{
    register ULONG r_d0 __asm("d0") = d0;
    register ULONG r_d1 __asm("d1") = d1;
    register APTR r_a0 __asm("a0") = a0;
    register APTR r_a1 __asm("a1") = a1;
    register APTR r_a2 __asm("a2") = (APTR)((UBYTE *)SysBase + lvo);
    register APTR r_a3 __asm("a3") = out;
    __asm__ __volatile__ ("move.l a6,-(sp)\n\tmove.l 4.w,a6\n\tjsr (a2)\n\tmovem.l d0-d1/a0-a1,(a3)\n\tmove.l (sp)+,a6"
                          : "+r" (r_d0), "+r" (r_d1), "+r" (r_a0), "+r" (r_a1), "+r" (r_a2), "+r" (r_a3)
                          : : "cc", "memory");
}

static void show(const char *what, APTR ioreq, APTR name)
{
    static const char *n[4] = { "d0", "d1", "a0", "a1" };
    int i;
    probe_s(what);
    for (i = 1; i < 4; i++) {
        probe_s(" ");
        probe_s(n[i]);
        probe_s("=");
        if (out[i] == (ULONG)ioreq) probe_s("ioreq");
        else if (name && out[i] == (ULONG)name) probe_s("name");
        else if (out[i] == 0x5a5a5a5a) probe_s("kept");
        else if (out[i] == 0) probe_s("0");
        else probe_s("other");
    }
    probe_ch('\n');
}

int main(void)
{
    struct MsgPort *port = CreateMsgPort();
    struct timerequest *tr = (struct timerequest *)CreateIORequest(port, sizeof(struct timerequest));
    static char name[] = "timer.device";
    call(-444, UNIT_VBLANK, 0, name, tr);                          /* OpenDevice */
    show("OpenDevice", tr, name);
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_secs = 0;
    tr->tr_time.tv_micro = 1;
    call(-456, 0x5a5a5a5a, 0x5a5a5a5a, (APTR)0x5a5a5a5a, tr);     /* DoIO */
    show("DoIO", tr, NULL);
    tr->tr_time.tv_micro = 1;
    call(-462, 0x5a5a5a5a, 0x5a5a5a5a, (APTR)0x5a5a5a5a, tr);     /* SendIO */
    show("SendIO", tr, NULL);
    call(-474, 0x5a5a5a5a, 0x5a5a5a5a, (APTR)0x5a5a5a5a, tr);     /* WaitIO */
    show("WaitIO", tr, NULL);
    call(-450, 0x5a5a5a5a, 0x5a5a5a5a, (APTR)0x5a5a5a5a, tr);     /* CloseDevice */
    show("CloseDevice", tr, NULL);
    DeleteIORequest((struct IORequest *)tr);
    DeleteMsgPort(port);
    return 0;
}
