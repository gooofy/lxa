/*
 * Probe: ARexx through RexxMast - a program sends RXCOMM/RXFUNC packets
 * to the REXX port and serves its own host port "PROBEHOST" while the
 * scripts run: script results and error codes, host commands (RC,
 * RESULT, OPTIONS RESULTS), ADDRESS, function hosts (ADDLIB), the Clip
 * List (RXADDCON/RXREMCON, SETCLIP/GETCLIP), script files with
 * arguments and extensions, script I/O (rm_Stdout) and the RX command.
 * Output compared with AmigaOS 3.1.
 */
#include "rexxprobe.h"

static struct MsgPort *host;    /* public "PROBEHOST" */
static struct MsgPort *reply;   /* our reply port */
static int done_seen;
static int broken;              /* a packet never came back: stop */

static void host_reply(struct RexxMsg *rm, LONG rc, const char *result)
{
    rm->rm_Result1 = rc;
    rm->rm_Result2 = 0;
    if (rc == 0 && result && (rm->rm_Action & RXFF_RESULT))
        rm->rm_Result2 = (LONG)CreateArgstring((STRPTR)result, str_len(result));
    ReplyMsg((struct Message *)rm);
}

static int starts(const char *s, const char *p)
{
    while (*p)
        if (*s++ != *p++)
            return 0;
    return 1;
}

/* one message at PROBEHOST: print it and answer */
static void serve(struct RexxMsg *rm)
{
    LONG code = rm->rm_Action & RXCODEMASK;
    LONG nargs = rm->rm_Action & RXARGMASK;
    char *a0 = (char *)rm->rm_Args[0];
    static char buf[200];
    int i, n;

    probe_s("  host: ");
    probe_s(code == RXCOMM ? "RXCOMM" : code == RXFUNC ? "RXFUNC" : "other");
    probe_s(" flags=");
    probe_hex(rm->rm_Action & 0x00ff0000, 8);
    probe_s(" nargs=");
    probe_dec(nargs);
    probe_s(" check=");
    probe_s(CHECKREXXMSG(rm) ? "TRUE" : "FALSE");
    probe_s(" comm=");
    probe_s(rm->rm_CommAddr ? (char *)rm->rm_CommAddr : "NULL");
    probe_s(" ext=");
    probe_s(rm->rm_FileExt ? (char *)rm->rm_FileExt : "NULL");
    probe_s(" arg0=\"");
    probe_s(a0 ? a0 : "NULL");
    probe_s("\"");
    if (a0) {
        probe_s(" len=");
        probe_dec(LengthArgstring((UBYTE *)a0));
    }
    for (i = 1; i <= nargs && i < 16 && code == RXFUNC; i++) {
        probe_s(" arg");
        probe_dec(i);
        probe_s("=");
        probe_s(rm->rm_Args[i] ? (char *)rm->rm_Args[i] : "NULL");
    }
    probe_ch('\n');

    if (code == RXFUNC) {
        if (a0 && str_eq(a0, "PROBEFN")) {
            n = 0;
            for (i = 1; i <= nargs && i < 16; i++) {
                const char *s = rm->rm_Args[i] ? (char *)rm->rm_Args[i] : "-";
                while (*s && n < (int)sizeof(buf) - 2)
                    buf[n++] = *s++;
                buf[n++] = '+';
            }
            buf[n] = 0;
            host_reply(rm, 0, buf);
        } else if (a0 && str_eq(a0, "FAILFN")) {
            host_reply(rm, 10, NULL);
        } else {
            host_reply(rm, 1, NULL);   /* not mine */
        }
        return;
    }
    if (a0 && starts(a0, "ECHO ")) {
        host_reply(rm, 0, a0 + 5);
    } else if (a0 && str_eq(a0, "FAIL")) {
        host_reply(rm, 10, NULL);
    } else if (a0 && str_eq(a0, "WARN")) {
        host_reply(rm, 5, NULL);
    } else if (a0 && str_eq(a0, "FATAL")) {
        host_reply(rm, 20, NULL);
    } else if (a0 && str_eq(a0, "EMPTY")) {
        host_reply(rm, 0, "");
    } else if (a0 && str_eq(a0, "DONE")) {
        done_seen = 1;
        host_reply(rm, 0, NULL);
    } else {
        host_reply(rm, 30, NULL);
    }
}

