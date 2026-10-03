/*
 * Probe (Phase 222a): dos.library error codes and miscellany - Fault,
 * PrintFault, SetIoErr/IoErr, local and global variables (SetVar, GetVar,
 * FindVar, DeleteVar), DateToStr/StrToDate/CompareDates, AllocDosObject
 * defaults, CheckSignal.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/datetime.h>
#include <dos/var.h>
#include <dos/rdargs.h>
#include <dos/exall.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static void showdate(const char *label, struct DateStamp *ds, UBYTE format, UBYTE flags)
{
    struct DateTime dt;
    char day[LEN_DATSTRING + 4], date[LEN_DATSTRING + 4], time[LEN_DATSTRING + 4];
    BOOL ok;
    dt.dat_Stamp = *ds;
    dt.dat_Format = format;
    dt.dat_Flags = flags;
    dt.dat_StrDay = (STRPTR)day;
    dt.dat_StrDate = (STRPTR)date;
    dt.dat_StrTime = (STRPTR)time;
    day[0] = date[0] = time[0] = 0;
    ok = DateToStr(&dt);
    probe_s(label);
    probe_s(": ");
    probe_s(ok ? "ok" : "FAIL");
    probe_s(" day=\"");
    probe_s(day);
    probe_s("\" date=\"");
    probe_s(date);
    if (!flags) {                       /* "today" stamps have a wall-clock time */
        probe_s("\" time=\"");
        probe_s(time);
    }
    probe_s("\"\n");
}

static LONG preset_days = 77;

static void parsedate(const char *date, const char *time, UBYTE format, UBYTE flags)
{
    struct DateTime dt;
    BOOL ok;
    dt.dat_Stamp.ds_Days = preset_days;
    dt.dat_Stamp.ds_Minute = 77;
    dt.dat_Stamp.ds_Tick = 77;
    dt.dat_Format = format;
    dt.dat_Flags = flags;
    dt.dat_StrDay = NULL;
    dt.dat_StrDate = (STRPTR)date;
    dt.dat_StrTime = (STRPTR)time;
    ok = StrToDate(&dt);
    probe_s("StrToDate(\"");
    probe_s(date ? date : "NULL");
    probe_s("\", \"");
    probe_s(time ? time : "NULL");
    probe_s("\", fmt ");
    probe_dec(format);
    probe_s(") = ");
    probe_s(ok ? "ok" : "FAIL");
    probe_s(" days ");
    probe_dec(dt.dat_Stamp.ds_Days);
    probe_s(" min ");
    probe_dec(dt.dat_Stamp.ds_Minute);
    probe_s(" tick ");
    probe_dec(dt.dat_Stamp.ds_Tick);
    probe_ch('\n');
}

