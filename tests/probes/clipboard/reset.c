/*
 * Probe (Phase 222g): clipboard.device CMD_RESET.  On AmigaOS 3.1 BeginIO()
 * returns at once but the request is never replied (not even after
 * AbortIO() or later traffic on the unit), so DoIO(CMD_RESET) hangs.  The
 * probe issues it with BeginIO() and only looks at the request; it is
 * abandoned (never closed) on a unit nobody else uses.
 */
#include "clipasync.h"

#define RESET_UNIT 201

static void st(const char *label, struct IOClipReq *r)
{
    probe_s(label);
    probe_s(": io_Flags=");
    probe_hex(r->io_Flags, 2);
    probe_s(" io_Error=");
    probe_dec(r->io_Error);
    probe_s(" ln_Type=");
    probe_dec(r->io_Message.mn_Node.ln_Type);
    probe_s(" CheckIO=");
    probe_s(CheckIO((struct IORequest *)r) ? "done" : "pending");
    probe_ch('\n');
}

int main(void)
{
    static const char clip[] = "FORM\0\0\0\x14" "FTXTCHRS\0\0\0\x08" "Clip two";
    struct MsgPort *port = CreateMsgPort();
    struct IOClipReq *r;

    if (!port)
        return 20;
    r = (struct IOClipReq *)CreateIORequest(port, sizeof(struct IOClipReq));
    if (!r || OpenDevice((STRPTR)"clipboard.device", RESET_UNIT, (struct IORequest *)r, 0))
        return 20;
    if (!open_job(1, port, RESET_UNIT))
        return 20;
    base = cur_id(1, CBD_CURRENTWRITEID);

    P_SECTION("CMD_RESET");
    r->io_Command = CMD_RESET;
    r->io_Flags = IOF_QUICK;
    r->io_Error = 0x55;
    r->io_Actual = 0x1234;
    r->io_ClipID = 0x777;
    r->io_Offset = 0x55;
    BeginIO((struct IORequest *)r);
    st("after BeginIO (IOF_QUICK)", r);
    P_HEX("io_Actual", r->io_Actual);
    P_HEX("io_ClipID", r->io_ClipID);
    P_HEX("io_Offset", r->io_Offset);
    Delay(25);
    st("0.5 s later", r);
    P_BOOL("reply port empty", port->mp_MsgList.lh_Head->ln_Succ == NULL);

    ids("ids on the same unit", 1);
    setup(1, 0, 0, (APTR)clip, 28);
    P_BOOL("write", run(1, CMD_WRITE));
    P_BOOL("update", run(1, CMD_UPDATE));
    show("  write", 1);
    Delay(5);
    st("after a write", r);

    AbortIO((struct IORequest *)r);
    Delay(5);
    st("after AbortIO", r);
    P_BOOL("reply port empty", port->mp_MsgList.lh_Head->ln_Succ == NULL);

    close_job(1);
    /* r is abandoned: CloseDevice() with the request outstanding is not
     * probed */
    return 0;
}
