/*
 * Probe (Phase 222f): clipboard.device - OpenDevice units, CMD_WRITE /
 * CMD_UPDATE / CMD_READ semantics, CBD_CURRENTREADID / CBD_CURRENTWRITEID,
 * CBD_POST + SatisfyMsg, CBD_CHANGEHOOK and the hold-off rules: an
 * unfinished read holds off writes and posts, a write in progress holds
 * off reads.
 *
 * Clip IDs are printed relative to the unit's CBD_CURRENTWRITEID at the
 * start of each section ("base+N"), so earlier clipboard users do not
 * matter.  Each hold-off scenario uses its own unit and leaves it clean
 * (no unfinished read, write or post), so the probe can run again in the
 * same session.  All requests go through helper processes
 * (clipasync.h): on AmigaOS 3.1 a held request blocks its caller.
 *
 * Left out on purpose (crash or hang AmigaOS 3.1): two writes in progress
 * at once, CMD_UPDATE without a preceding write, reads of future clip IDs.
 */
#include <utility/hooks.h>
#include "clipasync.h"

static struct MsgPort *port, *sport;
static UBYTE buf[128];

/* FORM FTXT, CHRS "Hello Clipboard!" (36 bytes) */
static const char clipA[] = "FORM\0\0\0\x1c" "FTXTCHRS\0\0\0\x10" "Hello Clipboard!";
#define CLIPA_LEN 36
static const char clipB[] = "FORM\0\0\0\x14" "FTXTCHRS\0\0\0\x08" "Clip two";
#define CLIPB_LEN 28

static void fill(void)
{
    int i;
    for (i = 0; i < (int)sizeof(buf); i++)
        buf[i] = 0xee;
}

/* (re)open request slots 0..4 on 'unit' and set the ID base */
static void begin(const char *name, ULONG unit)
{
    int i;
    for (i = 0; i < 5; i++) {
        if (jobs[i].req && jobs[i].busy) {
            probe_s("  (slot still busy: ");
            probe_dec(i);
            probe_s(")\n");
            continue;
        }
        close_job(i);
        if (!open_job(i, port, unit)) {
            probe_s("  (open failed)\n");
            jobs[i].req = NULL;
        }
    }
    P_SECTION(name);
    base = cur_id(3, CBD_CURRENTWRITEID);
}

/* write a whole clip and commit it; returns its ID */
static LONG put(const char *d, LONG len)
{
    setup(0, 0, 0, (APTR)d, len);
    if (!run(0, CMD_WRITE))
        return -2;
    if (!run(0, CMD_UPDATE))
        return -3;
    return jobs[0].req->io_ClipID;
}

/* read on until the clip ends (io_Actual 0); returns the number of reads */
static LONG fin(int n)
{
    LONG i;
    for (i = 1; i <= 6; i++) {
        jobs[n].req->io_Data = NULL;
        jobs[n].req->io_Length = 100000;
        if (!run(n, CMD_READ))
            return -1;
        if (jobs[n].req->io_Actual == 0)
            return i;
    }
    return i;
}

/* start a new clip on slot 4 and tell whether it gets through */
static void try_write(const char *label)
{
    setup(4, 0, 0, (APTR)clipB, CLIPB_LEN);
    start(4, CMD_WRITE);
    probe_s(label);
    if (wait_done(4, 15)) {
        probe_s(": write not held off\n");
        show("    write", 4);
        run(4, CMD_UPDATE);
        show("    update", 4);
    } else
        probe_s(": write held off\n");
}

/* has the held write on slot 4 got through by now? */
static void held(const char *label)
{
    if (jobs[4].req->io_Command != CMD_WRITE)
        return;
    probe_s(label);
    if (wait_done(4, 15)) {
        probe_s(": held write done\n");
        show("    write", 4);
        run(4, CMD_UPDATE);
        show("    update", 4);
    } else
        probe_s(": held write still pending\n");
}

