/*
 * Probe (Phase 237b): the device list as 1.x programs walk it - through
 * DOSBase->dl_Root->rn_Info->di_DevInfo under Forbid(), without
 * LockDosList().  Fred Fish ANIMBuild copies every DLT_DEVICE name with a
 * handler task into a 32-byte buffer.  Only properties are printed (the
 * devices themselves differ between machines); with an argument every
 * entry is listed.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

int main(int argc, char **argv)
{
    struct RootNode *root = DOSBase->dl_Root;
    struct DosInfo *info = (struct DosInfo *)BADDR(root->rn_Info);
    struct DosList *dl;
    int devices = 0, with_task = 0, longest = 0, volumes = 0, assigns = 0, bad = 0;

    Forbid();
    for (dl = (struct DosList *)BADDR(info->di_DevInfo); dl; dl = (struct DosList *)BADDR(dl->dol_Next)) {
        UBYTE *name = (UBYTE *)BADDR(dl->dol_Name);
        int len = name ? name[0] : -1;
        if (argc > 1) {
            probe_s("type ");
            probe_dec(dl->dol_Type);
            probe_s(" task ");
            probe_s(dl->dol_Task ? "set" : "NULL");
            probe_s(" name ");
            if (name) {
                int i;
                for (i = 1; i <= len; i++)
                    probe_ch(name[i]);
            } else
                probe_s("(NULL)");
            probe_ch('\n');
        }
        if (!name || len == 0)
            bad++;
        if (dl->dol_Type == DLT_DEVICE) {
            devices++;
            if (dl->dol_Task) {
                with_task++;
                if (len > longest)
                    longest = len;
            }
        } else if (dl->dol_Type == DLT_VOLUME)
            volumes++;
        else
            assigns++;
    }
    Permit();
    P_BOOL("has devices", devices > 0);
    P_BOOL("has a device with a handler task", with_task > 0);
    P_BOOL("has volumes", volumes > 0);
    P_BOOL("names of devices with a task fit 30 characters", longest <= 30);
    P_BOOL("every entry has a name", bad == 0);
    return 0;
}
