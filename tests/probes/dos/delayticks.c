/*
 * Probe (Phase 237d): how long Delay() really takes.  lxaprobe's WAIT_EXIT
 * counts 20 ms per Delay(1) and so waits about twice its budget on
 * AmigaOS 3.1; Fred Fish programs that sleep 10 s exit within a "6 s" run
 * there.  Measures n x Delay(k) with DateStamp() (in ticks, after
 * synchronising to a tick) and prints the elapsed ticks rounded to tens of
 * percent of the nominal time, so small jitter does not matter.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static LONG ticks(void)
{
    struct DateStamp ds;
    DateStamp(&ds);
    return ds.ds_Minute * 3000 + ds.ds_Tick;
}

static void t(const char *name, LONG n, LONG k)
{
    LONG i, t0, el, nominal = n * k;
    t0 = ticks();
    while (ticks() == t0)
        ;
    t0 = ticks();
    for (i = 0; i < n; i++)
        Delay(k);
    el = ticks() - t0;
    probe_s(name);
    probe_s(": ");
    if (nominal == 0)
        probe_s(el < n / 4 + 2 ? "no wait" : "waits");
    else {
        probe_dec((el * 10 + nominal / 2) / nominal);
        probe_s("/10 of the nominal time");
    }
    probe_ch('\n');
}

int main(void)
{
    t("100 x Delay(1)", 100, 1);
    t("50 x Delay(2)", 50, 2);
    t("20 x Delay(5)", 20, 5);
    t("4 x Delay(50)", 4, 50);
    t("100 x Delay(0)", 100, 0);
    return 0;
}
