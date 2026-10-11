/*
 * Probe (Phase 237e): trackdisk.device status commands with no disk in the
 * drive.  Fred Fish Kickmaker (disk 955) asks TD_PROTSTATUS and expects an
 * error when DF0: is empty.  Prints io_Error and io_Actual of
 * TD_CHANGESTATE, TD_PROTSTATUS, TD_GETDRIVETYPE and TD_GETNUMTRACKS for
 * unit 0.
 */
#include <exec/types.h>
#include <exec/io.h>
#include <devices/trackdisk.h>
#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static void cmd(struct IOExtTD *io, UWORD c, const char *name)
{
    io->iotd_Req.io_Command = c;
    io->iotd_Req.io_Flags = 0;
    io->iotd_Req.io_Actual = 0x5a5a;
    DoIO((struct IORequest *)io);
    probe_s(name);
    probe_s(": error ");
    probe_dec(io->iotd_Req.io_Error);
    probe_s(" actual ");
    probe_dec(io->iotd_Req.io_Actual);
    probe_ch('\n');
}

int main(void)
{
    struct MsgPort *mp = CreateMsgPort();
    struct IOExtTD *io;

    if (!mp)
        return 20;
    io = (struct IOExtTD *)CreateIORequest(mp, sizeof(*io));
    if (!io || OpenDevice((STRPTR)TD_NAME, 0, (struct IORequest *)io, 0))
        return 20;
    cmd(io, TD_CHANGESTATE, "TD_CHANGESTATE");
    cmd(io, TD_PROTSTATUS, "TD_PROTSTATUS");
    cmd(io, TD_GETDRIVETYPE, "TD_GETDRIVETYPE");
    cmd(io, TD_GETNUMTRACKS, "TD_GETNUMTRACKS");
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(mp);
    return 0;
}