static void show_satisfy(const char *label)
{
    struct SatisfyMsg *sm = NULL;
    int i;
    for (i = 0; i < 10; i++) {
        if ((sm = (struct SatisfyMsg *)GetMsg(sport)) != NULL)
            break;
        Delay(1);
    }
    probe_s(label);
    if (!sm) {
        probe_s(": none\n");
        return;
    }
    probe_s(": unit=");
    probe_dec(sm->sm_Unit);
    probe_s(" clip=");
    p_id(sm->sm_ClipID);
    probe_s(" mn_Length=");
    probe_dec(sm->sm_Msg.mn_Length);
    probe_s(" ln_Type=");
    probe_dec(sm->sm_Msg.mn_Node.ln_Type);
    probe_s(sm->sm_Msg.mn_ReplyPort ? " replyport=set\n" : " replyport=NULL\n");
}

/* --- change hook -------------------------------------------------------- */

#define MAX_CALLS 16
struct HookState {
    LONG calls;
    LONG type[MAX_CALLS];
    LONG cmd[MAX_CALLS];
    LONG id[MAX_CALLS];
    APTR obj[MAX_CALLS];
};

static ULONG hook_func(register struct Hook *hook __asm("a0"),
                       register APTR object __asm("a2"),
                       register struct ClipHookMsg *msg __asm("a1"))
{
    struct HookState *s = (struct HookState *)hook->h_Data;
    if (s->calls < MAX_CALLS) {
        s->type[s->calls] = msg->chm_Type;
        s->cmd[s->calls] = msg->chm_ChangeCmd;
        s->id[s->calls] = msg->chm_ClipID;
        s->obj[s->calls] = object;
    }
    s->calls++;
    return 0;
}

static struct Hook hook;
static struct HookState hs;

static void show_calls(const char *label)
{
    LONG i;
    Delay(2);
    probe_s(label);
    probe_s(": hook calls = ");
    probe_dec(hs.calls);
    probe_ch('\n');
    for (i = 0; i < hs.calls && i < MAX_CALLS; i++) {
        probe_s("    type=");
        probe_dec(hs.type[i]);
        probe_s(" cmd=");
        probe_dec(hs.cmd[i]);
        probe_s(" clip=");
        p_id(hs.id[i]);
        probe_s(hs.obj[i] == (APTR)jobs[0].req->io_Unit ? " object=io_Unit\n" : " object=other\n");
    }
    hs.calls = 0;
}

static void set_hook(LONG install)
{
    jobs[3].req->io_Data = (STRPTR)&hook;
    jobs[3].req->io_Length = install;
    run(3, CBD_CHANGEHOOK);
}

/* ------------------------------------------------------------------------ */

static void sec_units(void)
{
    static const LONG units[] = { 0, 1, 2, 9, 10, 99, 100, 254, 255, 256, 257, 1000,
                                  65535, 65536, 0x7fffffffL, -1 };
    struct IOClipReq *t, *t2;
    int i;

    P_SECTION("OpenDevice units");
    for (i = 0; i < (int)(sizeof(units) / sizeof(units[0])); i++) {
        BYTE err;
        t = (struct IOClipReq *)CreateIORequest(port, sizeof(struct IOClipReq));
        if (!t)
            return;
        err = OpenDevice((STRPTR)"clipboard.device", units[i], (struct IORequest *)t, 0);
        probe_s("unit ");
        probe_dec(units[i]);
        probe_s(": OpenDevice=");
        probe_dec(err);
        probe_s(" io_Error=");
        probe_dec(t->io_Error);
        if (err == 0) {
            probe_s(" cu_UnitNum=");
            probe_dec(t->io_Unit->cu_UnitNum);
            probe_s(" ln_Type=");
            probe_dec(t->io_Unit->cu_Node.ln_Type);
            CloseDevice((struct IORequest *)t);
        }
        probe_ch('\n');
        DeleteIORequest((struct IORequest *)t);
    }

    t = (struct IOClipReq *)CreateIORequest(port, sizeof(struct IOClipReq));
    t2 = (struct IOClipReq *)CreateIORequest(port, sizeof(struct IOClipReq));
    if (t && t2 && OpenDevice((STRPTR)"clipboard.device", 5, (struct IORequest *)t, 0) == 0) {
        if (OpenDevice((STRPTR)"clipboard.device", 5, (struct IORequest *)t2, 0) == 0) {
            P_BOOL("two opens of unit 5 share io_Unit", t->io_Unit == t2->io_Unit);
            CloseDevice((struct IORequest *)t2);
        }
        if (OpenDevice((STRPTR)"clipboard.device", 6, (struct IORequest *)t2, 0) == 0) {
            P_BOOL("unit 6 has its own io_Unit", t->io_Unit != t2->io_Unit);
            CloseDevice((struct IORequest *)t2);
        }
        CloseDevice((struct IORequest *)t);
    }
    if (t)
        DeleteIORequest((struct IORequest *)t);
    if (t2)
        DeleteIORequest((struct IORequest *)t2);
}

