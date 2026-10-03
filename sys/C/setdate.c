/*
 * SETDATE - set the date stamp of files and directories
 *
 * Template: FILE/A,WEEKDAY,DATE,TIME,ALL/S
 *
 * Without a date and time the current time is used.  FILE may be a
 * pattern; ALL also processes the contents of matching directories.
 * Messages follow AmigaOS 3.1 (verified on the reference, Phase 221):
 *   SetDate failed: <fault text>
 *   SetDate failed: Invalid DATE or TIME string!
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/datetime.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "FILE/A,WEEKDAY,DATE,TIME,ALL/S"

enum { A_FILE, A_WEEKDAY, A_DATE, A_TIME, A_ALL, A_COUNT };

static void failed(LONG err)
{
    char buf[100];
    Fault(err, (STRPTR)"SetDate failed", (STRPTR)buf, sizeof(buf));
    PutStr((STRPTR)buf);
    PutStr((STRPTR)"\n");
}

/* is the string a time ("12:00" / "12:00:00")? */
static BOOL is_time(const char *s)
{
    return strchr(s, ':') != NULL;
}

static LONG set_all(const char *pattern, struct DateStamp *ds, BOOL all)
{
    struct AnchorPath *ap = AllocVec(sizeof(struct AnchorPath) + 512, MEMF_PUBLIC | MEMF_CLEAR);
    LONG err, rc = 0;

    if (!ap)
        return RETURN_FAIL;
    ap->ap_Strlen = 512;
    for (err = MatchFirst((STRPTR)pattern, ap); !err; err = MatchNext(ap)) {
        if (ap->ap_Info.fib_DirEntryType >= 0) {
            if (ap->ap_Flags & APF_DIDDIR) {
                ap->ap_Flags &= ~APF_DIDDIR;
                continue;
            }
            if (all)
                ap->ap_Flags |= APF_DODIR;
        }
        if (!SetFileDate((STRPTR)ap->ap_Buf, ds)) {
            err = IoErr();
            failed(err);
            SetIoErr(err);
            rc = RETURN_FAIL;
            break;
        }
    }
    if (!rc && err != ERROR_NO_MORE_ENTRIES) {
        failed(err);
        SetIoErr(err);
        rc = RETURN_FAIL;
    }
    MatchEnd(ap);
    FreeVec(ap);
    return rc;
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    struct DateStamp ds;
    LONG rc;
    char pat[260];

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }

    DateStamp(&ds);
    if (args[A_WEEKDAY] || args[A_DATE] || args[A_TIME]) {
        /* the positional items: a weekday/date, then a time */
        const char *date = NULL, *time = NULL;
        const char *items[3];
        int i;
        items[0] = (char *)args[A_WEEKDAY];
        items[1] = (char *)args[A_DATE];
        items[2] = (char *)args[A_TIME];
        for (i = 0; i < 3; i++) {
            if (!items[i])
                continue;
            if (is_time(items[i]))
                time = items[i];
            else if (!date)
                date = items[i];
        }
        {
            struct DateTime dt;
            char dbuf[32], tbuf[32];
            memset(&dt, 0, sizeof(dt));
            dt.dat_Stamp = ds;
            dt.dat_Format = FORMAT_DOS;
            dt.dat_Flags = DTF_SUBST;
            if (date) {
                strncpy(dbuf, date, sizeof(dbuf) - 1);
                dbuf[sizeof(dbuf) - 1] = '\0';
                dt.dat_StrDate = (STRPTR)dbuf;
            }
            if (time) {
                strncpy(tbuf, time, sizeof(tbuf) - 1);
                tbuf[sizeof(tbuf) - 1] = '\0';
                dt.dat_StrTime = (STRPTR)tbuf;
            } else if (date) {
                /* a date alone means midnight */
                dt.dat_Stamp.ds_Minute = 0;
                dt.dat_Stamp.ds_Tick = 0;
            }
            if ((!date && !time) || !StrToDate(&dt)) {
                PutStr((STRPTR)"SetDate failed: Invalid DATE or TIME string!\n");
                FreeArgs(rda);
                SetIoErr(0);
                return RETURN_FAIL;
            }
            ds = dt.dat_Stamp;
        }
    }

    if (ParsePattern((STRPTR)args[A_FILE], (STRPTR)pat, sizeof(pat)) == 1 || args[A_ALL]) {
        rc = set_all((char *)args[A_FILE], &ds, args[A_ALL] != 0);
    } else if (!SetFileDate((STRPTR)args[A_FILE], &ds)) {
        LONG err = IoErr();
        failed(err);
        SetIoErr(err);
        rc = RETURN_FAIL;
    } else {
        rc = 0;
    }
    FreeArgs(rda);
    return rc;
}
