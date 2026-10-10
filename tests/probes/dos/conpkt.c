/*
 * Probe (Phase 237d): packets to the handler of a CON:/RAW: window.
 * Programs find their console window with ACTION_DISK_INFO
 * (id_VolumeNode = the window, id_InUse = the console IORequest) and
 * switch modes with ACTION_SCREEN_MODE (Fred Fish Mon, disk 310; vim).
 * lxa's console windows had no handler port (fh_Type NULL).
 */
#include <exec/types.h>
#include <exec/io.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static struct InfoData id __attribute__((aligned(4)));

static int strcmp_probe(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a != *b;
}

static void t(const char *spec)
{
    BPTR f = Open((STRPTR)spec, MODE_NEWFILE);
    struct FileHandle *fh;
    LONG r;

    probe_s(spec);
    probe_ch('\n');
    if (!f) {
        probe_s("  no window\n");
        return;
    }
    fh = (struct FileHandle *)BADDR(f);
    P_BOOL("  fh_Type set", fh->fh_Type != NULL);
    if (fh->fh_Type) {
        r = DoPkt(fh->fh_Type, ACTION_DISK_INFO, MKBADDR(&id), 0, 0, 0, 0);
        P_BOOL("  ACTION_DISK_INFO", r != 0);
        if (r) {
            struct Window *w = (struct Window *)id.id_VolumeNode;
            probe_s("  window title: ");
            probe_s(w && w->Title ? (char *)w->Title : "(none)");
            probe_ch('\n');
            P_BOOL("  id_InUse is an IORequest on console.device",
                   id.id_InUse && ((struct IORequest *)id.id_InUse)->io_Device &&
                   !strcmp_probe(((struct IORequest *)id.id_InUse)->io_Device->dd_Library.lib_Node.ln_Name,
                                 "console.device"));
            P_HEX("  id_DiskType", id.id_DiskType);
        }
        r = DoPkt(fh->fh_Type, ACTION_SCREEN_MODE, DOSTRUE, 0, 0, 0, 0);
        P_BOOL("  ACTION_SCREEN_MODE raw", r != 0);
        r = DoPkt(fh->fh_Type, ACTION_SCREEN_MODE, 0, 0, 0, 0, 0);
        P_BOOL("  ACTION_SCREEN_MODE cooked", r != 0);
        r = DoPkt(fh->fh_Type, ACTION_WAIT_CHAR, 1000, 0, 0, 0, 0);
        P_LONG("  ACTION_WAIT_CHAR", r);
        r = DoPkt(fh->fh_Type, ACTION_WRITE, fh->fh_Arg1, (LONG)"hello", 5, 0, 0);
        P_LONG("  ACTION_WRITE", r);
    }
    Close(f);
}

int main(void)
{
    t("CON:0/0/320/100/probe con");
    t("RAW:0/0/320/100/probe raw");
    return 0;
}