static void sec_fresh(void)
{
    /* never written by anyone: absolute IDs */
    begin("unit 200 (never written)", 200);
    base = 0;
    probe_s("absolute IDs: ");
    ids("unit 200", 3);
    fill();
    setup(1, 0, 0, buf, 10);
    run(1, CMD_READ);
    show("read", 1);
    run(1, CMD_READ);
    show("read again", 1);
    ids("unit 200", 3);
}

static void sec_independent(void)
{
    LONG b0, id;

    begin("units are independent", 1);
    b0 = base;
    begin("units are independent (unit 0 side)", 0);
    id = put(clipA, CLIPA_LEN);
    P_ID("unit 0 clip", id);
    begin("units are independent (unit 1 side)", 1);
    P_LONG("unit 1 CURRENTWRITEID unchanged", base == b0);
    fill();
    setup(1, 0, 0, buf, 100);
    run(1, CMD_READ);
    show("unit 1 read", 1);
    if (jobs[1].req->io_Actual)
        P_LONG("  finish reads", fin(1));
}

static void sec_write(void)
{
    LONG id;

    begin("CMD_WRITE / CMD_UPDATE", 0);
    ids("start", 3);
    setup(0, 0, 0, (APTR)clipA, 8);
    run(0, CMD_WRITE);
    show("write 8 (clip 0)", 0);
    id = jobs[0].req->io_ClipID;
    ids("  during write", 3);
    jobs[0].req->io_Data = (STRPTR)clipA + 8;
    jobs[0].req->io_Length = 4;
    run(0, CMD_WRITE);
    show("write 4 (continue)", 0);
    setup(0, id, 20, (APTR)(clipA + 20), 16);
    run(0, CMD_WRITE);
    show("write 16 at offset 20 (gap)", 0);
    setup(0, id, 12, (APTR)(clipA + 12), 4);
    run(0, CMD_WRITE);
    show("write 4 back at offset 12", 0);
    setup(0, id, 0, NULL, 0);
    run(0, CMD_WRITE);
    show("write 0 bytes", 0);
    ids("  during write", 3);
    jobs[0].req->io_ClipID = id + 3;
    run(0, CMD_UPDATE);
    show("update with a wrong clip ID", 0);
    ids("  after update", 3);
    fill();
    setup(1, 0, 0, buf, 100);
    run(1, CMD_READ);
    show("read", 1);
    P_BYTES("  data", buf, 40);
    P_LONG("  finish reads", fin(1));

    id = put(clipB, CLIPB_LEN);
    P_ID("next clip", id);
    show("  update", 0);
    ids("  after", 3);
    setup(0, id - 1, 0, (APTR)clipA, CLIPA_LEN);
    run(0, CMD_WRITE);
    show("write with an obsolete clip ID", 0);
    ids("  after", 3);
}

