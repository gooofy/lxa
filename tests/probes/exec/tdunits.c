/*
 * Probe (Phase 237d): which trackdisk.device units open, and what the
 * system has for its floppy drives.  Fred Fish NoClick2 (disk 231) opens
 * unit 1 and, when that works, waits for a second "trackdisk.device" task;
 * NoClick (disk 501) walks the DosList for device nodes with a startup
 * message.  Prints OpenDevice() results for units 0-4, whether a task
 * "trackdisk.device" exists, and the DosList device nodes whose startup
 * message names trackdisk.device.
 */
#include <exec/types.h>
#include <exec/io.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <devices/trackdisk.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <string.h>
#include "probe.h"

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static void bstr(BSTR b)
{
    UBYTE *s = (UBYTE *)BADDR(b);
    int i;
    if (!s) {
        probe_s("(null)");
        return;
    }
    for (i = 0; i < s[0]; i++)
        probe_ch(s[1 + i]);
}

int main(void)
{
    struct MsgPort *mp = CreateMsgPort();
    struct IOExtTD *io;
    struct Task *t;
    struct DosList *dl;
    ULONG unit;

    if (!mp)
        return 20;
    io = (struct IOExtTD *)CreateIORequest(mp, sizeof(struct IOExtTD));
    if (!io)
        return 20;
    for (unit = 0; unit < 5; unit++) {
        BYTE err = OpenDevice((STRPTR)TD_NAME, unit, (struct IORequest *)io, 0);
        probe_s("OpenDevice(trackdisk ");
        probe_dec(unit);
        probe_s(") = ");
        probe_dec(err);
        if (!err) {
            io->iotd_Req.io_Command = TD_GETDRIVETYPE;
            DoIO((struct IORequest *)io);
            probe_s(" drivetype=");
            probe_dec(io->iotd_Req.io_Actual);
            CloseDevice((struct IORequest *)io);
        }
        probe_ch('\n');
    }
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(mp);

    Forbid();
    t = FindTask((STRPTR)"trackdisk.device");
    {
        struct Node *n;
        LONG count = 0, pri = -999;
        for (n = SysBase->TaskWait.lh_Head; n->ln_Succ; n = n->ln_Succ)
            if (n->ln_Name && !strcmp(n->ln_Name, "trackdisk.device")) {
                count++;
                pri = n->ln_Pri;
            }
        for (n = SysBase->TaskReady.lh_Head; n->ln_Succ; n = n->ln_Succ)
            if (n->ln_Name && !strcmp(n->ln_Name, "trackdisk.device"))
                count++;
        Permit();
        P_BOOL("FindTask(trackdisk.device)", t != NULL);
        P_LONG("waiting/ready trackdisk.device tasks", count);
        P_LONG("their priority", pri);
        if (t)
            P_LONG("node type", t->tc_Node.ln_Type);
    }

    dl = LockDosList(LDF_DEVICES | LDF_READ);
    while ((dl = NextDosEntry(dl, LDF_DEVICES)) != NULL) {
        struct FileSysStartupMsg *fssm;
        if ((ULONG)dl->dol_misc.dol_handler.dol_Startup <= 2)
            continue;
        fssm = (struct FileSysStartupMsg *)BADDR(dl->dol_misc.dol_handler.dol_Startup);
        if (!fssm->fssm_Device)
            continue;
        {
            UBYTE *d = (UBYTE *)BADDR(fssm->fssm_Device);
            if (d[0] < 9 || d[1] != 't' || d[2] != 'r' || d[3] != 'a' || d[4] != 'c' || d[5] != 'k')
                continue;
        }
        probe_s("device ");
        bstr(dl->dol_Name);
        probe_s(": unit=");
        probe_dec(fssm->fssm_Unit);
        probe_s(" device=");
        bstr(fssm->fssm_Device);
        probe_ch('\n');
    }
    UnLockDosList(LDF_DEVICES | LDF_READ);
    return 0;
}
