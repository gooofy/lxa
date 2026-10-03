/*
 * Probe (Phase 222a): the names NameFromLock()/NameFromFH() give for
 * assigns and volumes.
 *
 * No volnames.ref.out yet: AmigaOS 3.1 names every object by its volume
 * ("Ram Disk:T", "System:C", ENV: is "Ram Disk:ENV"), lxa's boot volume
 * is "SYS", RAM: has no "Ram Disk" volume and T:/ENV: are assigns to host
 * directories outside every volume, so lxa returns "T:", "ENVARC:", ...
 * Capture it once lxa's system volumes are laid out like the reference.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include "probe.h"

static void name(const char *path)
{
    char buf[256];
    BPTR l = Lock((STRPTR)path, ACCESS_READ);
    probe_s("NameFromLock(");
    probe_s(path);
    probe_s(") = ");
    if (!l) {
        probe_s("(no lock)\n");
        return;
    }
    if (NameFromLock(l, (STRPTR)buf, sizeof(buf)))
        P_STR("", buf);
    else {
        probe_s("FAIL ");
        probe_dec(IoErr());
        probe_ch('\n');
    }
    UnLock(l);
}

int main(void)
{
    char buf[256];
    BPTR fh;

    name("RAM:");
    name("T:");
    name("SYS:");
    name("SYS:Tests");
    name("C:");
    name("LIBS:");
    name("S:");
    name("ENV:");
    name("ENVARC:");
    name("DEVS:");
    name("FONTS:");
    name("L:");
    name("");
    name("/");
    fh = Open((STRPTR)"T:lxaprobe_vol", MODE_NEWFILE);
    if (fh) {
        if (NameFromFH(fh, (STRPTR)buf, sizeof(buf)))
            P_STR("NameFromFH(T:lxaprobe_vol)", buf);
        Close(fh);
        DeleteFile((STRPTR)"T:lxaprobe_vol");
    }
    return 0;
}