static void sec_read(void)
{
    LONG id;

    begin("CMD_READ", 0);
    id = put(clipA, CLIPA_LEN);
    fill();
    setup(1, 0, 0, buf, 4);
    run(1, CMD_READ);
    show("read 4", 1);
    P_BYTES("  data", buf, 6);
    jobs[1].req->io_Length = 0;
    run(1, CMD_READ);
    show("read 0", 1);
    fill();
    jobs[1].req->io_Length = 6;
    run(1, CMD_READ);
    show("read 6", 1);
    P_BYTES("  data", buf, 8);
    fill();
    jobs[1].req->io_Offset = 2;
    jobs[1].req->io_Length = 4;
    run(1, CMD_READ);
    show("read 4 at io_Offset 2", 1);
    P_BYTES("  data", buf, 6);
    jobs[1].req->io_Data = NULL;
    jobs[1].req->io_Length = 6;
    run(1, CMD_READ);
    show("read 6 with NULL io_Data", 1);
    fill();
    jobs[1].req->io_Data = (STRPTR)buf;
    jobs[1].req->io_Length = 100;
    run(1, CMD_READ);
    show("read 100", 1);
    P_BYTES("  data", buf, 26);
    fill();
    jobs[1].req->io_Length = 100;
    run(1, CMD_READ);
    show("read at the end", 1);
    P_BYTES("  data", buf, 2);
    run(1, CMD_READ);
    show("read again (clip -1)", 1);
    P_ID("CBD_CURRENTREADID", cur_id(3, CBD_CURRENTREADID));

    fill();
    setup(1, 0, 30, buf, 100);
    run(1, CMD_READ);
    show("new read at io_Offset 30", 1);
    P_BYTES("  data", buf, 8);
    P_LONG("  finish reads", fin(1));
    setup(1, 0, 36, buf, 10);
    run(1, CMD_READ);
    show("new read at io_Offset 36", 1);
    setup(1, 0, 1000, buf, 10);
    run(1, CMD_READ);
    show("new read at io_Offset 1000", 1);
    setup(1, 0, 0, NULL, 1000);
    run(1, CMD_READ);
    show("new read, NULL io_Data, length 1000", 1);
    P_LONG("  finish reads", fin(1));
    setup(1, -1, 0, buf, 10);
    run(1, CMD_READ);
    show("read with clip -1", 1);
    setup(1, id - 1, 0, buf, 10);
    run(1, CMD_READ);
    show("read with an obsolete clip ID", 1);
}

static void sec_sizes(void)
{
    static const char odd[] = "FORM\0\0\0\x05" "FTXTa";
    static const char claims_more[] = "FORM\0\0\0\x40" "FTXTCHRS\0\0\0\x04" "abcd";
    static const char claims_less[] = "FORM\0\0\0\x04" "FTXTCHRS\0\0\0\x04" "abcd";
    static const char raw[] = "ABCD\0\0\0\x03" "xyz";
    static const char cat[] = "CAT \0\0\0\x0c" "FTXTFORM\0\0\0\0";
    static const char zero[] = "FORM\0\0\0\0";
    static const char *const data[] = { odd, claims_more, claims_less, raw, cat, zero };
    static const LONG len[] = { 13, 24, 24, 11, 20, 8 };
    static const char *const name[] = { "FORM size 5", "FORM size 0x40, 24 bytes written",
                                        "FORM size 4, 24 bytes written", "ABCD size 3 (not IFF)",
                                        "CAT size 12", "FORM size 0" };
    int i;

    begin("clip sizes", 0);
    for (i = 0; i < (int)(sizeof(len) / sizeof(len[0])); i++) {
        put(data[i], len[i]);
        P_STR("clip", name[i]);
        fill();
        setup(1, 0, 0, buf, 100);
        run(1, CMD_READ);
        show("  read 100", 1);
        P_BYTES("  data", buf, jobs[1].req->io_Actual > 0 && jobs[1].req->io_Actual < 100 ?
                jobs[1].req->io_Actual + 2 : 26);
        if (jobs[1].req->io_Actual)
            P_LONG("  finish reads", fin(1));
        P_LONG("  final io_Offset", jobs[1].req->io_Offset);
    }
}

