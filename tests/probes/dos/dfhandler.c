/*
 * Probe (Phase 237e): the handlers of the floppy drives DF0: and DF1:
 * without a disk.  Fred Fish FastDisk (disk 286) inhibits DF1: and DF0:
 * with packets to their DeviceProc() ports; TurboBackup (disk 139) looks
 * for them too.  Prints DeviceProc(), the handler task's name, the answers
 * to ACTION_INHIBIT and ACTION_DISK_INFO, and Lock("DF0:") (with requesters
 * off).
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static void drive(const char *name)
{
    struct MsgPort *port = DeviceProc((STRPTR)name);
    struct InfoData *id = AllocMem(sizeof(*id), MEMF_PUBLIC | MEMF_CLEAR);
    LONG r;

    probe_s(name);
    probe_s(" DeviceProc: ");
    if (!port) {
        probe_s("NULL, IoErr ");
        probe_dec(IoErr());
        probe_ch('\n');
        return;
    }
    probe_s("a port, task ");
    probe_s(port->mp_SigTask && ((struct Task *)port->mp_SigTask)->tc_Node.ln_Name ?
            ((struct Task *)port->mp_SigTask)->tc_Node.ln_Name : "(none)");
    probe_ch('\n');
    r = DoPkt(port, ACTION_INHIBIT, DOSTRUE, 0, 0, 0, 0);
    probe_s("  INHIBIT TRUE: ");
    probe_dec(r);
    probe_ch(' ');
    probe_dec(IoErr());
    probe_ch('\n');
    if (id) {
        r = DoPkt(port, ACTION_DISK_INFO, MKBADDR(id), 0, 0, 0, 0);
        probe_s("  DISK_INFO: ");
        probe_dec(r);
        probe_s(" type ");
        probe_hex(id->id_DiskType, 8);
        probe_s(" state ");
        probe_dec(id->id_DiskState);
        probe_s(" unit ");
        probe_dec(id->id_UnitNumber);
        probe_ch('\n');
        FreeMem(id, sizeof(*id));
    }
    r = DoPkt(port, ACTION_INHIBIT, DOSFALSE, 0, 0, 0, 0);
    probe_s("  INHIBIT FALSE: ");
    probe_dec(r);
    probe_ch(' ');
    probe_dec(IoErr());
    probe_ch('\n');
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR wp = me->pr_WindowPtr;
    BPTR l;

    drive("DF0:");
    drive("DF1:");
    me->pr_WindowPtr = (APTR)-1;
    l = Lock((STRPTR)"DF0:", ACCESS_READ);
    probe_s("Lock(\"DF0:\"): ");
    probe_s(l ? "a lock" : "0");
    probe_s(" IoErr ");
    probe_dec(IoErr());
    probe_ch('\n');
    if (l)
        UnLock(l);
    me->pr_WindowPtr = wp;
    return 0;
}
