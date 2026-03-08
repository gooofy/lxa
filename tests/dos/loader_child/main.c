#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#define TEST_PORT_NAME "LoadSegRuntime.Parent"
#define TEST_MESSAGE_MAGIC 0x4c535247UL

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

struct LoaderChildMessage {
    struct Message msg;
    ULONG magic;
};

static void notify_parent(void)
{
    struct MsgPort *port;
    struct LoaderChildMessage *msg;
    struct Process *me = (struct Process *)FindTask(NULL);

    Forbid();
    port = FindPort((STRPTR)TEST_PORT_NAME);
    Permit();

    if (!port)
        return;

    msg = (struct LoaderChildMessage *)AllocMem(sizeof(*msg), MEMF_PUBLIC | MEMF_CLEAR);
    if (!msg)
        return;

    msg->msg.mn_Node.ln_Type = NT_MESSAGE;
    msg->msg.mn_Length = sizeof(*msg);
    msg->msg.mn_ReplyPort = me ? &me->pr_MsgPort : NULL;
    msg->magic = TEST_MESSAGE_MAGIC;
    PutMsg(port, (struct Message *)msg);
}

int main(void)
{
    LONG rc = 0;
    struct Process *me = (struct Process *)FindTask(NULL);
    STRPTR argstr = me ? me->pr_Arguments : NULL;

    notify_parent();

    if (argstr) {
        const char *p = (const char *)argstr;

        while (*p == ' ' || *p == '\t')
            p++;

        if (p[0] == 'R' && p[1] == 'E' && p[2] == 'T' && p[3] == 'U' &&
            p[4] == 'R' && p[5] == 'N' && p[6] == '=') {
            BOOL negative = FALSE;
            LONG value = 0;

            p += 7;
            if (*p == '-') {
                negative = TRUE;
                p++;
            }

            while (*p >= '0' && *p <= '9') {
                value = value * 10 + (*p - '0');
                p++;
            }

            rc = negative ? -value : value;
        }
    }

    Exit(rc);
    return rc;
}
