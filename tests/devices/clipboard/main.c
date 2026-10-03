/*
 * Test for clipboard.device (Phase 91 coverage, validated against
 * AmigaOS 3.1 in Phase 220).
 *
 * Clip IDs are printed relative to the CBD_CURRENTWRITEID value seen at
 * start-up, so the output does not depend on earlier clipboard users.
 * Clips are well-formed IFF FORMs: the 3.1 clipboard.device derives the
 * clip size from the FORM header, and a read only ends (io_Actual 0,
 * io_ClipID -1) once it is past that size.  An unfinished read holds off
 * every later write, so each read here is run to its end.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/execbase.h>
#include <exec/errors.h>
#include <exec/libraries.h>
#include <devices/clipboard.h>
#include <utility/hooks.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static int failures = 0;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG num)
{
    char buf[16];
    int i = 0;

    if (num < 0)
    {
        buf[i++] = '-';
        num = -num;
    }

    if (num == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        char temp[16];
        int j = 0;

        while (num > 0)
        {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }

        while (j > 0)
            buf[i++] = temp[--j];
    }

    buf[i] = '\0';
    print(buf);
}

static void check(BOOL ok, const char *what)
{
    print(ok ? "OK: " : "FAIL: ");
    print(what);
    print("\n");
    if (!ok)
        failures++;
}

static void show(const char *what, LONG v)
{
    print("  ");
    print(what);
    print(" = ");
    print_num(v);
    print("\n");
}

static BOOL mem_equal(const char *s1, const char *s2, ULONG len)
{
    ULONG i;

    for (i = 0; i < len; i++)
    {
        if (s1[i] != s2[i])
            return FALSE;
    }

    return TRUE;
}

#define MAX_HOOK_CALLS 8

struct HookState
{
    ULONG calls;
    LONG cmd[MAX_HOOK_CALLS];
    LONG clip_id[MAX_HOOK_CALLS];
    APTR object[MAX_HOOK_CALLS];
};

static ULONG clipboard_hook(register struct Hook *hook __asm("a0"),
                            register APTR object __asm("a2"),
                            register struct ClipHookMsg *msg __asm("a1"))
{
    struct HookState *state = (struct HookState *)hook->h_Data;

    if (state && msg)
    {
        if (state->calls < MAX_HOOK_CALLS)
        {
            state->cmd[state->calls] = msg->chm_ChangeCmd;
            state->clip_id[state->calls] = msg->chm_ClipID;
            state->object[state->calls] = object;
        }
        state->calls++;
    }

    return 0;
}

static void show_hook_calls(struct HookState *state, LONG base, APTR unit)
{
    ULONG i;

    show("change hook calls", (LONG)state->calls);
    for (i = 0; i < state->calls && i < MAX_HOOK_CALLS; i++)
    {
        print("  hook call: cmd=");
        print_num(state->cmd[i]);
        print(" clip=base+");
        print_num(state->clip_id[i] - base);
        print(state->object[i] == unit ? " object=io_Unit\n" : " object=other\n");
    }
    state->calls = 0;
}

static struct IOClipReq *create_clip_req(struct MsgPort *port)
{
    return (struct IOClipReq *)CreateIORequest(port, sizeof(struct IOClipReq));
}

static LONG do_cmd(struct IOClipReq *req, UWORD cmd)
{
    req->io_Command = cmd;
    req->io_Error = 0;
    return DoIO((struct IORequest *)req);
}

static LONG current_id(struct IOClipReq *req, UWORD cmd)
{
    req->io_ClipID = 0;
    do_cmd(req, cmd);
    return req->io_ClipID;
}

/* write a whole clip (clip_id 0 = new clip) and commit it */
static LONG write_clip(struct IOClipReq *req, const char *data, LONG len, LONG clip_id)
{
    req->io_Data = (STRPTR)data;
    req->io_Length = len;
    req->io_Offset = 0;
    req->io_ClipID = clip_id;
    if (do_cmd(req, CMD_WRITE) != 0 || req->io_Actual != len || req->io_Offset != len)
        return -1;
    if (do_cmd(req, CMD_UPDATE) != 0)
        return -1;
    return req->io_ClipID;
}

