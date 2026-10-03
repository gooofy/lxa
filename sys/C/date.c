/*
 * DATE - show or set the system date and time
 *
 * Template: DAY,DATE,TIME,TO=VER/K
 *
 * Without arguments: "Monday 01-Jan-24 12:34:56" (to TO if given).  A
 * date (dd-mmm-yy, a weekday, Today/Yesterday/Tomorrow) and/or a time
 * (hh:mm[:ss]) set the clock through timer.device TR_SETSYSTIME; the
 * part not given stays.  Messages of AmigaOS 3.1 (verified on the
 * reference, Phase 221).
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/datetime.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "DAY,DATE,TIME,TO=VER/K"

enum { A_DAY, A_DATE, A_TIME, A_TO, A_COUNT };

static void bad_args(void)
{
    PutStr((STRPTR)"***Bad args\n"
                   "- use DD-MMM-YY or <dayname> or yesterday etc. to set date\n"
                   "      HH:MM:SS OR HH:MM to set time\n");
}

static BOOL set_clock(struct DateStamp *ds)
{
    struct MsgPort *port = CreateMsgPort();
    struct timerequest *tr;
    BOOL ok = FALSE;

    if (!port)
        return FALSE;
    tr = (struct timerequest *)CreateIORequest(port, sizeof(struct timerequest));
    if (tr) {
        if (!OpenDevice((STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest *)tr, 0)) {
            tr->tr_node.io_Command = TR_SETSYSTIME;
            tr->tr_time.tv_secs = ds->ds_Days * 86400 + ds->ds_Minute * 60 + ds->ds_Tick / TICKS_PER_SECOND;
            tr->tr_time.tv_micro = (ds->ds_Tick % TICKS_PER_SECOND) * (1000000 / TICKS_PER_SECOND);
            ok = DoIO((struct IORequest *)tr) == 0;
            CloseDevice((struct IORequest *)tr);
        }
        DeleteIORequest((struct IORequest *)tr);
    }
    DeleteMsgPort(port);
    return ok;
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    struct DateTime dt;
    char day[16], date[16], time[16];

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        bad_args();
        SetIoErr(0);
        return RETURN_FAIL;
    }

    if (args[A_DAY] || args[A_DATE] || args[A_TIME]) {
        const char *items[3], *d = NULL, *t = NULL;
        char dbuf[32], tbuf[32];
        int i;
        items[0] = (char *)args[A_DAY];
        items[1] = (char *)args[A_DATE];
        items[2] = (char *)args[A_TIME];
        for (i = 0; i < 3; i++) {
            if (!items[i])
                continue;
            if (strchr(items[i], ':'))
                t = items[i];
            else if (!d)
                d = items[i];
            else {
                bad_args();
                FreeArgs(rda);
                return RETURN_FAIL;
            }
        }
        memset(&dt, 0, sizeof(dt));
        DateStamp(&dt.dat_Stamp);
        dt.dat_Format = FORMAT_DOS;
        dt.dat_Flags = DTF_SUBST;
        if (d) {
            strncpy(dbuf, d, sizeof(dbuf) - 1);
            dbuf[sizeof(dbuf) - 1] = '\0';
            dt.dat_StrDate = (STRPTR)dbuf;
        }
        if (t) {
            strncpy(tbuf, t, sizeof(tbuf) - 1);
            tbuf[sizeof(tbuf) - 1] = '\0';
            dt.dat_StrTime = (STRPTR)tbuf;
        }
        if (!StrToDate(&dt)) {
            bad_args();
            FreeArgs(rda);
            SetIoErr(0);
            return RETURN_FAIL;
        }
        set_clock(&dt.dat_Stamp);
        FreeArgs(rda);
        return 0;
    }

    memset(&dt, 0, sizeof(dt));
    DateStamp(&dt.dat_Stamp);
    dt.dat_Format = FORMAT_DOS;
    dt.dat_StrDay = (STRPTR)day;
    dt.dat_StrDate = (STRPTR)date;
    dt.dat_StrTime = (STRPTR)time;
    DateToStr(&dt);
    if (args[A_TO]) {
        BPTR fh = Open((STRPTR)args[A_TO], MODE_NEWFILE);
        LONG a[3];
        if (!fh) {
            LONG err = IoErr();
            PrintFault(err, (STRPTR)args[A_TO]);
            FreeArgs(rda);
            return RETURN_FAIL;
        }
        a[0] = (LONG)day;
        a[1] = (LONG)date;
        a[2] = (LONG)time;
        VFPrintf(fh, (STRPTR)"%s %s %s\n", a);
        Close(fh);
    } else {
        Printf((STRPTR)"%s %s %s\n", (LONG)day, (LONG)date, (LONG)time);
    }
    FreeArgs(rda);
    return 0;
}
