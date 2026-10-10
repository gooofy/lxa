/*
 * Probe (Phase 237c, from Phase 255): a lock is a BPTR to a struct
 * FileLock that programs read directly - Modula-2 programs (Fred Fish
 * calendarfactory, AmigaPunt, Pennywise) find the volume name through
 * fl_Volume, SafeBoot copies it.  Prints the fields as properties.
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

static void pbstr(BSTR b)
{
    UBYTE *s = (UBYTE *)BADDR(b);
    int i;
    if (!s) {
        probe_s("(none)");
        return;
    }
    probe_ch('"');
    for (i = 1; i <= s[0]; i++)
        probe_ch(s[i]);
    probe_ch('"');
}

static void show(const char *name, LONG mode)
{
    BPTR l = Lock((STRPTR)name, mode);
    struct FileLock *fl;
    struct DosList *vol;

    probe_s(name);
    if (!l) {
        probe_s(": no lock\n");
        return;
    }
    fl = (struct FileLock *)BADDR(l);
    vol = (struct DosList *)BADDR(fl->fl_Volume);
    probe_s(": access ");
    probe_dec(fl->fl_Access);
    probe_s(" task ");
    probe_s(fl->fl_Task ? "set" : "NULL");
    probe_s(fl->fl_Task && fl->fl_Task == DeviceProc((STRPTR)name) ? " (DeviceProc)" : "");
    probe_s(" volume ");
    if (vol) {
        probe_s("type ");
        probe_dec(vol->dol_Type);
        probe_s(" name ");
        pbstr(vol->dol_Name);
        probe_s(vol->dol_Task == fl->fl_Task ? " task=lock's" : " task differs");
    } else
        probe_s("NULL");
    probe_ch('\n');
    {
        BPTR d = DupLock(l);
        struct FileLock *fd = (struct FileLock *)BADDR(d);
        probe_s("  DupLock: ");
        probe_s(d && d != l ? "new lock" : "same/none");
        if (d) {
            probe_s(fd->fl_Volume == fl->fl_Volume ? ", same volume" : ", other volume");
            probe_s(fd->fl_Key == fl->fl_Key ? ", same key" : ", other key");
            UnLock(d);
        }
        probe_ch('\n');
    }
    UnLock(l);
}

int main(void)
{
    show("RAM:", SHARED_LOCK);
    show("SYS:", SHARED_LOCK);
    show("SYS:C", SHARED_LOCK);
    {
        BPTR a = Lock((STRPTR)"SYS:C", SHARED_LOCK), b = Lock((STRPTR)"SYS:Libs", SHARED_LOCK);
        if (a && b)
            P_BOOL("SYS:C and SYS:Libs keys differ",
                   ((struct FileLock *)BADDR(a))->fl_Key != ((struct FileLock *)BADDR(b))->fl_Key);
        if (a) UnLock(a);
        if (b) UnLock(b);
    }
    return 0;
}
