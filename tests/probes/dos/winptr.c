/*
 * Probe (Phase 237e): pr_WindowPtr of a command and of the processes it
 * creates.  Fred Fish WatcherPrefs (disk 879) and BootPic (disk 963) pass
 * it to EasyRequest() or test it before showing a requester.  Prints the
 * command's pr_WindowPtr (0, -1 or a window) and that of a CreateNewProc()
 * child started without NP_WindowPtr.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static volatile LONG child_wp = 0x55555555;
static volatile LONG child_done;

static void child(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    child_wp = (LONG)me->pr_WindowPtr;
    Forbid();
    child_done = 1;
}

static void show(const char *label, LONG v)
{
    probe_s(label);
    probe_s(": ");
    probe_s(v == 0 ? "0" : v == -1 ? "-1" : "a window");
    probe_ch('\n');
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    int i;

    show("command", (LONG)me->pr_WindowPtr);
    if (CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"probe.winptr", NP_Output, 0,
                          NP_Input, 0, NP_CloseOutput, FALSE, NP_CloseInput, FALSE, TAG_END)) {
        for (i = 0; i < 50 && !child_done; i++)
            Delay(1);
        show("child", child_wp);
    }
    me->pr_WindowPtr = (APTR)-1;
    child_done = 0;
    if (CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"probe.winptr", NP_Output, 0,
                          NP_Input, 0, NP_CloseOutput, FALSE, NP_CloseInput, FALSE, TAG_END)) {
        for (i = 0; i < 50 && !child_done; i++)
            Delay(1);
        show("child of a parent with -1", child_wp);
    }
    me->pr_WindowPtr = NULL;
    return 0;
}
