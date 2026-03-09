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

    result = GT_GetIMsg(user_port);
    if (result != msg)
        return fail("GT_GetIMsg did not return the queued IntuiMessage");
    if (GetMsg(user_port) != NULL)
        return fail("GT_GetIMsg should remove the message from the user port");
    printf("OK: GT_GetIMsg returned the queued IntuiMessage\n");

    filtered = *msg;
    if (GT_FilterIMsg(&filtered) != &filtered)
        return fail("GT_FilterIMsg should return the original message pointer");
    if (GT_PostFilterIMsg(&filtered) != &filtered)
        return fail("GT_PostFilterIMsg should return the original message pointer");
    printf("OK: GT_FilterIMsg and GT_PostFilterIMsg preserve the message pointer\n");

    GT_ReplyIMsg(result);
    if ((struct IntuiMessage *)GetMsg(reply_port) != msg)
        return fail("GT_ReplyIMsg did not reply the message to the reply port");
    printf("OK: GT_ReplyIMsg replied the IntuiMessage\n");

    FreeMem(msg, sizeof(struct IntuiMessage));
    DeleteMsgPort(reply_port);
    DeleteMsgPort(user_port);
    CloseLibrary(GadToolsBase);

    printf("PASS: message wrapper test complete\n");
    return 0;
}