static void serve_pending(void)
{
    struct RexxMsg *rm;
    while ((rm = (struct RexxMsg *)GetMsg(host)))
        serve(rm);
}

/* put a packet to REXX and serve PROBEHOST until it comes back (~20 s) */
static BOOL send_wait(struct RexxMsg *rm)
{
    struct MsgPort *rexx;
    struct Message *m;
    int t;

    Forbid();
    rexx = FindPort((STRPTR)"REXX");
    if (rexx)
        PutMsg(rexx, (struct Message *)rm);
    Permit();
    if (!rexx) {
        probe_s("  (no REXX port)\n");
        return FALSE;
    }
    for (t = 0; t < 1000; t++) {
        serve_pending();
        if ((m = GetMsg(reply))) {
            serve_pending();
            return TRUE;
        }
        Delay(1);
    }
    probe_s("  (timeout)\n");
    broken = 1;
    return FALSE;
}

static void print_result(struct RexxMsg *rm)
{
    probe_s("  rc=");
    probe_dec(rm->rm_Result1);
    if (rm->rm_Result1 == 0 && (rm->rm_Action & RXFF_RESULT)) {
        if (rm->rm_Result2) {
            probe_s(" result=\"");
            probe_s((char *)rm->rm_Result2);
            probe_s("\" len=");
            probe_dec(LengthArgstring((UBYTE *)rm->rm_Result2));
            DeleteArgstring((UBYTE *)rm->rm_Result2);
        } else
            probe_s(" result=NULL");
    } else {
        probe_s(" result2=");
        probe_dec(rm->rm_Result2);
    }
    probe_ch('\n');
}

/* run a packet with up to 3 arguments; mask: integer arguments */
static void packet(const char *label, LONG action, const char *a0, const char *a1,
                   const char *a2, ULONG mask, BPTR out)
{
    struct RexxMsg *rm = CreateRexxMsg(reply, (STRPTR)"rexx", (STRPTR)"PROBEHOST");
    LONG n = 0;

    probe_s(label);
    probe_ch('\n');
    if (!rm || broken)
        return;
    rm->rm_Args[0] = (STRPTR)a0;
    rm->rm_Args[1] = (STRPTR)a1;
    rm->rm_Args[2] = (STRPTR)a2;
    n = a2 || (mask & 4) ? 3 : a1 || (mask & 2) ? 2 : 1;
    if (!FillRexxMsg(rm, n, mask)) {
        probe_s("  FillRexxMsg failed\n");
        DeleteRexxMsg(rm);
        return;
    }
    rm->rm_Action = action | ((action & RXCODEMASK) == RXFUNC ? n - 1 : 0);
    if (out) {
        rm->rm_Stdout = (LONG)out;
        rm->rm_Stdin = (LONG)out;
    }
    if (!send_wait(rm))
        return;   /* still owned by REXX */
    print_result(rm);
    ClearRexxMsg(rm, n);
    DeleteRexxMsg(rm);
}

/* RXADDCON: ARG0 name, ARG1 value (argstrings), ARG2 the value's length
 * as a plain integer */
static void addcon(const char *name, const char *value)
{
    struct RexxMsg *rm = CreateRexxMsg(reply, (STRPTR)"rexx", (STRPTR)"PROBEHOST");

    probe_s("RXADDCON ");
    probe_s(name);
    probe_ch('\n');
    if (!rm || broken)
        return;
    rm->rm_Args[0] = (STRPTR)name;
    rm->rm_Args[1] = (STRPTR)value;
    FillRexxMsg(rm, 2, 0);
    rm->rm_Args[2] = (STRPTR)str_len(value);
    rm->rm_Action = RXADDCON;
    if (!send_wait(rm))
        return;
    print_result(rm);
    rm->rm_Args[2] = NULL;
    ClearRexxMsg(rm, 2);
    DeleteRexxMsg(rm);
}

