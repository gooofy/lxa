/*
 * Test: commodities/exchange_gate (Phase 251)
 *
 * A commodity with a broker that Workbench 3.1's Exchange lists and
 * controls: it starts WB31:Tools/Commodities/Exchange and prints every
 * command it receives (disable/enable, show/hide, kill).  The scenario
 * tests/scenarios/exchange-gate.yaml drives Exchange; stdout and the
 * Exchange window are compared with AmigaOS 3.1.
 */
#include <exec/types.h>
#include <exec/ports.h>
#include <dos/dostags.h>
#include <libraries/commodities.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/commodities_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/commodities.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct Library *CxBase = NULL;

static void say(const char *s)
{
    Printf((STRPTR)"%s\n", s);
    Flush(Output());
}

int main(void)
{
    struct MsgPort *port;
    struct NewBroker nb;
    CxObj *broker;
    CxMsg *msg;
    LONG err;
    BOOL done = FALSE;
    BPTR nil;

    CxBase = OpenLibrary((STRPTR)"commodities.library", 37);
    port = CreateMsgPort();
    if (!CxBase || !port)
        return 20;

    nb.nb_Version = NB_VERSION;
    nb.nb_Name = (STRPTR)"LXA Gate";
    nb.nb_Title = (STRPTR)"LXA Gate Commodity";
    nb.nb_Descr = (STRPTR)"Controlled by Exchange";
    nb.nb_Unique = NBU_UNIQUE | NBU_NOTIFY;
    nb.nb_Flags = COF_SHOW_HIDE;
    nb.nb_Pri = 0;
    nb.nb_Port = port;
    nb.nb_ReservedChannel = 0;
    broker = CxBroker(&nb, &err);
    if (!broker) {
        Printf((STRPTR)"CxBroker failed: %ld\n", err);
        return 20;
    }
    ActivateCxObj(broker, TRUE);
    say("broker ready");

    nil = Open((STRPTR)"NIL:", MODE_NEWFILE);
    if (SystemTags((STRPTR)"WB31:Tools/Commodities/Exchange",
                   SYS_Asynch, TRUE, SYS_Input, nil, SYS_Output, 0,
                   TAG_DONE) != 0) {
        say("cannot start Exchange");
        Close(nil);
        DeleteCxObjAll(broker);
        return 20;
    }
    say("Exchange started");

    while (!done) {
        WaitPort(port);
        while ((msg = (CxMsg *)GetMsg(port)) != NULL) {
            ULONG type = CxMsgType(msg);
            LONG id = CxMsgID(msg);

            ReplyMsg((struct Message *)msg);
            if (type != CXM_COMMAND)
                continue;
            switch (id) {
            case CXCMD_DISABLE:
                say("CXCMD_DISABLE");
                ActivateCxObj(broker, FALSE);
                break;
            case CXCMD_ENABLE:
                say("CXCMD_ENABLE");
                ActivateCxObj(broker, TRUE);
                break;
            case CXCMD_APPEAR:
                say("CXCMD_APPEAR");
                break;
            case CXCMD_DISAPPEAR:
                say("CXCMD_DISAPPEAR");
                break;
            case CXCMD_KILL:
                say("CXCMD_KILL");
                done = TRUE;
                break;
            case CXCMD_UNIQUE:
                say("CXCMD_UNIQUE");
                break;
            default:
                Printf((STRPTR)"command %ld\n", id);
                Flush(Output());
                break;
            }
        }
    }

    DeleteCxObjAll(broker);
    while ((msg = (CxMsg *)GetMsg(port)) != NULL)
        ReplyMsg((struct Message *)msg);
    DeleteMsgPort(port);
    CloseLibrary(CxBase);
    say("done");
    return 0;
}
