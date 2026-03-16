/*
 * Test for clipboard.device Phase 91 coverage.
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

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static void print_num(ULONG num)
{
    char buf[16];
    int i = 0;

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

static BOOL str_equal(const char *s1, const char *s2, ULONG len)
{
    ULONG i;

    for (i = 0; i < len; i++)
    {
        if (s1[i] != s2[i])
            return FALSE;
    }

    return TRUE;
}

struct HookState
{
    ULONG calls;
    LONG last_cmd;
    LONG last_clip_id;
    APTR last_object;
};

static ULONG clipboard_hook(register struct Hook *hook __asm("a0"),
                            register APTR object __asm("a2"),
                            register struct ClipHookMsg *msg __asm("a1"))
{
    struct HookState *state = (struct HookState *)hook->h_Data;

    if (state && msg)
    {
        state->calls++;
        state->last_cmd = msg->chm_ChangeCmd;
        state->last_clip_id = msg->chm_ClipID;
        state->last_object = object;
    }

    return 0;
}

static void close_if_open(struct IOClipReq *req)
{
    if (req && req->io_Device && req->io_Unit)
    {
        CloseDevice((struct IORequest *)req);
    }
}

static void reset_hook_state(struct HookState *state)
{
    state->calls = 0;
    state->last_cmd = -1;
    state->last_clip_id = -1;
    state->last_object = NULL;
}

static struct IOClipReq *create_clip_req(struct MsgPort *port)
{
    struct IOClipReq *req;

    req = (struct IOClipReq *)CreateIORequest(port, sizeof(struct IOClipReq));
    if (req)
    {
        req->io_Message.mn_Length = sizeof(struct IOClipReq);
    }

    return req;
}

static void delete_clip_req(struct IOClipReq *req)
{
    if (req)
        DeleteIORequest((struct IORequest *)req);
}

int main(void)
{
    struct MsgPort *reply_port = NULL;
    struct MsgPort *satisfy_port = NULL;
    struct MsgPort *abort_port = NULL;
    struct IOClipReq *clip_req = NULL;
    struct IOClipReq *writer_req = NULL;
    struct IOClipReq *read_req = NULL;
    struct IOClipReq *reopen_req = NULL;
    struct Device *device;
    struct Node *device_node;
    struct SatisfyMsg *satisfy_msg;
    struct SatisfyMsg *abort_msg;
    struct Hook hook;
    struct HookState hook_state;
    LONG error;
    LONG clip_id;
    LONG post_id;
    LONG abort_post_id;
    char write_data[] = "Hello Clipboard!";
    char post_data[] = "Deferred clipboard data";
    char final_data[] = "Final clipboard contents";
    char read_data[64];

    print("Testing clipboard.device\n");

    reply_port = CreateMsgPort();
    satisfy_port = CreateMsgPort();
    abort_port = CreateMsgPort();
    if (!reply_port || !satisfy_port || !abort_port)
    {
        print("FAIL: Cannot allocate message ports\n");
        goto fail;
    }
    print("OK: Message ports created\n");

    clip_req = create_clip_req(reply_port);
    writer_req = create_clip_req(reply_port);
    read_req = create_clip_req(reply_port);
    reopen_req = create_clip_req(reply_port);
    if (!clip_req || !writer_req || !read_req || !reopen_req)
    {
        print("FAIL: Cannot allocate IO requests\n");
        goto fail;
    }
    print("OK: IO requests created\n");

    error = OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)clip_req, 0);
    if (error != 0)
    {
        print("FAIL: Cannot open clipboard.device, error=");
        print_num((ULONG)error);
        print("\n");
        goto fail;
    }

    error = OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)writer_req, 0);
    if (error != 0)
    {
        print("FAIL: Cannot open writer clipboard request, error=");
        print_num((ULONG)error);
        print("\n");
        goto fail;
    }

    error = OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)read_req, 0);
    if (error != 0)
    {
        print("FAIL: Cannot open read clipboard request, error=");
        print_num((ULONG)error);
        print("\n");
        goto fail;
    }
    print("OK: clipboard.device opened\n");

    clip_req->io_Command = CBD_CURRENTWRITEID;
    clip_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)clip_req);
    if (clip_req->io_Error != 0 || clip_req->io_ClipID != 0)
    {
        print("FAIL: CBD_CURRENTWRITEID initial state incorrect\n");
        goto fail;
    }
    print("OK: CBD_CURRENTWRITEID reports empty clipboard\n");

    writer_req->io_Command = CMD_WRITE;
    writer_req->io_Flags = IOF_QUICK;
    writer_req->io_Data = (STRPTR)write_data;
    writer_req->io_Length = sizeof(write_data) - 1;
    writer_req->io_Offset = 0;
    writer_req->io_ClipID = 0;
    DoIO((struct IORequest *)writer_req);
    if (writer_req->io_Error != 0 || writer_req->io_Actual != sizeof(write_data) - 1)
    {
        print("FAIL: CMD_WRITE failed for immediate clip\n");
        goto fail;
    }
    clip_id = writer_req->io_ClipID;
    print("OK: CMD_WRITE staged clip ID=");
    print_num((ULONG)clip_id);
    print("\n");

    writer_req->io_Command = CMD_UPDATE;
    writer_req->io_Flags = IOF_QUICK;
    writer_req->io_ClipID = clip_id;
    DoIO((struct IORequest *)writer_req);
    if (writer_req->io_Error != 0 || writer_req->io_ClipID != clip_id)
    {
        print("FAIL: CMD_UPDATE failed for immediate clip\n");
        goto fail;
    }
    print("OK: CMD_UPDATE committed immediate clip\n");

    clip_req->io_Command = CBD_CURRENTREADID;
    clip_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)clip_req);
    if (clip_req->io_Error != 0 || clip_req->io_ClipID != clip_id)
    {
        print("FAIL: CBD_CURRENTREADID did not match committed clip\n");
        goto fail;
    }
    print("OK: CBD_CURRENTREADID matches committed clip\n");

    read_req->io_Command = CMD_READ;
    read_req->io_Flags = IOF_QUICK;
    read_req->io_Data = (STRPTR)read_data;
    read_req->io_Length = sizeof(read_data);
    read_req->io_Offset = 0;
    read_req->io_ClipID = clip_id;
    DoIO((struct IORequest *)read_req);
    if (read_req->io_Error != 0 ||
        read_req->io_Actual != sizeof(write_data) - 1 ||
        !str_equal(read_data, write_data, read_req->io_Actual))
    {
        print("FAIL: CMD_READ immediate clip mismatch\n");
        goto fail;
    }
    print("OK: CMD_READ returns committed data\n");

    reset_hook_state(&hook_state);
    hook.h_Entry = (ULONG (*)())clipboard_hook;
    hook.h_SubEntry = NULL;
    hook.h_Data = &hook_state;
    clip_req->io_Command = CBD_CHANGEHOOK;
    clip_req->io_Flags = IOF_QUICK;
    clip_req->io_Data = (STRPTR)&hook;
    clip_req->io_Length = 1;
    DoIO((struct IORequest *)clip_req);
    if (clip_req->io_Error != 0)
    {
        print("FAIL: CBD_CHANGEHOOK install failed\n");
        goto fail;
    }
    print("OK: CBD_CHANGEHOOK installs hook\n");

    writer_req->io_Command = CBD_POST;
    writer_req->io_Flags = IOF_QUICK;
    writer_req->io_Data = (STRPTR)satisfy_port;
    writer_req->io_ClipID = 0;
    DoIO((struct IORequest *)writer_req);
    if (writer_req->io_Error != 0)
    {
        print("FAIL: CBD_POST failed\n");
        goto fail;
    }
    post_id = writer_req->io_ClipID;
    if (hook_state.calls != 1 ||
        hook_state.last_cmd != CBD_POST ||
        hook_state.last_clip_id != post_id ||
        hook_state.last_object != writer_req->io_Unit)
    {
        print("FAIL: CBD_POST did not invoke change hook correctly\n");
        goto fail;
    }
    print("OK: CBD_POST registers deferred clip and calls change hook\n");

    clip_req->io_Command = CBD_CURRENTREADID;
    clip_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)clip_req);
    if (clip_req->io_Error != 0 || clip_req->io_ClipID != post_id)
    {
        print("FAIL: CBD_CURRENTREADID did not expose post ID\n");
        goto fail;
    }

    clip_req->io_Command = CBD_CURRENTWRITEID;
    clip_req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)clip_req);
    if (clip_req->io_Error != 0 || clip_req->io_ClipID != post_id)
    {
        print("FAIL: CBD_CURRENTWRITEID did not expose post ID\n");
        goto fail;
    }
    print("OK: current clip IDs track deferred post\n");

    read_req->io_Command = CMD_READ;
    read_req->io_Data = (STRPTR)read_data;
    read_req->io_Length = sizeof(read_data);
    read_req->io_Offset = 0;
    read_req->io_ClipID = post_id;
    SendIO((struct IORequest *)read_req);
    if (CheckIO((struct IORequest *)read_req) != NULL)
    {
        print("FAIL: CMD_READ should pend for posted clip\n");
        goto fail;
    }
    print("OK: CMD_READ pends for posted clip\n");

    satisfy_msg = (struct SatisfyMsg *)GetMsg(satisfy_port);
    if (!satisfy_msg || satisfy_msg->sm_Unit != PRIMARY_CLIP || satisfy_msg->sm_ClipID != post_id)
    {
        print("FAIL: CBD_POST did not send expected SatisfyMsg\n");
        goto fail;
    }
    print("OK: CBD_POST sends SatisfyMsg on demand\n");

    writer_req->io_Command = CMD_WRITE;
    writer_req->io_Flags = IOF_QUICK;
    writer_req->io_Data = (STRPTR)post_data;
    writer_req->io_Length = sizeof(post_data) - 1;
    writer_req->io_Offset = 0;
    writer_req->io_ClipID = post_id;
    DoIO((struct IORequest *)writer_req);
    if (writer_req->io_Error != 0)
    {
        print("FAIL: CMD_WRITE failed while satisfying post\n");
        goto fail;
    }

    writer_req->io_Command = CMD_UPDATE;
    writer_req->io_Flags = IOF_QUICK;
    writer_req->io_ClipID = post_id;
    DoIO((struct IORequest *)writer_req);
    if (writer_req->io_Error != 0)
    {
        print("FAIL: CMD_UPDATE failed while satisfying post\n");
        goto fail;
    }

    error = WaitIO((struct IORequest *)read_req);
    if (error != 0 ||
        read_req->io_Actual != sizeof(post_data) - 1 ||
        !str_equal(read_data, post_data, read_req->io_Actual))
    {
        print("FAIL: Pending CMD_READ did not complete with posted data\n");
        goto fail;
    }
    if (hook_state.calls != 2 ||
        hook_state.last_cmd != CMD_UPDATE ||
        hook_state.last_clip_id != post_id)
    {
        print("FAIL: CMD_UPDATE did not invoke change hook correctly\n");
        goto fail;
    }
    print("OK: satisfying posted clip completes pending read and calls change hook\n");

    clip_req->io_Command = CBD_CHANGEHOOK;
    clip_req->io_Flags = IOF_QUICK;
    clip_req->io_Data = (STRPTR)&hook;
    clip_req->io_Length = 0;
    DoIO((struct IORequest *)clip_req);
    if (clip_req->io_Error != 0)
    {
        print("FAIL: CBD_CHANGEHOOK remove failed\n");
        goto fail;
    }
    reset_hook_state(&hook_state);

    writer_req->io_Command = CMD_WRITE;
    writer_req->io_Flags = IOF_QUICK;
    writer_req->io_Data = (STRPTR)final_data;
    writer_req->io_Length = sizeof(final_data) - 1;
    writer_req->io_Offset = 0;
    writer_req->io_ClipID = 0;
    DoIO((struct IORequest *)writer_req);
    if (writer_req->io_Error != 0)
    {
        print("FAIL: CMD_WRITE failed after hook removal\n");
        goto fail;
    }
    clip_id = writer_req->io_ClipID;

    writer_req->io_Command = CMD_UPDATE;
    writer_req->io_Flags = IOF_QUICK;
    writer_req->io_ClipID = clip_id;
    DoIO((struct IORequest *)writer_req);
    if (writer_req->io_Error != 0 || hook_state.calls != 0)
    {
        print("FAIL: removed change hook still fired\n");
        goto fail;
    }
    print("OK: CBD_CHANGEHOOK removal stops notifications\n");

    writer_req->io_Command = CBD_POST;
    writer_req->io_Flags = IOF_QUICK;
    writer_req->io_Data = (STRPTR)abort_port;
    writer_req->io_ClipID = 0;
    DoIO((struct IORequest *)writer_req);
    if (writer_req->io_Error != 0)
    {
        print("FAIL: CBD_POST failed for abort test\n");
        goto fail;
    }
    abort_post_id = writer_req->io_ClipID;

    read_req->io_Command = CMD_READ;
    read_req->io_Data = (STRPTR)read_data;
    read_req->io_Length = sizeof(read_data);
    read_req->io_Offset = 0;
    read_req->io_ClipID = abort_post_id;
    SendIO((struct IORequest *)read_req);
    if (CheckIO((struct IORequest *)read_req) != NULL)
    {
        print("FAIL: abort test CMD_READ should pend\n");
        goto fail;
    }

    abort_msg = (struct SatisfyMsg *)GetMsg(abort_port);
    if (!abort_msg || abort_msg->sm_ClipID != abort_post_id)
    {
        print("FAIL: abort test missing SatisfyMsg\n");
        goto fail;
    }

    AbortIO((struct IORequest *)read_req);
    error = WaitIO((struct IORequest *)read_req);
    if (error != IOERR_ABORTED || read_req->io_Error != IOERR_ABORTED)
    {
        print("FAIL: AbortIO did not abort pending CMD_READ\n");
        goto fail;
    }
    print("OK: AbortIO aborts pending clipboard reads\n");

    writer_req->io_Command = CMD_WRITE;
    writer_req->io_Flags = IOF_QUICK;
    writer_req->io_Data = (STRPTR)final_data;
    writer_req->io_Length = sizeof(final_data) - 1;
    writer_req->io_Offset = 0;
    writer_req->io_ClipID = 0;
    DoIO((struct IORequest *)writer_req);
    if (writer_req->io_Error != 0)
    {
        print("FAIL: cleanup CMD_WRITE after AbortIO failed\n");
        goto fail;
    }
    clip_id = writer_req->io_ClipID;
    writer_req->io_Command = CMD_UPDATE;
    writer_req->io_Flags = IOF_QUICK;
    writer_req->io_ClipID = clip_id;
    DoIO((struct IORequest *)writer_req);
    if (writer_req->io_Error != 0)
    {
        print("FAIL: cleanup CMD_UPDATE after AbortIO failed\n");
        goto fail;
    }

    device = clip_req->io_Device;
    device_node = FindName(&SysBase->DeviceList, (STRPTR)"clipboard.device");
    if (device_node != &device->dd_Library.lib_Node)
    {
        print("FAIL: clipboard.device missing from DeviceList before Expunge\n");
        goto fail;
    }

    RemDevice(device);
    if ((device->dd_Library.lib_Flags & LIBF_DELEXP) == 0 ||
        FindName(&SysBase->DeviceList, (STRPTR)"clipboard.device") != NULL)
    {
        print("FAIL: Expunge() did not defer/unlink correctly\n");
        goto fail;
    }
    print("OK: Expunge() defers while open and unlinks clipboard.device\n");

    error = OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)reopen_req, 0);
    if (error != IOERR_OPENFAIL)
    {
        print("FAIL: deferred Expunge() still allowed opens\n");
        if (error == 0)
            CloseDevice((struct IORequest *)reopen_req);
        goto fail;
    }
    print("OK: deferred Expunge() blocks new opens\n");

    CloseDevice((struct IORequest *)read_req);
    if (device->dd_Library.lib_OpenCnt != 2 ||
        (device->dd_Library.lib_Flags & LIBF_DELEXP) == 0)
    {
        print("FAIL: deferred Expunge cleared too early after first close\n");
        goto fail;
    }

    CloseDevice((struct IORequest *)writer_req);
    if (device->dd_Library.lib_OpenCnt != 1 ||
        (device->dd_Library.lib_Flags & LIBF_DELEXP) == 0)
    {
        print("FAIL: deferred Expunge cleared too early after second close\n");
        goto fail;
    }

    CloseDevice((struct IORequest *)clip_req);
    if (device->dd_Library.lib_OpenCnt != 0 ||
        (device->dd_Library.lib_Flags & LIBF_DELEXP) != 0 ||
        FindName(&SysBase->DeviceList, (STRPTR)"clipboard.device") != NULL)
    {
        print("FAIL: final Close() did not complete deferred Expunge\n");
        goto fail;
    }
    print("OK: final Close() completes deferred Expunge\n");

    print("OK: Cleanup complete\n");
    print("PASS: clipboard.device test complete\n");

    delete_clip_req(reopen_req);
    delete_clip_req(read_req);
    delete_clip_req(writer_req);
    delete_clip_req(clip_req);
    DeleteMsgPort(abort_port);
    DeleteMsgPort(satisfy_port);
    DeleteMsgPort(reply_port);
    return 0;

fail:
    close_if_open(read_req);
    close_if_open(writer_req);
    close_if_open(clip_req);

    delete_clip_req(reopen_req);
    delete_clip_req(read_req);
    delete_clip_req(writer_req);
    delete_clip_req(clip_req);

    if (abort_port)
        DeleteMsgPort(abort_port);
    if (satisfy_port)
        DeleteMsgPort(satisfy_port);
    if (reply_port)
        DeleteMsgPort(reply_port);

    return 1;
}
