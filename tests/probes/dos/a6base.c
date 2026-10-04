/*
 * Probe (Phase 237): dos.library functions called with a6 = SysBase
 * instead of DOSBase.  1.x programs (Fred Fish LaceTogl, FME, ...) call
 * dos through another register (jsr -48(a4)) and leave a6 = SysBase.
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

/* call DOSBase-LVO with d1..d3 and a6 = SysBase */
static LONG call3(LONG lvo, LONG d1, LONG d2, LONG d3)
{
    register LONG r_d0 __asm("d0");
    register LONG r_d1 __asm("d1") = d1;
    register LONG r_d2 __asm("d2") = d2;
    register LONG r_d3 __asm("d3") = d3;
    register APTR r_a0 __asm("a0") = (APTR)((UBYTE *)DOSBase + lvo);
    __asm__ __volatile__ (
        "move.l a6,-(sp)\n\t"
        "move.l 4.w,a6\n\t"
        "jsr (a0)\n\t"
        "move.l (sp)+,a6"
        : "=r" (r_d0), "+r" (r_d1), "+r" (r_d2), "+r" (r_d3), "+r" (r_a0)
        :
        : "a1", "cc", "memory");
    return r_d0;
}

int main(void)
{
    static char msg[] = "written with a6 = SysBase\n";
    static char buf[16];
    LONG out, fh, lock, n;

    probe_flush();
    out = call3(-60, 0, 0, 0);                      /* Output() */
    P_BOOL("Output() = Output()", out == (LONG)Output());
    n = call3(-48, out, (LONG)msg, sizeof(msg) - 1); /* Write() */
    P_LONG("Write", n);
    fh = call3(-30, (LONG)"T:probe-a6base", MODE_NEWFILE, 0);  /* Open() */
    P_NULL("Open", (APTR)fh);
    if (fh) {
        P_LONG("Write file", call3(-48, fh, (LONG)"abc", 3));
        P_LONG("Seek", call3(-66, fh, 0, OFFSET_BEGINNING));
        P_LONG("Read", call3(-42, fh, (LONG)buf, 3));
        P_LONG("Close", call3(-36, fh, 0, 0));
    }
    lock = call3(-84, (LONG)"T:probe-a6base", SHARED_LOCK, 0);  /* Lock() */
    P_NULL("Lock", (APTR)lock);
    if (lock)
        call3(-90, lock, 0, 0);                     /* UnLock() */
    P_LONG("DeleteFile", call3(-72, (LONG)"T:probe-a6base", 0, 0));
    call3(-462, 205, 0, 0);                         /* SetIoErr() */
    P_LONG("IoErr", call3(-132, 0, 0, 0));
    return 0;
}
