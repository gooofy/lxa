#include <exec/types.h>
#include <exec/ports.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <clib/exec_protos.h>
#include <clib/gadtools_protos.h>
#include <stdio.h>

struct Library *GadToolsBase = NULL;

static int fail(const char *message)
{
    printf("FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    struct MsgPort *user_port = NULL;
    struct MsgPort *reply_port = NULL;
    struct IntuiMessage *msg;
    struct IntuiMessage filtered;
    struct IntuiMessage *result;

    printf("Testing GadTools message wrappers...\n");

    GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 37);
    if (!GadToolsBase)
        return fail("cannot open gadtools.library");

    user_port = CreateMsgPort();
    reply_port = CreateMsgPort();
    if (!user_port || !reply_port)
        return fail("cannot create message ports");

    msg = (struct IntuiMessage *)AllocMem(sizeof(struct IntuiMessage), MEMF_CLEAR | MEMF_PUBLIC);
    if (!msg)
        return fail("cannot allocate IntuiMessage");

    msg->ExecMessage.mn_ReplyPort = reply_port;
    msg->Class = IDCMP_GADGETUP;
    msg->Code = 123;
    PutMsg(user_port, (struct Message *)msg);

    /* GadTools hands out its own copy of the message (AmigaOS 3.1). */
    result = GT_GetIMsg(user_port);
    if (!result)
        return fail("GT_GetIMsg did not return the queued IntuiMessage");
    if (result == msg)
        return fail("GT_GetIMsg should return GadTools' copy, not the original message");
    if (result->Class != IDCMP_GADGETUP || result->Code != 123)
        return fail("GT_GetIMsg copy does not carry the message contents");
    if (GetMsg(user_port) != NULL)
        return fail("GT_GetIMsg should remove the message from the user port");
    if (GetMsg(reply_port) != NULL)
        return fail("GT_GetIMsg must not reply the original before GT_ReplyIMsg");
    printf("OK: GT_GetIMsg returned a copy of the queued IntuiMessage\n");

    GT_ReplyIMsg(result);
    if ((struct IntuiMessage *)GetMsg(reply_port) != msg)
        return fail("GT_ReplyIMsg did not reply the original message to the reply port");
    printf("OK: GT_ReplyIMsg replied the original IntuiMessage\n");

    filtered = *msg;
    result = GT_FilterIMsg(&filtered);
    if (!result || result == &filtered)
        return fail("GT_FilterIMsg should return GadTools' copy of the message");
    if (result->Class != IDCMP_GADGETUP || result->Code != 123)
        return fail("GT_FilterIMsg copy does not carry the message contents");
    if (GT_PostFilterIMsg(result) != &filtered)
        return fail("GT_PostFilterIMsg should return the original message pointer");
    printf("OK: GT_FilterIMsg/GT_PostFilterIMsg map a copy back to the original\n");

    FreeMem(msg, sizeof(struct IntuiMessage));
    DeleteMsgPort(reply_port);
    DeleteMsgPort(user_port);
    CloseLibrary(GadToolsBase);

    printf("PASS: message wrapper test complete\n");
    return 0;
}