static void script(const char *text)
{
    probe_s("script: ");
    packet(text, RXCOMM | RXFF_STRING | RXFF_RESULT, text, NULL, NULL, 0, 0);
}

static void script_nores(const char *text)
{
    probe_s("script (no RXFF_RESULT): ");
    packet(text, RXCOMM | RXFF_STRING, text, NULL, NULL, 0, 0);
}

static void rx_sync(const char *label, const char *cmd)
{
    BPTR in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR out = Open((STRPTR)"T:rexxprobe.out", MODE_NEWFILE);
    LONG rc;

    probe_s(label);
    probe_ch('\n');
    rc = SystemTags((STRPTR)cmd, SYS_Input, in, SYS_Output, out, TAG_DONE);
    Close(in);
    Close(out);
    P_LONG("  rc", rc);
    P_FILE("  out", "T:rexxprobe.out");
    DeleteFile((STRPTR)"T:rexxprobe.out");
}

static void rx_async(const char *label, const char *cmd)
{
    BPTR in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    LONG rc;
    int t;

    probe_s(label);
    probe_ch('\n');
    done_seen = 0;
    rc = SystemTags((STRPTR)cmd, SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE, TAG_DONE);
    P_LONG("  SystemTags", rc);
    for (t = 0; t < 1000 && !done_seen; t++) {
        serve_pending();
        if (!done_seen)
            Delay(1);
    }
    if (!done_seen)
        probe_s("  (timeout)\n");
    Delay(25);   /* let RX finish */
}

