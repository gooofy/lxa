/*
 * Test: console/idcmp_console
 *
 * Who receives a keystroke typed into a window that has a console.device
 * unit attached: the console (CMD_READ), the window's IDCMP port, or both -
 * with IDCMP_RAWKEY, with no IDCMP flags and with IDCMP_VANILLAKEY.
 * Interactive: the driver (tests/scenarios/interactive/idcmp_console.yaml)
 * activates the window and presses "a" after each "Waiting for key" line.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>

extern struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
extern struct ExecBase *SysBase;

static void print(const char *s)
{
    LONG n = 0;
    while (s[n])
        n++;
    Write(Output(), (APTR)s, n);
}

static struct Window *win;
static struct IOStdReq *con;
static struct MsgPort *conport;
static char ch;
static BOOL pending;

static void start_read(void)
{
    con->io_Command = CMD_READ;
    con->io_Data = &ch;
    con->io_Length = 1;
    SendIO((struct IORequest *)con);
    pending = TRUE;
}

static void trial(const char *name, ULONG idcmp)
{
    LONG ticks, after = -1, idcmp_keys = 0, console_chars = 0;
    struct IntuiMessage *im;

    ModifyIDCMP(win, idcmp);
    print("Waiting for key: ");
    print(name);
    print("\n");
    /* up to 10 s for the key, then 0.5 s for anything that follows it */
    for (ticks = 0; ticks < 500 && (after < 0 || ticks < after + 25); ticks++) {
        Delay(1);
        if (win->UserPort) {
            while ((im = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL) {
                if (im->Class == IDCMP_RAWKEY || im->Class == IDCMP_VANILLAKEY) {
                    idcmp_keys++;
                    if (after < 0)
                        after = ticks;
                }
                ReplyMsg((struct Message *)im);
            }
        }
        while (pending && CheckIO((struct IORequest *)con)) {
            WaitIO((struct IORequest *)con);
            pending = FALSE;
            if (con->io_Actual > 0) {
                console_chars++;
                if (after < 0)
                    after = ticks;
            }
            start_read();
        }
    }
    print(name);
    print(after < 0 ? ": no key arrived\n" : ": key arrived\n");
    print(console_chars ? "  console: got characters\n" : "  console: nothing\n");
    print(idcmp_keys ? "  idcmp: got key messages\n" : "  idcmp: nothing\n");
}

int main(void)
{
    struct NewWindow nw;
    int rc = 1;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 20;
    conport = CreateMsgPort();
    con = conport ? (struct IOStdReq *)CreateIORequest(conport, sizeof(struct IOStdReq)) : NULL;
    if (!con)
        goto out;

    nw.LeftEdge = 0;
    nw.TopEdge = 0;
    nw.Width = 320;
    nw.Height = 100;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_RAWKEY;
    nw.Flags = WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_DRAGBAR | WFLG_DEPTHGADGET;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"Console IDCMP Test";
    nw.Screen = NULL;
    nw.BitMap = NULL;
    nw.MinWidth = nw.MinHeight = 0;
    nw.MaxWidth = nw.MaxHeight = 0;
    nw.Type = WBENCHSCREEN;
    win = OpenWindow(&nw);
    if (!win)
        goto out;

    con->io_Data = win;
    con->io_Length = sizeof(struct Window);
    if (OpenDevice((STRPTR)"console.device", CONU_STANDARD, (struct IORequest *)con, 0) != 0) {
        print("FAIL: console.device\n");
        goto out;
    }
    start_read();

    trial("IDCMP_RAWKEY", IDCMP_RAWKEY);
    trial("no IDCMP", 0);
    trial("IDCMP_VANILLAKEY", IDCMP_VANILLAKEY);
    print("done\n");
    rc = 0;

    if (pending) {
        AbortIO((struct IORequest *)con);
        WaitIO((struct IORequest *)con);
    }
    CloseDevice((struct IORequest *)con);
out:
    if (win)
        CloseWindow(win);
    if (con)
        DeleteIORequest((struct IORequest *)con);
    if (conport)
        DeleteMsgPort(conport);
    CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
