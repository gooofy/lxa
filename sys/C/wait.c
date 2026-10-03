/*
 * WAIT - wait for a time or until a time of day
 *
 * Template: TIME/N,SEC=SECS/S,MIN=MINS/S,UNTIL/K
 *
 * Waits TIME seconds (default 1; MINS: minutes) or UNTIL hh:mm (the next
 * time the clock shows it).  CTRL-C ends the wait (RC 5, "***Break").
 * As AmigaOS 3.1 (verified on the reference, Phase 221): a bad number is
 * reported as "bad number" (RC 20), a bad UNTIL as "Time should be HH:MM"
 * (RC 20).
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "TIME/N,SEC=SECS/S,MIN=MINS/S,UNTIL/K"

enum { A_TIME, A_SECS, A_MINS, A_UNTIL, A_COUNT };

/* wait 'ticks' (1/50 s) in steps, watching CTRL-C */
static BOOL wait_ticks(LONG ticks)
{
    while (ticks > 0) {
        LONG step = ticks > 50 ? 50 : ticks;
        Delay(step);
        ticks -= step;
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
            return FALSE;
    }
    return TRUE;
}

static BOOL parse_hhmm(const char *s, LONG *minutes)
{
    LONG h = 0, m = 0;
    int digits = 0;

    while (*s >= '0' && *s <= '9') {
        h = h * 10 + (*s++ - '0');
        digits++;
    }
    if (!digits || *s != ':')
        return FALSE;
    s++;
    digits = 0;
    while (*s >= '0' && *s <= '9') {
        m = m * 10 + (*s++ - '0');
        digits++;
    }
    if (!digits || *s || h > 23 || m > 59)
        return FALSE;
    *minutes = h * 60 + m;
    return TRUE;
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    LONG ticks;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }

    if (args[A_UNTIL]) {
        LONG target;
        struct DateStamp now;
        if (!parse_hhmm((char *)args[A_UNTIL], &target)) {
            PutStr((STRPTR)"Time should be HH:MM\n");
            FreeArgs(rda);
            SetIoErr(0);
            return RETURN_FAIL;
        }
        DateStamp(&now);
        if (target <= now.ds_Minute)
            target += 24 * 60;
        ticks = (target - now.ds_Minute) * 60 * TICKS_PER_SECOND - now.ds_Tick;
    } else {
        LONG t = args[A_TIME] ? *(LONG *)args[A_TIME] : 1;
        if (t < 0)
            t = 0;
        ticks = t * TICKS_PER_SECOND * (args[A_MINS] ? 60 : 1);
    }
    FreeArgs(rda);

    if (!wait_ticks(ticks)) {
        PutStr((STRPTR)"***Break\n");
        return RETURN_WARN;
    }
    return 0;
}
