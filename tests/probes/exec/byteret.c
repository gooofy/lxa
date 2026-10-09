/*
 * Probe (Phase 238): the full 32-bit D0 of exec functions declared to
 * return BYTE - OpenDevice, DoIO, WaitIO, AllocSignal, SetTaskPri.
 * Programs compiled by other compilers test the whole register
 * (garbagecollector.library of AmigaOberon does `tst.l d0` after
 * OpenDevice()), so the upper bytes are part of the ABI in practice.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/execbase.h>
#include <devices/timer.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static ULONG raw_opendevice(CONST_STRPTR name, ULONG unit, struct IORequest *io, ULONG flags)
{
    register ULONG d0 __asm("d0") = unit;
    register ULONG d1 __asm("d1") = flags;
    register CONST_STRPTR a0 __asm("a0") = name;
    register struct IORequest *a1 __asm("a1") = io;
    register struct ExecBase *a6 __asm("a6") = SysBase;
    __asm volatile ("jsr -444(a6)" : "+r"(d0), "+r"(d1), "+r"(a0), "+r"(a1) : "r"(a6) : "cc", "memory");
    return d0;
}

static ULONG raw_a1(LONG lvo, APTR arg)
{
    register ULONG d0 __asm("d0") = 0x12345678;
    register ULONG d1 __asm("d1") = 0x12345678;
    register APTR a0 __asm("a0") = 0;
    register APTR a1 __asm("a1") = arg;
    register struct ExecBase *a6 __asm("a6") = SysBase;
    if (lvo == -456)
        __asm volatile ("jsr -456(a6)" : "+r"(d0), "+r"(d1), "+r"(a0), "+r"(a1) : "r"(a6) : "cc", "memory");
    else
        __asm volatile ("jsr -474(a6)" : "+r"(d0), "+r"(d1), "+r"(a0), "+r"(a1) : "r"(a6) : "cc", "memory");
    return d0;
}

static ULONG raw_allocsignal(LONG n)
{
    register ULONG d0 __asm("d0") = n;
    register ULONG d1 __asm("d1") = 0x12345678;
    register APTR a0 __asm("a0") = 0;
    register APTR a1 __asm("a1") = 0;
    register struct ExecBase *a6 __asm("a6") = SysBase;
    __asm volatile ("jsr -330(a6)" : "+r"(d0), "+r"(d1), "+r"(a0), "+r"(a1) : "r"(a6) : "cc", "memory");
    return d0;
}

static ULONG raw_settaskpri(struct Task *t, LONG pri)
{
    register ULONG d0 __asm("d0") = pri;
    register ULONG d1 __asm("d1") = 0x12345678;
    register APTR a0 __asm("a0") = 0;
    register APTR a1 __asm("a1") = t;
    register struct ExecBase *a6 __asm("a6") = SysBase;
    __asm volatile ("jsr -300(a6)" : "+r"(d0), "+r"(d1), "+r"(a0), "+r"(a1) : "r"(a6) : "cc", "memory");
    return d0;
}

int main(void)
{
    struct MsgPort *port = CreateMsgPort();
    struct timerequest *tr;
    struct Task *me = FindTask(NULL);
    ULONG r;
    int i, n;
    BYTE sigs[32];

    if (!port)
        return 20;
    tr = (struct timerequest *)CreateIORequest(port, sizeof(struct timerequest));
    if (!tr)
        return 20;

    P_SECTION("OpenDevice");
    r = raw_opendevice((CONST_STRPTR)"timer.device", UNIT_VBLANK, (struct IORequest *)tr, 0);
    P_HEX("timer.device UNIT_VBLANK", r);
    P_LONG("io_Error", tr->tr_node.io_Error);

    P_SECTION("DoIO / WaitIO");
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_secs = 0;
    tr->tr_time.tv_micro = 20000;
    r = raw_a1(-456, tr);
    P_HEX("DoIO TR_ADDREQUEST", r);
    tr->tr_node.io_Command = 0x7f;      /* unknown command -> IOERR_NOCMD */
    r = raw_a1(-456, tr);
    P_HEX("DoIO unknown command", r);
    P_LONG("io_Error", tr->tr_node.io_Error);
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_secs = 0;
    tr->tr_time.tv_micro = 20000;
    SendIO((struct IORequest *)tr);
    r = raw_a1(-474, tr);
    P_HEX("WaitIO TR_ADDREQUEST", r);
    tr->tr_node.io_Command = 0x7f;
    SendIO((struct IORequest *)tr);
    r = raw_a1(-474, tr);
    P_HEX("WaitIO unknown command", r);
    CloseDevice((struct IORequest *)tr);

    r = raw_opendevice((CONST_STRPTR)"timer.device", 77, (struct IORequest *)tr, 0);
    P_HEX("timer.device unit 77", r);
    P_LONG("io_Error", tr->tr_node.io_Error);
    r = raw_opendevice((CONST_STRPTR)"nonexistent.device", 0, (struct IORequest *)tr, 0);
    P_HEX("nonexistent.device", r);
    P_LONG("io_Error", tr->tr_node.io_Error);

    P_SECTION("AllocSignal");
    r = raw_allocsignal(-1);
    P_LONG("AllocSignal(-1) in 16..31", (LONG)r >= 16 && (LONG)r <= 31);
    P_HEX("AllocSignal(-1) upper 24 bits", r & 0xffffff00UL);
    FreeSignal((LONG)(BYTE)r);
    n = 0;
    for (i = 0; i < 32; i++) {
        r = raw_allocsignal(-1);
        if ((LONG)r == -1 || (BYTE)r == -1)
            break;
        sigs[n++] = (BYTE)r;
    }
    P_HEX("AllocSignal(-1) when none free", r);
    r = raw_allocsignal(sigs[0]);
    P_HEX("AllocSignal(taken)", r);
    while (n)
        FreeSignal(sigs[--n]);

    P_SECTION("SetTaskPri");
    {
        LONG oldpri = (BYTE)raw_settaskpri(me, 0);   /* the launcher's priority */
        r = raw_settaskpri(me, -5);
        P_HEX("SetTaskPri(me, -5) (old pri 0)", r);
        r = raw_settaskpri(me, 0);
        P_HEX("SetTaskPri(me, 0) (old pri -5)", r);
        SetTaskPri(me, oldpri);
    }

    DeleteIORequest((struct IORequest *)tr);
    DeleteMsgPort(port);
    return 0;
}
