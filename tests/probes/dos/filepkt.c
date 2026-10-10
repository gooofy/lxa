/*
 * Probe (Phase 237d): packets sent to a file's handler directly.  Fred
 * Fish ASendPacket (disk 66) sends ACTION_WRITE packets to the fh_Type
 * port of a RAM: file with the handle's fh_Arg1; on lxa fh_Type was NULL
 * and the program waited forever.  Writes, seeks and reads through
 * DoPkt() and checks the file through the normal calls.
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

static char name[] = "T:probe-filepkt";

int main(void)
{
    BPTR f;
    struct FileHandle *fh;
    LONG r;
    char buf[16];
    int i;

    f = Open((STRPTR)name, MODE_NEWFILE);
    if (!f)
        return 20;
    fh = (struct FileHandle *)BADDR(f);
    P_BOOL("fh_Type set", fh->fh_Type != NULL);
    if (!fh->fh_Type) {
        Close(f);
        return 0;
    }
    r = DoPkt(fh->fh_Type, ACTION_WRITE, fh->fh_Arg1, (LONG)"hello world", 11, 0, 0);
    P_LONG("ACTION_WRITE", r);
    r = DoPkt(fh->fh_Type, ACTION_SEEK, fh->fh_Arg1, 6, OFFSET_BEGINNING, 0, 0);
    P_LONG("ACTION_SEEK (old position)", r);
    for (i = 0; i < 16; i++)
        buf[i] = 0;
    r = DoPkt(fh->fh_Type, ACTION_READ, fh->fh_Arg1, (LONG)buf, 5, 0, 0);
    P_LONG("ACTION_READ", r);
    probe_s("read: ");
    probe_s(buf);
    probe_ch('\n');
    Close(f);

    f = Open((STRPTR)name, MODE_OLDFILE);
    for (i = 0; i < 16; i++)
        buf[i] = 0;
    P_LONG("Read() after Close", Read(f, buf, 15));
    probe_s("file: ");
    probe_s(buf);
    probe_ch('\n');
    Close(f);
    DeleteFile((STRPTR)name);
    return 0;
}
