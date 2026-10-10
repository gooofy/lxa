/*
 * Probe (Phase 237d): Info() fills in id_VolumeNode with the volume's
 * DosList node.  Fred Fish IInfo (disk 626) prints the volume name through
 * it; lxa left it 0 and IInfo formatted a string from a wild pointer.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static struct InfoData id __attribute__((aligned(4)));

static void t(const char *path)
{
    BPTR l = Lock((STRPTR)path, SHARED_LOCK);
    probe_s(path);
    if (!l || !Info(l, &id)) {
        probe_s(": no Info\n");
        if (l)
            UnLock(l);
        return;
    }
    if (!id.id_VolumeNode)
        probe_s(": id_VolumeNode 0\n");
    else {
        struct DosList *dl = (struct DosList *)BADDR(id.id_VolumeNode);
        UBYTE *n = (UBYTE *)BADDR(dl->dol_Name);
        int i;
        probe_s(": type ");
        probe_dec(dl->dol_Type);
        probe_s(", name ");
        for (i = 1; i <= n[0]; i++)
            probe_ch(n[i]);
        probe_s(", is the lock's volume ");
        probe_s(BADDR(((struct FileLock *)BADDR(l))->fl_Volume) == dl ? "yes" : "no");
        probe_ch('\n');
    }
    UnLock(l);
}

int main(void)
{
    t("SYS:");
    t("SYS:C");
    t("RAM:");
    return 0;
}