/* read past the end of the current read clip so the clipboard is released */
static BOOL finish_read(struct IOClipReq *req, char *buf, LONG size)
{
    req->io_Data = (STRPTR)buf;
    req->io_Length = size;
    do_cmd(req, CMD_READ);
    return req->io_Error == 0 && req->io_Actual == 0 && req->io_ClipID == -1;
}

/* FORM FTXT with one CHRS chunk */
static const char clip1[] = "FORM\0\0\0\x1c" "FTXTCHRS\0\0\0\x10" "Hello Clipboard!";
static const char clip2[] = "FORM\0\0\0\x24" "FTXTCHRS\0\0\0\x17" "Deferred clipboard data\0";
static const char clip3[] = "FORM\0\0\0\x24" "FTXTCHRS\0\0\0\x18" "Final clipboard contents";
#define CLIP1_LEN 36
#define CLIP2_LEN 44
#define CLIP3_LEN 44

int main(void)
{
    struct MsgPort *reply_port;
    struct MsgPort *satisfy_port;
    struct MsgPort *abort_port;
    struct IOClipReq *clip_req;
    struct IOClipReq *writer_req;
    struct IOClipReq *read_req;
    struct IOClipReq *reopen_req;
    struct Device *device;
    struct SatisfyMsg *satisfy_msg;
    struct Hook hook;
    struct HookState hook_state;
    LONG base;
    LONG clip_id;
    LONG post_id;
    LONG error;
    char read_data[64];

    print("Testing clipboard.device\n");

    reply_port = CreateMsgPort();
    satisfy_port = CreateMsgPort();
    abort_port = CreateMsgPort();
    clip_req = create_clip_req(reply_port);
    writer_req = create_clip_req(reply_port);
    read_req = create_clip_req(reply_port);
    reopen_req = create_clip_req(reply_port);
    if (!reply_port || !satisfy_port || !abort_port ||
        !clip_req || !writer_req || !read_req || !reopen_req)
    {
        print("FAIL: Cannot allocate ports/requests\n");
        return 20;
    }

    if (OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)clip_req, 0) != 0 ||
        OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)writer_req, 0) != 0 ||
        OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)read_req, 0) != 0)
    {
        print("FAIL: Cannot open clipboard.device\n");
        return 20;
    }
    print("OK: clipboard.device opened\n");
    device = clip_req->io_Device;

    /* --- current IDs and an immediate clip --------------------------- */
    base = current_id(clip_req, CBD_CURRENTWRITEID);
    check(clip_req->io_Error == 0 && base > 0, "CBD_CURRENTWRITEID returns a positive clip ID");
    check(current_id(clip_req, CBD_CURRENTREADID) == base, "CBD_CURRENTREADID equals CBD_CURRENTWRITEID");

    writer_req->io_Data = (STRPTR)clip1;
    writer_req->io_Length = CLIP1_LEN;
    writer_req->io_Offset = 0;
    writer_req->io_ClipID = 0;
    do_cmd(writer_req, CMD_WRITE);
    clip_id = writer_req->io_ClipID;
    check(writer_req->io_Error == 0 && writer_req->io_Actual == CLIP1_LEN &&
          writer_req->io_Offset == CLIP1_LEN, "CMD_WRITE writes the clip");
    show("new clip ID - base", clip_id - base);

    do_cmd(writer_req, CMD_UPDATE);
    check(writer_req->io_Error == 0 && writer_req->io_ClipID == clip_id, "CMD_UPDATE commits the clip");
    check(current_id(clip_req, CBD_CURRENTREADID) == clip_id, "CBD_CURRENTREADID tracks the committed clip");
    check(current_id(clip_req, CBD_CURRENTWRITEID) == clip_id, "CBD_CURRENTWRITEID tracks the committed clip");

    read_req->io_Data = (STRPTR)read_data;
    read_req->io_Length = sizeof(read_data);
    read_req->io_Offset = 0;
    read_req->io_ClipID = 0;
    do_cmd(read_req, CMD_READ);
    check(read_req->io_Error == 0 && read_req->io_ClipID == clip_id &&
          read_req->io_Actual == CLIP1_LEN && read_req->io_Offset == CLIP1_LEN &&
          mem_equal(read_data, clip1, CLIP1_LEN), "CMD_READ returns the clip up to its FORM size");
    check(finish_read(read_req, read_data, sizeof(read_data)) && read_req->io_Offset == CLIP1_LEN,
          "CMD_READ past the end returns 0 bytes and ClipID -1");

    read_req->io_Data = (STRPTR)read_data;
    read_req->io_Length = 10;
    read_req->io_Offset = 0;
    read_req->io_ClipID = 0;
    do_cmd(read_req, CMD_READ);
    check(read_req->io_Actual == 10 && read_req->io_Offset == 10 && mem_equal(read_data, clip1, 10),
          "CMD_READ reads sequential parts");
    read_req->io_Data = NULL;
    read_req->io_Length = 1000;
    do_cmd(read_req, CMD_READ);
    check(read_req->io_Error == 0 && read_req->io_Actual == CLIP1_LEN - 10 && read_req->io_Offset == CLIP1_LEN,
          "CMD_READ with NULL io_Data skips to the end");
    check(finish_read(read_req, read_data, sizeof(read_data)), "skipped read ends at the clip end");

    /* --- change hook and a posted clip ------------------------------- */
    hook_state.calls = 0;
    hook.h_Entry = (ULONG (*)())clipboard_hook;
    hook.h_SubEntry = NULL;
    hook.h_Data = &hook_state;
    clip_req->io_Data = (STRPTR)&hook;
    clip_req->io_Length = 1;
    do_cmd(clip_req, CBD_CHANGEHOOK);
    check(clip_req->io_Error == 0, "CBD_CHANGEHOOK installs hook");

    writer_req->io_Data = (STRPTR)satisfy_port;
    writer_req->io_ClipID = 0;
    do_cmd(writer_req, CBD_POST);
    post_id = writer_req->io_ClipID;
    check(writer_req->io_Error == 0 && post_id == clip_id + 1, "CBD_POST assigns the next clip ID");
    show_hook_calls(&hook_state, base, writer_req->io_Unit);
    check(current_id(clip_req, CBD_CURRENTREADID) == post_id, "CBD_CURRENTREADID tracks the post");
    check(current_id(clip_req, CBD_CURRENTWRITEID) == post_id, "CBD_CURRENTWRITEID tracks the post");

    read_req->io_Command = CMD_READ;
    read_req->io_Error = 0;
    read_req->io_Data = (STRPTR)read_data;
    read_req->io_Length = sizeof(read_data);
    read_req->io_Offset = 0;
    read_req->io_ClipID = 0;
    SendIO((struct IORequest *)read_req);
    check(CheckIO((struct IORequest *)read_req) == NULL, "CMD_READ of a posted clip pends");

    satisfy_msg = (struct SatisfyMsg *)GetMsg(satisfy_port);
    check(satisfy_msg != NULL && satisfy_msg->sm_Unit == PRIMARY_CLIP && satisfy_msg->sm_ClipID == post_id,
          "CBD_POST sends a SatisfyMsg on demand");
    show_hook_calls(&hook_state, base, writer_req->io_Unit);

    writer_req->io_Data = (STRPTR)clip2;
    writer_req->io_Length = 8;
    writer_req->io_Offset = 0;
    writer_req->io_ClipID = post_id;
    do_cmd(writer_req, CMD_WRITE);
    check(writer_req->io_Error == 0 && writer_req->io_ClipID == post_id, "satisfying CMD_WRITE starts");
    show_hook_calls(&hook_state, base, writer_req->io_Unit);

    check(write_clip(writer_req, clip2, CLIP2_LEN, post_id) == post_id, "satisfying write uses the post ID");
    error = WaitIO((struct IORequest *)read_req);
    check(error == 0 && read_req->io_ClipID == post_id && read_req->io_Actual == CLIP2_LEN &&
          mem_equal(read_data, clip2, CLIP2_LEN), "pending CMD_READ completes with the posted data");
    check(finish_read(read_req, read_data, sizeof(read_data)), "posted clip read ends at the clip end");
    show_hook_calls(&hook_state, base, writer_req->io_Unit);

    clip_req->io_Data = (STRPTR)&hook;
    clip_req->io_Length = 0;
    do_cmd(clip_req, CBD_CHANGEHOOK);
    check(clip_req->io_Error == 0, "CBD_CHANGEHOOK removes hook");
    clip_id = write_clip(writer_req, clip3, CLIP3_LEN, 0);
    check(clip_id == post_id + 1 && hook_state.calls == 0, "removed change hook is not called");

    /* --- aborting a read that waits for a post ----------------------- */
    writer_req->io_Data = (STRPTR)abort_port;
    writer_req->io_ClipID = 0;
    do_cmd(writer_req, CBD_POST);
    post_id = writer_req->io_ClipID;
    check(writer_req->io_Error == 0 && post_id == clip_id + 1, "second CBD_POST assigns the next clip ID");

    read_req->io_Command = CMD_READ;
    read_req->io_Error = 0;
    read_req->io_Data = (STRPTR)read_data;
    read_req->io_Length = sizeof(read_data);
    read_req->io_Offset = 0;
    read_req->io_ClipID = 0;
    SendIO((struct IORequest *)read_req);
    check(CheckIO((struct IORequest *)read_req) == NULL, "CMD_READ of the second post pends");
    satisfy_msg = (struct SatisfyMsg *)GetMsg(abort_port);
    check(satisfy_msg != NULL && satisfy_msg->sm_ClipID == post_id, "second post sends a SatisfyMsg");

    AbortIO((struct IORequest *)read_req);
    error = WaitIO((struct IORequest *)read_req);
    check(error == IOERR_ABORTED && read_req->io_Error == IOERR_ABORTED, "AbortIO aborts a pending CMD_READ");

    clip_id = write_clip(writer_req, clip3, CLIP3_LEN, post_id);
    check(clip_id == post_id, "the aborted post can still be satisfied");

    /* --- Expunge while open ------------------------------------------ */
    RemDevice(device);
    check((device->dd_Library.lib_Flags & LIBF_DELEXP) != 0 &&
          FindName(&SysBase->DeviceList, (STRPTR)"clipboard.device") == &device->dd_Library.lib_Node,
          "RemDevice() while open sets LIBF_DELEXP and keeps the device");

    error = OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)reopen_req, 0);
    check(error == 0 && reopen_req->io_Device == device &&
          (device->dd_Library.lib_Flags & LIBF_DELEXP) == 0 &&
          device->dd_Library.lib_OpenCnt == 4, "OpenDevice() after deferred expunge clears LIBF_DELEXP");
    if (error == 0)
        CloseDevice((struct IORequest *)reopen_req);

    RemDevice(device);
    CloseDevice((struct IORequest *)read_req);
    CloseDevice((struct IORequest *)writer_req);
    CloseDevice((struct IORequest *)clip_req);
    check(device->dd_Library.lib_OpenCnt == 0 &&
          FindName(&SysBase->DeviceList, (STRPTR)"clipboard.device") == &device->dd_Library.lib_Node,
          "closing the last opener does not expunge clipboard.device");

    DeleteIORequest((struct IORequest *)reopen_req);
    DeleteIORequest((struct IORequest *)read_req);
    DeleteIORequest((struct IORequest *)writer_req);
    DeleteIORequest((struct IORequest *)clip_req);
    DeleteMsgPort(abort_port);
    DeleteMsgPort(satisfy_port);
    DeleteMsgPort(reply_port);

    if (failures)
    {
        print("FAIL: clipboard.device test had failures\n");
        return 1;
    }
    print("PASS: clipboard.device test complete\n");
    return 0;
}
