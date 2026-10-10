/*
 * Probe (Phase 237c): dos.library returns its result in D1 as well as in
 * D0 - for which functions.  1.x code tests D1 after the call: Fred
 * Fish Bbm's Manx overlay loader does `jsr Seek; tst.l d1; bmi retry` and
 * `jsr LoadSeg; tst.l d1; beq retry`.  Each function is called with D1
 * preset to a pattern the result cannot have; prints whether D1 equals D0
 * afterwards.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

/* call(lvo, d1, d2, d3) -> d0, *d1out */
ULONG dcall(LONG lvo, ULONG d1, ULONG d2, ULONG d3, ULONG *d1out);
__asm__(
    "    .text\n"
    "    .globl _dcall\n"
    "_dcall:\n"
    "    movem.l d2-d7/a2-a6,-(sp)\n"
    "    move.l _DOSBase,a6\n"
    "    move.l a6,a2\n"
    "    add.l 48(sp),a2\n"
    "    move.l 52(sp),d1\n"
    "    move.l 56(sp),d2\n"
    "    move.l 60(sp),d3\n"
    "    jsr (a2)\n"
    "    move.l 64(sp),a0\n"
    "    move.l d1,(a0)\n"
    "    movem.l (sp)+,d2-d7/a2-a6\n"
    "    rts\n");

static void t(const char *name, LONG lvo, ULONG d1, ULONG d2, ULONG d3)
{
    ULONG r1 = 0x5a5a5a5a, r0 = dcall(lvo, d1, d2, d3, &r1);
    probe_s(name);
    probe_s(": d1 ");
    probe_s(r1 == r0 ? "= d0" : (r1 == d1 ? "unchanged" : "other"));
    probe_ch('\n');
}

static UBYTE buf[64];
static struct FileInfoBlock fib __attribute__((aligned(4)));
static struct InfoData info __attribute__((aligned(4)));
static char path[] = "SYS:C";
static char tmp[] = "T:probe-retd1";
static char nopath[] = "SYS:no-such-object";

int main(void)
{
    BPTR fh, lock;

    t("Input", -54, 0x13579bdf, 0, 0);
    t("Output", -60, 0x13579bdf, 0, 0);
    fh = Open((STRPTR)tmp, MODE_NEWFILE);
    Write(fh, (STRPTR)"0123456789", 10);
    Close(fh);
    t("Open", -30, (ULONG)tmp, MODE_OLDFILE, 0);
    fh = Open((STRPTR)tmp, MODE_OLDFILE);
    t("Read", -42, fh, (ULONG)buf, 4);
    t("Seek", -66, fh, 2, OFFSET_BEGINNING);
    t("Write", -48, fh, (ULONG)buf, 0);
    t("Close", -36, fh, 0, 0);
    t("Lock", -84, (ULONG)path, SHARED_LOCK, 0);
    t("Lock (fails)", -84, (ULONG)nopath, SHARED_LOCK, 0);
    lock = Lock((STRPTR)path, SHARED_LOCK);
    t("DupLock", -96, lock, 0, 0);
    t("Examine", -102, lock, MKBADDR(&fib), 0);
    t("Info", -114, lock, MKBADDR(&info), 0);
    t("ParentDir", -210, lock, 0, 0);
    t("IoErr", -132, 0x13579bdf, 0, 0);
    t("LoadSeg", -150, (ULONG)"C:Echo", 0, 0);
    t("LoadSeg (fails)", -150, (ULONG)nopath, 0, 0);
    t("DeviceProc", -174, (ULONG)path, 0, 0);
    t("IsInteractive", -216, Output(), 0, 0);
    t("DateStamp", -192, (ULONG)buf, 0, 0);
    t("DeleteFile", -72, (ULONG)tmp, 0, 0);
    t("SetIoErr (V36)", -462, 0, 0, 0);
    t("FilePart (V36)", -870, (ULONG)path, 0, 0);
    UnLock(lock);
    return 0;
}