int main(void)
{
    char buf[128];
    LONG r, i;

    P_SECTION("Fault");
    {
        static const LONG codes[] = {
            0, 1, 103, 104, 105, 114, 115, 116, 117, 118, 119, 120, 121, 122, 202, 203, 204, 205, 206,
            209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226,
            232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 300, 303, 304, 305, 999, -1,
        };
        for (i = 0; i < (int)(sizeof(codes) / sizeof(codes[0])); i++) {
            int k;
            for (k = 0; k < 128; k++)
                buf[k] = 0;
            r = Fault(codes[i], NULL, (STRPTR)buf, sizeof(buf));
            probe_s("Fault(");
            probe_dec(codes[i]);
            probe_s(") = ");
            probe_dec(r);
            probe_s(" \"");
            probe_s(buf);
            probe_s("\"\n");
        }
        SetIoErr(0);
        r = Fault(205, (STRPTR)"hdr", (STRPTR)buf, sizeof(buf));
        P_LONG("Fault(205, \"hdr\") length", r);
        P_LONG("  IoErr after Fault", IoErr());
        P_STR("  text", buf);
        r = Fault(205, (STRPTR)"", (STRPTR)buf, sizeof(buf));
        P_LONG("Fault(205, \"\") length", r);
        P_STR("  text", buf);
        r = Fault(205, (STRPTR)"hdr", (STRPTR)buf, 10);
        P_LONG("Fault(205, \"hdr\", 10 bytes) length", r);
        P_STR("  text", buf);
        r = Fault(0, (STRPTR)"hdr", (STRPTR)buf, sizeof(buf));
        P_LONG("Fault(0, \"hdr\") length", r);
        P_STR("  text", buf);
        r = Fault(999, (STRPTR)"hdr", (STRPTR)buf, sizeof(buf));
        P_LONG("Fault(999, \"hdr\") length", r);
        P_STR("  text", buf);
        r = Fault(205, NULL, (STRPTR)buf, 1);
        P_LONG("Fault(205, NULL, 1 byte) length", r);
        P_LONG("  first byte", buf[0]);
    }

    P_SECTION("Fault sweep (codes with a message)");
    for (i = -5; i < 1000; i++) {
        r = Fault(i, NULL, (STRPTR)buf, sizeof(buf));
        if (r && !(buf[0] == 'E' && buf[1] == 'r' && buf[2] == 'r' && buf[3] == 'o' && buf[4] == 'r' && buf[5] == ' ')) {
            probe_s("Fault(");
            probe_dec(i);
            probe_s(") = \"");
            probe_s(buf);
            probe_s("\"\n");
        }
    }

    P_SECTION("Fault truncation");
    for (i = 1; i <= 24; i++) {
        int k;
        for (k = 0; k < 40; k++)
            buf[k] = '.';
        buf[39] = 0;
        SetIoErr(0);
        r = Fault(205, (STRPTR)"hdr", (STRPTR)buf, i);
        probe_s("Fault(205, \"hdr\", ");
        probe_dec(i);
        probe_s(") = ");
        probe_dec(r);
        probe_s(" \"");
        probe_s(buf);
        probe_s("\" IoErr ");
        probe_dec(IoErr());
        probe_ch('\n');
    }
    for (i = 1; i <= 20; i++) {
        int k;
        for (k = 0; k < 40; k++)
            buf[k] = '.';
        buf[39] = 0;
        r = Fault(205, NULL, (STRPTR)buf, i);
        probe_s("Fault(205, NULL, ");
        probe_dec(i);
        probe_s(") = ");
        probe_dec(r);
        probe_s(" \"");
        probe_s(buf);
        probe_s("\"\n");
    }
    for (i = 1; i <= 12; i++) {
        int k;
        for (k = 0; k < 40; k++)
            buf[k] = '.';
        buf[39] = 0;
        r = Fault(777, NULL, (STRPTR)buf, i);
        probe_s("Fault(777, NULL, ");
        probe_dec(i);
        probe_s(") = ");
        probe_dec(r);
        probe_s(" \"");
        probe_s(buf);
        probe_s("\"\n");
    }

    P_SECTION("PrintFault/SetIoErr");
    probe_flush();
    /* PrintFault() output is buffered: flush it to keep the order */
    r = PrintFault(205, (STRPTR)"pf");
    Flush(Output());
    P_BOOL("PrintFault(205, \"pf\")", r);
    P_LONG("IoErr after PrintFault(205)", IoErr());
    r = PrintFault(0, (STRPTR)"pf0");
    Flush(Output());
    P_BOOL("PrintFault(0, \"pf0\")", r);
    P_LONG("IoErr after PrintFault", IoErr());
    P_LONG("SetIoErr(123) old", SetIoErr(123));
    P_LONG("SetIoErr(-5) old", SetIoErr(-5));
    P_LONG("IoErr", IoErr());
    P_LONG("pr_Result2", ((struct Process *)FindTask(NULL))->pr_Result2);

    P_SECTION("variables");
    r = SetVar((STRPTR)"lxaprobe_v", (STRPTR)"value1", -1, GVF_LOCAL_ONLY);
    P_BOOL("SetVar local", r);
    r = GetVar((STRPTR)"lxaprobe_v", (STRPTR)buf, sizeof(buf), GVF_LOCAL_ONLY);
    P_LONG("GetVar local", r);
    P_STR("  value", buf);
    r = GetVar((STRPTR)"LXAPROBE_V", (STRPTR)buf, sizeof(buf), GVF_LOCAL_ONLY);
    P_LONG("GetVar local (other case)", r);
    r = GetVar((STRPTR)"lxaprobe_v", (STRPTR)buf, 4, GVF_LOCAL_ONLY);
    P_LONG("GetVar local, 4-byte buffer", r);
    P_STR("  value", buf);
    P_LONG("  IoErr", IoErr());
    r = SetVar((STRPTR)"lxaprobe_v", (STRPTR)"ab\0cd", 5, GVF_LOCAL_ONLY);
    P_BOOL("SetVar local with NUL, size 5", r);
    for (i = 0; i < 8; i++)
        buf[i] = 'x';
    r = GetVar((STRPTR)"lxaprobe_v", (STRPTR)buf, sizeof(buf), GVF_LOCAL_ONLY | GVF_BINARY_VAR);
    P_LONG("GetVar binary", r);
    P_BYTES("  bytes", buf, 7);
    r = SetVar((STRPTR)"lxaprobe_v", (STRPTR)"line1\nline2", -1, GVF_LOCAL_ONLY);
    r = GetVar((STRPTR)"lxaprobe_v", (STRPTR)buf, sizeof(buf), GVF_LOCAL_ONLY);
    P_LONG("GetVar multi-line (text mode)", r);
    P_STR("  value", buf);
    {
        struct LocalVar *lv = FindVar((STRPTR)"lxaprobe_v", LV_VAR);
        P_NULL("FindVar", lv);
        if (lv) {
            P_LONG("  lv_Node.ln_Type", lv->lv_Node.ln_Type);
            P_LONG("  lv_Len", lv->lv_Len);
            P_HEX("  lv_Flags", lv->lv_Flags);
        }
        P_NULL("FindVar(LV_ALIAS)", FindVar((STRPTR)"lxaprobe_v", LV_ALIAS));
    }
    P_BOOL("DeleteVar local", DeleteVar((STRPTR)"lxaprobe_v", GVF_LOCAL_ONLY));
    r = DeleteVar((STRPTR)"lxaprobe_v", GVF_LOCAL_ONLY);
    P_BOOL("DeleteVar local again", r);
    P_LONG("  IoErr", IoErr());
    r = GetVar((STRPTR)"lxaprobe_v", (STRPTR)buf, sizeof(buf), GVF_LOCAL_ONLY);
    P_LONG("GetVar deleted", r);
    P_LONG("  IoErr", IoErr());
    r = SetVar((STRPTR)"lxaprobe_g", (STRPTR)"global", -1, GVF_GLOBAL_ONLY);
    P_BOOL("SetVar global", r);
    r = GetVar((STRPTR)"lxaprobe_g", (STRPTR)buf, sizeof(buf), GVF_GLOBAL_ONLY);
    P_LONG("GetVar global", r);
    P_STR("  value", buf);
    r = GetVar((STRPTR)"lxaprobe_g", (STRPTR)buf, sizeof(buf), 0);
    P_LONG("GetVar (any) finds global", r);
    r = GetVar((STRPTR)"lxaprobe_g", (STRPTR)buf, sizeof(buf), GVF_LOCAL_ONLY);
    P_LONG("GetVar local-only misses global", r);
    SetVar((STRPTR)"lxaprobe_g", (STRPTR)"local", -1, GVF_LOCAL_ONLY);
    r = GetVar((STRPTR)"lxaprobe_g", (STRPTR)buf, sizeof(buf), 0);
    P_LONG("GetVar (any) prefers local", r);
    P_STR("  value", buf);
    DeleteVar((STRPTR)"lxaprobe_g", GVF_LOCAL_ONLY);
    P_BOOL("DeleteVar global", DeleteVar((STRPTR)"lxaprobe_g", GVF_GLOBAL_ONLY));
    r = GetVar((STRPTR)"lxaprobe_g", (STRPTR)buf, sizeof(buf), GVF_GLOBAL_ONLY);
    P_LONG("GetVar deleted global", r);
    P_LONG("  IoErr", IoErr());

    P_SECTION("DateToStr");
    {
        static const LONG stamps[][3] = {
            {0, 0, 0}, {1, 1, 1}, {365, 59, 49}, {2922, 720, 1500}, {7305, 1439, 2999}, {16436, 754, 50},
            {-1, 0, 0}, {50000, 0, 0},
        };
        static const char *const fmts[] = {"DOS", "INT", "USA", "CDN"};
        int f;
        for (i = 0; i < (int)(sizeof(stamps) / sizeof(stamps[0])); i++)
            for (f = 0; f <= FORMAT_CDN; f++) {
                struct DateStamp ds;
                char label[32];
                int k = 0;
                const char *s = fmts[f];
                ds.ds_Days = stamps[i][0];
                ds.ds_Minute = stamps[i][1];
                ds.ds_Tick = stamps[i][2];
                label[k++] = 's';
                label[k++] = (char)('0' + i);
                label[k++] = ' ';
                while (*s)
                    label[k++] = *s++;
                label[k] = 0;
                showdate(label, &ds, (UBYTE)f, 0);
            }
    }
    {
        struct DateStamp now;
        DateStamp(&now);
        showdate("DTF_SUBST (today)", &now, FORMAT_DOS, DTF_SUBST);
        now.ds_Days -= 1;
        showdate("DTF_SUBST (yesterday)", &now, FORMAT_DOS, DTF_SUBST);
        now.ds_Days += 2;
        showdate("DTF_SUBST (tomorrow)", &now, FORMAT_DOS, DTF_SUBST);
        now.ds_Days -= 4;
        showdate("DTF_SUBST (3 days ago)", &now, FORMAT_DOS, DTF_SUBST);
        now.ds_Days -= 10;
        showdate("DTF_SUBST (13 days ago)", &now, FORMAT_DOS, DTF_SUBST);
        {
            static const LONG deltas[] = {2, 3, 6, 7, -2, -5, -6, -7, -8};
            static const char *const names[] = {"+2", "+3", "+6", "+7", "-2", "-5", "-6", "-7", "-8"};
            struct DateStamp base;
            int d;
            DateStamp(&base);
            for (d = 0; d < 9; d++) {
                char label[32];
                const char *s = "DTF_SUBST ";
                int k = 0;
                while (*s) label[k++] = *s++;
                s = names[d];
                while (*s) label[k++] = *s++;
                label[k] = 0;
                now = base;
                now.ds_Days += deltas[d];
                showdate(label, &now, FORMAT_DOS, DTF_SUBST);
            }
            now = base;
            now.ds_Days -= 1;
            showdate("DTF_SUBST yesterday, FORMAT_USA", &now, FORMAT_USA, DTF_SUBST);
            showdate("DTF_FUTURE yesterday", &now, FORMAT_DOS, DTF_FUTURE);
            now.ds_Days += 3;
            showdate("DTF_FUTURE +2", &now, FORMAT_DOS, DTF_FUTURE | DTF_SUBST);
        }
    }

    P_SECTION("StrToDate");
    parsedate("01-Jan-78", "00:00:00", FORMAT_DOS, 0);
    parsedate("31-Dec-99", "23:59:59", FORMAT_DOS, 0);
    parsedate("29-Feb-00", "12:34:56", FORMAT_DOS, 0);
    parsedate("01-jan-78", "1:2:3", FORMAT_DOS, 0);
    parsedate("32-Jan-78", "00:00:00", FORMAT_DOS, 0);
    parsedate("01-Foo-78", "00:00:00", FORMAT_DOS, 0);
    parsedate("01-Jan-78", "25:00:00", FORMAT_DOS, 0);
    parsedate("01-Jan-78", "12:00", FORMAT_DOS, 0);
    parsedate("78-01-02", "10:00:00", FORMAT_INT, 0);
    parsedate("01-02-78", "10:00:00", FORMAT_USA, 0);
    parsedate("02-01-78", "10:00:00", FORMAT_CDN, 0);
    parsedate("01-Jan-78", NULL, FORMAT_DOS, 0);
    parsedate(NULL, "01:00:00", FORMAT_DOS, 0);
    parsedate("", "", FORMAT_DOS, 0);
    parsedate("01-Jan-2005", "00:00:00", FORMAT_DOS, 0);
    parsedate("01-Jan-77", "00:00:00", FORMAT_DOS, 0);
    parsedate("1-Jan-78", "00:00:00", FORMAT_DOS, 0);
    parsedate("01-January-78", "00:00:00", FORMAT_DOS, 0);
    parsedate("01-Jan-78 ", " 12:00:00", FORMAT_DOS, 0);
    parsedate("01/Jan/78", "12.00.00", FORMAT_DOS, 0);
    parsedate("30-Feb-80", "00:00:00", FORMAT_DOS, 0);
    parsedate("01-Jan-78", "23:60:00", FORMAT_DOS, 0);
    parsedate("01-Jan-78", "23:59:60", FORMAT_DOS, 0);
    parsedate("01-Jan-78", "7", FORMAT_DOS, 0);
    parsedate("01-Jan-78", "123:00:00", FORMAT_DOS, 0);
    parsedate("Today", "00:00:00", FORMAT_DOS, 0);
    parsedate("Yesterday", "00:00:00", FORMAT_DOS, DTF_SUBST);
    parsedate("Monday", "00:00:00", FORMAT_DOS, 0);
    parsedate("Sunday", "00:00:00", FORMAT_DOS, 0);
    parsedate("Tuesday", "00:00:00", FORMAT_DOS, 0);
    parsedate("Wednesday", "00:00:00", FORMAT_DOS, 0);
    parsedate("Thursday", "00:00:00", FORMAT_DOS, 0);
    parsedate("Friday", "00:00:00", FORMAT_DOS, 0);
    parsedate("Saturday", "00:00:00", FORMAT_DOS, 0);
    parsedate("Tomorrow", "00:00:00", FORMAT_DOS, 0);
    parsedate("Future", "00:00:00", FORMAT_DOS, 0);
    parsedate("tue", "00:00:00", FORMAT_DOS, 0);
    parsedate("Tuesday", "00:00:00", FORMAT_DOS, DTF_FUTURE);
    parsedate("Monday", "00:00:00", FORMAT_DOS, DTF_FUTURE);
    parsedate("Sunday", "00:00:00", FORMAT_DOS, DTF_FUTURE);
    preset_days = 78;
    probe_s("(stamp preset to day 78)\n");
    parsedate("Monday", "00:00:00", FORMAT_DOS, 0);
    parsedate("Sunday", "00:00:00", FORMAT_DOS, 0);
    parsedate("Monday", "00:00:00", FORMAT_DOS, DTF_FUTURE);
    preset_days = 80;
    probe_s("(stamp preset to day 80)\n");
    parsedate("Monday", "00:00:00", FORMAT_DOS, 0);
    parsedate("Today", "00:00:00", FORMAT_DOS, 0);
    parsedate("Monday", "00:00:00", FORMAT_DOS, DTF_FUTURE);
    preset_days = 77;
    parsedate("01-01-78", "00:00:00", FORMAT_DOS, 0);
    parsedate("02-Jan-78", "00:00:00", FORMAT_CDN, 0);
    parsedate("Jan-02-78", "00:00:00", FORMAT_USA, 0);
    parsedate("13-01-78", "00:00:00", FORMAT_USA, 0);
    parsedate("78-13-01", "00:00:00", FORMAT_INT, 0);
    parsedate("78-Jan-02", "00:00:00", FORMAT_INT, 0);

    P_SECTION("CompareDates");
    {
        struct DateStamp a, b;
        a.ds_Days = 10; a.ds_Minute = 5; a.ds_Tick = 1;
        b = a;
        P_LONG("CompareDates(equal)", CompareDates(&a, &b));
        b.ds_Tick = 2;
        P_LONG("CompareDates(a, b later tick) sign", CompareDates(&a, &b) < 0 ? -1 : CompareDates(&a, &b) > 0 ? 1 : 0);
        b.ds_Tick = 0; b.ds_Minute = 6;
        P_LONG("CompareDates(a, b later minute) sign", CompareDates(&a, &b) < 0 ? -1 : CompareDates(&a, &b) > 0 ? 1 : 0);
        b.ds_Days = 9; b.ds_Minute = 1000;
        P_LONG("CompareDates(a, b earlier day) sign", CompareDates(&a, &b) < 0 ? -1 : CompareDates(&a, &b) > 0 ? 1 : 0);
    }

    P_SECTION("AllocDosObject");
    {
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        struct RDArgs *rda = AllocDosObject(DOS_RDARGS, NULL);
        struct ExAllControl *eac = AllocDosObject(DOS_EXALLCONTROL, NULL);
        struct FileHandle *fh = AllocDosObject(DOS_FILEHANDLE, NULL);
        struct DateTime *dt = AllocDosObject(DOS_STDPKT, NULL);
        P_NULL("DOS_FIB", fib);
        if (fib) {
            ULONG sum = 0;
            for (i = 0; i < (LONG)sizeof(*fib); i++)
                sum += ((UBYTE *)fib)[i];
            P_LONG("  FIB byte sum (cleared)", (LONG)sum);
            FreeDosObject(DOS_FIB, fib);
        }
        P_NULL("DOS_RDARGS", rda);
        if (rda) {
            P_LONG("  RDA_Source.CS_Length", rda->RDA_Source.CS_Length);
            P_LONG("  RDA_Flags", rda->RDA_Flags);
            P_NULL("  RDA_Buffer", rda->RDA_Buffer);
            FreeDosObject(DOS_RDARGS, rda);
        }
        P_NULL("DOS_EXALLCONTROL", eac);
        if (eac) {
            P_LONG("  eac_Entries", eac->eac_Entries);
            P_LONG("  eac_LastKey", eac->eac_LastKey);
            FreeDosObject(DOS_EXALLCONTROL, eac);
        }
        P_NULL("DOS_FILEHANDLE", fh);
        if (fh) {
            P_LONG("  fh_Pos", fh->fh_Pos);
            P_LONG("  fh_End", fh->fh_End);
            P_LONG("  fh_Arg1", fh->fh_Arg1);
            FreeDosObject(DOS_FILEHANDLE, fh);
        }
        P_NULL("DOS_STDPKT", dt);
        if (dt) {
            struct StandardPacket *sp = (struct StandardPacket *)dt;
            P_BOOL("  sp_Msg.mn_Node.ln_Name == &sp_Pkt", sp->sp_Msg.mn_Node.ln_Name == (char *)&sp->sp_Pkt);
            P_BOOL("  sp_Pkt.dp_Link == &sp_Msg", sp->sp_Pkt.dp_Link == &sp->sp_Msg);
            FreeDosObject(DOS_STDPKT, dt);
        }
        P_NULL("type 99", AllocDosObject(99, NULL));
    }

    P_SECTION("CheckSignal");
    SetSignal(0, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
    P_HEX("CheckSignal(C|D) none", CheckSignal(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D));
    Signal(FindTask(NULL), SIGBREAKF_CTRL_D);
    P_HEX("CheckSignal(C|D) after D", CheckSignal(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D));
    P_HEX("CheckSignal(C|D) cleared", CheckSignal(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D));
    return 0;
}