static void sec_holdoff(void)
{
    LONG id;

    begin("unfinished read holds off writes", 50);
    put(clipA, CLIPA_LEN);
    fill();
    setup(1, 0, 0, buf, 4);
    run(1, CMD_READ);
    show("read 4", 1);
    try_write("  then");
    ids("  while held", 3);
    jobs[1].req->io_Length = 32;
    run(1, CMD_READ);
    show("read 32 (up to the end)", 1);
    held("  then");
    run(1, CMD_READ);
    show("read at the end", 1);
    held("  then");
    ids("  after", 3);
    fill();
    setup(1, 0, 0, buf, 100);
    run(1, CMD_READ);
    show("new read", 1);
    P_BYTES("  data", buf, 30);
    P_LONG("  finish reads", fin(1));

    begin("two readers", 51);
    put(clipA, CLIPA_LEN);
    fill();
    setup(1, 0, 0, buf, 4);
    run(1, CMD_READ);
    show("reader 1 read 4", 1);
    try_write("  then");
    setup(2, 0, 0, buf + 64, 4);
    run(2, CMD_READ);
    show("reader 2 read 4", 2);
    P_BYTES("  data", buf + 64, 4);
    ids("  ids", 3);
    P_LONG("reader 1 finish reads", fin(1));
    held("  then");
    P_LONG("reader 2 finish reads", fin(2));
    held("  then");
    ids("  after", 3);

    begin("restarting a read counts as another reader", 52);
    put(clipA, CLIPA_LEN);
    setup(1, 0, 0, buf, 4);
    run(1, CMD_READ);
    show("read 4", 1);
    setup(1, 0, 0, buf, 4);
    run(1, CMD_READ);
    show("read 4 again with clip 0", 1);
    try_write("  then");
    P_LONG("finish reads", fin(1));
    held("  then");
    /* reading the current clip by its ID to the end ends a read too */
    setup(1, cur_id(3, CBD_CURRENTREADID), 0, NULL, 1000);
    run(1, CMD_READ);
    show("read by current ID", 1);
    run(1, CMD_READ);
    show("  at the end", 1);
    held("  then");

    begin("reads that do not hold off", 53);
    put(clipA, CLIPA_LEN);
    setup(1, cur_id(3, CBD_CURRENTREADID), 0, buf, 4);
    run(1, CMD_READ);
    show("read 4 by current ID", 1);
    try_write("  then");
    run(1, CMD_READ);
    show("  read on (clip replaced)", 1);
    id = put(clipA, CLIPA_LEN);
    setup(1, id - 1, 0, buf, 4);
    run(1, CMD_READ);
    show("read with an obsolete ID", 1);
    try_write("  then");
    setup(1, 0, 100, buf, 4);
    run(1, CMD_READ);
    show("read starting past the end", 1);
    try_write("  then");
    setup(1, 0, 0, buf, 0);
    run(1, CMD_READ);
    show("read of 0 bytes", 1);
    try_write("  then");
    P_LONG("finish reads", fin(1));
    held("  then");

    begin("ending a read that was not counted", 54);
    put(clipA, CLIPA_LEN);
    setup(1, cur_id(3, CBD_CURRENTREADID), 0, NULL, 1000);
    run(1, CMD_READ);
    show("read by current ID", 1);
    run(1, CMD_READ);
    show("  at the end", 1);
    try_write("  then");
    setup(1, 0, 0, buf, 4);
    run(1, CMD_READ);
    show("new read with clip 0", 1);
    held("  then");
    ids("  after", 3);
}

static void sec_write_vs_read(void)
{
    LONG id;

    begin("write in progress holds off reads", 55);
    put(clipA, CLIPA_LEN);
    setup(0, 0, 0, (APTR)clipB, CLIPB_LEN);
    run(0, CMD_WRITE);
    show("write (no update yet)", 0);
    ids("  ids", 3);
    fill();
    setup(1, 0, 0, buf, 100);
    start(1, CMD_READ);
    P_BOOL("read done", wait_done(1, 15));
    run(0, CMD_UPDATE);
    show("update", 0);
    P_BOOL("read done", wait_done(1, 15));
    show("  read", 1);
    P_BYTES("  data", buf, 30);
    if (!jobs[1].busy)
        P_LONG("  finish reads", fin(1));

    begin("write with a chosen clip ID", 56);
    put(clipA, CLIPA_LEN);
    setup(0, base + 3, 0, (APTR)clipB, CLIPB_LEN);
    run(0, CMD_WRITE);
    show("write with clip base+3", 0);
    ids("  ids", 3);
    setup(1, 0, 0, buf, 100);
    start(1, CMD_READ);
    P_BOOL("read done", wait_done(1, 15));
    show("  read", 1);
    run(0, CMD_UPDATE);
    show("update", 0);
    P_BOOL("read done", wait_done(1, 15));
    show("  read", 1);
    ids("  ids", 3);
    setup(1, 0, 0, buf, 100);
    run(1, CMD_READ);
    show("read with clip 0", 1);
    /* not read to the end: that would end a read that was never counted */
    setup(1, base + 3, 0, buf, 100);
    run(1, CMD_READ);
    show("read with clip base+3", 1);
    for (id = 0; id < 4; id++) {
        setup(0, 0, 0, (APTR)clipA, CLIPA_LEN);
        run(0, CMD_WRITE);
        show("write with clip 0", 0);
        if (jobs[0].req->io_Error == 0) {
            run(0, CMD_UPDATE);
            show("  update", 0);
            break;
        }
    }
    ids("  ids", 3);
}

