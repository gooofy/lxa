/*
 * Virtual clock test (Phase 201)
 *
 * Checks that the time sources an application sees advance consistently:
 *   - DateStamp() advances by ~50 ticks across Delay(50)
 *   - timer.device GetSysTime() advances by ~1 s across Delay(50)
 *   - a 0.5 s UNIT_MICROHZ TR_ADDREQUEST takes ~0.5 s of GetSysTime() time
 *   - ReadEClock() advances by ~E-clock-frequency ticks per second
 *
 * The tolerances are those of a real Amiga, so the program also passes on
 * the reference system.  It prints the boot DateStamp day so the host-side
 * driver can verify the deterministic epoch.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/timer.h>
#include <stdio.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct Device *TimerBase;

static int failures = 0;

static void check(const char *what, LONG value, LONG lo, LONG hi)
{
    if (value >= lo && value <= hi)
        printf("OK: %s = %ld\n", what, value);
    else
    {
        printf("FAIL: %s = %ld (expected %ld..%ld)\n", what, value, lo, hi);
        failures++;
    }
}

static LONG usec_diff(struct timeval *a, struct timeval *b)
{
    return (LONG)(b->tv_secs - a->tv_secs) * 1000000L + ((LONG)b->tv_micro - (LONG)a->tv_micro);
}

int main(void)
{
    struct DateStamp ds1, ds2;
    struct timeval tv1, tv2;
    struct EClockVal ec1, ec2;
    struct MsgPort *port;
    struct timerequest *tr;
    ULONG efreq;
    LONG ticks;

    port = CreateMsgPort();
    tr = (struct timerequest *)CreateIORequest(port, sizeof(struct timerequest));
    if (!port || !tr || OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0))
    {
        printf("FAIL: cannot open timer.device\n");
        return 20;
    }
    TimerBase = tr->tr_node.io_Device;

    DateStamp(&ds1);
    printf("INFO: boot day %ld\n", ds1.ds_Days);

    /* Delay(50) vs DateStamp and GetSysTime */
    DateStamp(&ds1);
    GetSysTime(&tv1);
    Delay(50);
    DateStamp(&ds2);
    GetSysTime(&tv2);
    ticks = (ds2.ds_Days - ds1.ds_Days) * 24L * 60 * 50 + (ds2.ds_Minute - ds1.ds_Minute) * 60L * 50
          + (ds2.ds_Tick - ds1.ds_Tick);
    check("DateStamp ticks across Delay(50)", ticks, 49, 53);
    check("GetSysTime ms across Delay(50)", usec_diff(&tv1, &tv2) / 1000, 980, 1060);

    /* 0.5 s timer request */
    GetSysTime(&tv1);
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_secs = 0;
    tr->tr_time.tv_micro = 500000;
    DoIO((struct IORequest *)tr);
    GetSysTime(&tv2);
    check("TR_ADDREQUEST 500ms elapsed ms", usec_diff(&tv1, &tv2) / 1000, 495, 540);

    /* E-clock across Delay(25) */
    efreq = ReadEClock(&ec1);
    Delay(25);
    ReadEClock(&ec2);
    check("ReadEClock ms across Delay(25)",
          (LONG)(((ec2.ev_lo - ec1.ev_lo) * 10UL) / (efreq / 100UL)), 480, 540);

    CloseDevice((struct IORequest *)tr);
    DeleteIORequest((struct IORequest *)tr);
    DeleteMsgPort(port);

    if (failures == 0)
        printf("PASS: virtual clock consistent\n");
    return failures ? 10 : 0;
}
