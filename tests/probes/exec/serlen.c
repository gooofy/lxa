/*
 * Probe (Phase 237e): OpenDevice("serial.device") with an IORequest whose
 * mn_Length is 0.  Fred Fish PANL (disk 82) allocates an IOExtSer with
 * AllocMem() and leaves mn_Length 0; on the reference it opens the device
 * and its window.  Prints the OpenDevice() result for mn_Length 0 and for
 * sizeof(struct IOExtSer), shared.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/serial.h>
#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static void try(const char *label, UWORD len, UBYTE serflags)
{
    struct MsgPort *mp = CreateMsgPort();
    struct IOExtSer *io = AllocMem(sizeof(*io), MEMF_PUBLIC | MEMF_CLEAR);
    BYTE err;

    if (!mp || !io)
        return;
    io->IOSer.io_Message.mn_ReplyPort = mp;
    io->IOSer.io_Message.mn_Length = len;
    io->io_SerFlags = serflags;
    err = OpenDevice((STRPTR)SERIALNAME, 0, (struct IORequest *)io, 0);
    probe_s(label);
    probe_s(": ");
    probe_dec(err);
    probe_ch('\n');
    if (!err)
        CloseDevice((struct IORequest *)io);
    FreeMem(io, sizeof(*io));
    DeleteMsgPort(mp);
}

int main(void)
{
    try("mn_Length 0, shared", 0, SERF_SHARED);
    try("mn_Length sizeof, shared", sizeof(struct IOExtSer), SERF_SHARED);
    try("mn_Length sizeof(IOStdReq), shared", sizeof(struct IOStdReq), SERF_SHARED);
    /* (exclusive opens fail on the reference: its harness has the port) */
    return 0;
}