static void sec_post(void)
{
    LONG id, a;

    hs.calls = 0;
    hook.h_Entry = (ULONG (*)())hook_func;
    hook.h_SubEntry = NULL;
    hook.h_Data = &hs;

    begin("CBD_CHANGEHOOK and CBD_POST", 60);
    set_hook(1);
    P_LONG("install hook io_Error", jobs[3].req->io_Error);
    setup(0, 0, 0, (APTR)clipA, CLIPA_LEN);
    run(0, CMD_WRITE);
    show_calls("after write");
    run(0, CMD_UPDATE);
    show_calls("after update");
    setup(1, 0, 0, buf, 100);
    run(1, CMD_READ);
    fin(1);
    show_calls("after read");

    setup(0, 0, 0, (APTR)sport, 0);
    run(0, CBD_POST);
    show("post", 0);
    id = jobs[0].req->io_ClipID;
    show_calls("after post");
    ids("  ids", 3);
    show_satisfy("satisfy before read");
    fill();
    setup(1, 0, 0, buf, 100);
    start(1, CMD_READ);
    P_BOOL("read of the post done", wait_done(1, 15));
    show_satisfy("satisfy");
    show_calls("after read");
    setup(0, id, 0, (APTR)clipB, CLIPB_LEN);
    run(0, CMD_WRITE);
    show("satisfying write", 0);
    ids("  ids", 3);
    show_calls("after satisfying write");
    P_BOOL("read done", wait_done(1, 15));
    run(0, CMD_UPDATE);
    show("satisfying update", 0);
    show_calls("after satisfying update");
    P_BOOL("read done", wait_done(1, 15));
    show("  read", 1);
    P_BYTES("  data", buf, 30);
    if (!jobs[1].busy)
        P_LONG("  finish reads", fin(1));
    show_satisfy("satisfy left");
    set_hook(0);
    P_LONG("remove hook io_Error", jobs[3].req->io_Error);
    put(clipA, CLIPA_LEN);
    show_calls("after clip (hook removed)");

    begin("post superseded by a clip", 61);
    setup(0, 0, 0, (APTR)sport, 0);
    run(0, CBD_POST);
    show("post", 0);
    id = jobs[0].req->io_ClipID;
    a = put(clipA, CLIPA_LEN);
    P_ID("clip", a);
    ids("  ids", 3);
    setup(1, 0, 0, buf, 100);
    run(1, CMD_READ);
    show("read", 1);
    P_LONG("  finish reads", fin(1));
    show_satisfy("satisfy");
    setup(0, id, 0, (APTR)clipB, CLIPB_LEN);
    run(0, CMD_WRITE);
    show("write with the superseded post ID", 0);

    begin("two posts", 62);
    setup(0, 0, 0, (APTR)sport, 0);
    run(0, CBD_POST);
    show("post 1", 0);
    setup(0, 0, 0, (APTR)sport, 0);
    run(0, CBD_POST);
    show("post 2", 0);
    id = jobs[0].req->io_ClipID;
    ids("  ids", 3);
    setup(1, 0, 0, buf, 100);
    start(1, CMD_READ);
    P_BOOL("read done", wait_done(1, 15));
    show_satisfy("satisfy");
    show_satisfy("satisfy");
    setup(0, id, 0, (APTR)clipB, CLIPB_LEN);
    run(0, CMD_WRITE);
    run(0, CMD_UPDATE);
    show("satisfy post 2", 0);
    P_BOOL("read done", wait_done(1, 15));
    show("  read", 1);
    if (!jobs[1].busy)
        P_LONG("  finish reads", fin(1));

    begin("unfinished read holds off a post", 63);
    put(clipA, CLIPA_LEN);
    setup(2, 0, 0, buf + 64, 4);
    run(2, CMD_READ);
    show("reader read 4", 2);
    setup(0, 0, 0, (APTR)sport, 0);
    start(0, CBD_POST);
    P_BOOL("post done", wait_done(0, 15));
    ids("  ids", 3);
    P_LONG("reader finish reads", fin(2));
    P_BOOL("post done", wait_done(0, 15));
    show("  post", 0);
    ids("  ids", 3);
    a = put(clipA, CLIPA_LEN);
    P_ID("clip superseding the post", a);
    show_satisfy("satisfy");

    begin("aborting a read of a post", 64);
    put(clipA, CLIPA_LEN);
    setup(0, 0, 0, (APTR)sport, 0);
    run(0, CBD_POST);
    show("post", 0);
    id = jobs[0].req->io_ClipID;
    setup(1, 0, 0, buf, 100);
    start(1, CMD_READ);
    P_BOOL("read done", wait_done(1, 15));
    show_satisfy("satisfy");
    if (jobs[1].busy) {
        AbortIO((struct IORequest *)jobs[1].req);
        P_BOOL("read done after AbortIO", wait_done(1, 15));
        show("  read", 1);
    }
    setup(0, id, 0, (APTR)clipB, CLIPB_LEN);
    run(0, CMD_WRITE);
    run(0, CMD_UPDATE);
    show("satisfy the post", 0);
    if (!jobs[1].busy && jobs[1].req->io_Error == 0 && jobs[1].req->io_Actual)
        P_LONG("  finish reads", fin(1));
}