int main(void)
{
    BPTR fh;

    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 0);
    if (!RexxSysBase)
        return 0;
    P_NULL("REXX port before RexxMast", find_port("REXX"));
    P_BOOL("start RexxMast", start_rexxmast());
    P_BOOL("REXX port is rl_RexxPort", find_port("REXX") == &RexxSysBase->rl_RexxPort);

    reply = CreateMsgPort();
    host = CreateMsgPort();
    host->mp_Node.ln_Name = (char *)"PROBEHOST";
    host->mp_Node.ln_Pri = 0;
    AddPort(host);

    write_file("T:pfunc.rexx", "/* function */\nparse arg a, b\nreturn a + b\n");
    write_file("T:pcmd.rexx", "/* command */\nparse arg x\nreturn '[' || x || ']' arg()\n");
    write_file("T:pnocomment.rexx", "return 5\n");
    write_file("T:pexit.rexx", "/* exit code */\nexit 12\n");

    P_SECTION("results");
    script("return 1+2");
    script("return 'abc'");
    script("return");
    script("exit 7");
    script("exit 'xyz'");
    script("exit");
    script("return 10/4");
    script("return 2**10");
    script("return upper('abc') || d2c(65) || c2d('A')");
    script("return address()");
    script("return arg() '|' arg(1)");
    script_nores("return 'ignored'");
    script_nores("exit 6");

    P_SECTION("builtins");
    script("return 1/3");
    script("return 1e3 + 0");
    script("return 'a' = 'A'");
    script("return datatype(12) datatype('x') datatype('12', 'W')");
    script("return c2x('AB') b2c('01000001') c2b('A')");
    script("return bitand('ff'x, '0f'x) == '0f'x");
    script("return compress(' a b  c ') '|' trim('  x  ') '|' strip('  y  ')");
    script("return hash('abc') index('abcabc', 'c') find('a b c', 'b')");
    script("return translate('abc') right(5, 3, 0) left('ab', 4, '.')");
    script("return sourceline() sourceline(1)");
    script("parse source s; return s");
    script("parse version v; return v");
    script("return show('P', 'AREXX')");
    script("return digits() fuzz() form()");
    script("return d2x(255) x2d('ff') d2c(72) || d2c(105)");
    script("return words('a b c') word('a b c', 2) subword('a b c d', 2, 2)");
    script("return reverse('abc') copies('ab', 3) overlay('x', 'abc', 2)");
    script("return verify('abc', 'ab') abbrev('print', 'pri') xrange('a', 'e')");
    script("return max(1, 5, 3) min(4, 2) abs(-3) sign(-2) trunc(3.7)");
    script("return format(3.14159, 2, 2) '|' format(5)");
    script("x.1 = 'one'; i = 1; return x.i symbol('x.1') symbol('nope') symbol('1')");
    script("call open('f', 'T:pio', 'W'); call writeln('f', 'line1'); call close('f'); "
           "call open('g', 'T:pio', 'R'); x = readln('g'); y = eof('g'); call close('g'); "
           "return x y exists('T:pio') exists('T:nope')");
    script("return open('h', 'T:nope/x', 'R') close('nope')");
    script("interpret 'x = 2 * 21'; return x");
    script("return f(3); f: procedure; parse arg n; return n * 2");
    script("return value('nope') nope");
    script("signal on novalue; return nope2; novalue: return 'novalue' sigl");

    P_SECTION("conditions");
    script("signal on error; 'WARN'; return 'no'; error: return 'trapped' rc sigl");
    script("signal on failure; 'FAIL'; return 'no'; failure: return 'ftrap' rc sigl");
    script("signal on error; 'FAIL'; return 'no'; error: return 'etrap' rc sigl");
    script("signal on syntax; x = 1/0; syntax: return 'syn' rc sigl errortext(rc)");
    script("signal on syntax; x = nofunc(); syntax: return 'syn' rc errortext(rc)");
    script("options failat 30; 'FAIL'; 'UNKNOWN'; return rc");
    script("options failat 3; 'WARN'; return rc");
    script("trace off; 'FAIL'; return rc");
    script("trace e; 'WARN'; return rc");
    script("address command 'SYS:C/Wait 0'; return rc");
    script("address command 'nosuchcommand'; return rc");
    script("address command; 'SYS:C/Wait 0'; return rc address()");

    P_SECTION("errortext");
    {
        static char line[40];
        static const char pre[] = "return errortext(";
        int e, k;
        for (e = 0; e <= 50; e++) {
            for (k = 0; pre[k]; k++)
                line[k] = pre[k];
            if (e >= 10)
                line[k++] = (char)('0' + e / 10);
            line[k++] = (char)('0' + e % 10);
            line[k++] = ')';
            line[k] = 0;
            script(line);
        }
    }

    P_SECTION("errors");
    script("x = (");
    script("x = 1/0");
    script("x = 'a' + 1");
    script("x = nosuchfunction(1)");
    script("do i = 1 to 3");
    script("end");
    script("say");
    script("signal nolabel");
    script("call nolabel2");
    script("x = substr('abc', 0)");
    script("if 2 then nop");
    script("x = 'zz'x");
    script("x = 'abc");
    script("x = 1 /* open");
    script("if 1 then; else nop; else nop");
    script("select; when 1 then nop; end; otherwise");
    script("x = abs()");
    script("x = left('a', -1)");
    script("leave");
    script("procedure");
    script("x = 1 +");
    script("x = (1");
    script("x = 1)");
    script("parse var");
    script("trace 'z'");
    script("x = 1e999999999 * 10");
    script("numeric digits 0");

    P_SECTION("host commands");
    script("options results; 'ECHO hello'; return rc '|' result");
    script("'ECHO noresults'; return rc '|' result");
    script("options results; 'FAIL'; return rc '|' result");
    script("'WARN'; return rc");
    script("'FATAL'; return rc");
    script("'UNKNOWN COMMAND'; return rc");
    script("options results; 'EMPTY'; return rc '|' length(result)");
    script("options results; address PROBEHOST 'ECHO viaaddr'; return result address()");
    script("address NOSUCHPORT 'x'; return rc");
    script("address NOSUCHPORT; 'y'; return rc address()");
    script("options results; 'ECHO a'; options; 'ECHO b'; return result");
    script("options results; x = 'ECHO' 'spaced  out'; x; return result");
    script("return show('P','PROBEHOST') show('P','NOSUCHPORT') show('P','REXX')");

    P_SECTION("function hosts");
    script("return addlib('PROBEHOST', 0)");
    script("return probefn('a', 'b', 3)");
    script("return probefn()");
    script("x = failfn(1); return x");
    script("x = otherfn(1); return x");
    script("return show('L', 'PROBEHOST')");
    script("return remlib('PROBEHOST')");
    script("return remlib('PROBEHOST')");
    script("x = probefn(1); return x");

    P_SECTION("clip list");
    script("return setclip('pclip', 'cval')");
    script("return getclip('pclip') '|' getclip('nope') '|' show('C', 'pclip')");
    addcon("pclip2", "value2");
    script("return getclip('pclip2')");
    packet("RXREMCON pclip2", RXREMCON, "pclip2", NULL, NULL, 0, 0);
    script("return '['getclip('pclip2')']'");
    packet("RXREMCON nope", RXREMCON, "nope", NULL, NULL, 0, 0);
    script("return setclip('pclip')");
    script("return '['getclip('pclip')']'");

    P_SECTION("script files");
    packet("RXCOMM T:pcmd.rexx one two", RXCOMM | RXFF_RESULT, "T:pcmd.rexx one two", NULL, NULL, 0, 0);
    packet("RXCOMM T:pcmd one (extension)", RXCOMM | RXFF_RESULT, "T:pcmd one", NULL, NULL, 0, 0);
    packet("RXCOMM T:pexit", RXCOMM | RXFF_RESULT, "T:pexit", NULL, NULL, 0, 0);
    packet("RXCOMM T:pnocomment", RXCOMM | RXFF_RESULT, "T:pnocomment", NULL, NULL, 0, 0);
    packet("RXCOMM nosuchscript", RXCOMM | RXFF_RESULT, "nosuchscript", NULL, NULL, 0, 0);
    packet("RXFUNC T:pfunc 2 3", RXFUNC | RXFF_RESULT, "T:pfunc", "2", "3", 0, 0);
    packet("RXFUNC T:pcmd x y", RXFUNC | RXFF_RESULT, "T:pcmd", "x", "y", 0, 0);
    script("return pfunc(4, 5)");

    P_SECTION("script output");
    fh = Open((STRPTR)"T:rexxprobe.say", MODE_NEWFILE);
    packet("say to rm_Stdout", RXCOMM | RXFF_STRING | RXFF_RESULT,
           "say 'hello' 1+1; return 0", NULL, NULL, 0, fh);
    Close(fh);
    P_FILE("  out", "T:rexxprobe.say");
    DeleteFile((STRPTR)"T:rexxprobe.say");

    P_SECTION("rx");
    rx_sync("rx \"say 6*7\"", "SYS:Rexxc/RX \"say 6*7\"");
    rx_sync("rx \"exit 5\"", "SYS:Rexxc/RX \"exit 5\"");
    rx_sync("rx T:pcmd hello", "SYS:Rexxc/RX T:pcmd hello");
    rx_sync("rx T:pexit", "SYS:Rexxc/RX T:pexit");
    rx_sync("rx nosuchscript", "SYS:Rexxc/RX nosuchscript");
    rx_async("rx async host commands",
             "SYS:Rexxc/RX \"address PROBEHOST; 'ECHO fromrx'; 'DONE'\"");

    RemPort(host);
    serve_pending();
    DeleteMsgPort(host);
    DeleteMsgPort(reply);
    DeleteFile((STRPTR)"T:pfunc.rexx");
    DeleteFile((STRPTR)"T:pcmd.rexx");
    DeleteFile((STRPTR)"T:pnocomment.rexx");
    DeleteFile((STRPTR)"T:pexit.rexx");

    P_BOOL("stop RexxMast", stop_rexxmast());
    CloseLibrary((struct Library *)RexxSysBase);
    return 0;
}