static void sec_commands(void)
{
    /* CMD_RESET is left out: AmigaOS 3.1 never replies to it */
    static const UWORD cmds[] = { CMD_INVALID, CMD_CLEAR, CMD_STOP, CMD_START, CMD_FLUSH,
                                  13, 14, 15, 16, 100, 0x7fff, 0xffff };
    struct IOClipReq *q;
    int i;

    begin("other commands", 0);
    q = jobs[3].req;
    for (i = 0; i < (int)(sizeof(cmds) / sizeof(cmds[0])); i++) {
        q->io_Actual = 0x1234;
        q->io_ClipID = 0x777;
        q->io_Offset = 0x55;
        probe_s("command ");
        probe_dec(cmds[i]);
        probe_ch('\n');
        if (run(3, cmds[i])) {
            probe_s("  err=");
            probe_dec(q->io_Error);
            probe_s(" actual=");
            probe_hex(q->io_Actual, 4);
            probe_s(" clip=");
            probe_hex(q->io_ClipID, 4);
            probe_s(" offset=");
            probe_hex(q->io_Offset, 4);
            probe_ch('\n');
        }
    }
    q->io_Command = CBD_CURRENTREADID;
    DoIO((struct IORequest *)q);
    P_HEX("DoIO(CBD_CURRENTREADID) io_Flags & IOF_QUICK", q->io_Flags & IOF_QUICK);
    q->io_Command = CBD_CURRENTWRITEID;
    q->io_Actual = 0x1234;
    DoIO((struct IORequest *)q);
    P_HEX("DoIO(CBD_CURRENTWRITEID) io_Actual", q->io_Actual);
    q->io_Command = CMD_INVALID;
    DoIO((struct IORequest *)q);
    P_HEX("DoIO(CMD_INVALID) io_Flags & IOF_QUICK", q->io_Flags & IOF_QUICK);
    P_LONG("DoIO(CMD_INVALID) io_Error", q->io_Error);
}

int main(void)
{
    int i;

    port = CreateMsgPort();
    sport = CreateMsgPort();
    if (!port || !sport)
        return 20;

    sec_units();
    sec_fresh();
    sec_independent();
    sec_write();
    sec_read();
    sec_sizes();
    sec_holdoff();
    sec_write_vs_read();
    sec_post();
    sec_commands();

    P_SECTION("done");
    for (i = 0; i < NJOBS; i++) {
        if (jobs[i].req && jobs[i].busy) {
            probe_s("slot still busy: ");
            probe_dec(i);
            probe_ch('\n');
        }
        close_job(i);
    }
    DeleteMsgPort(sport);
    DeleteMsgPort(port);
    return 0;
}
