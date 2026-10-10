/*
 * Probe (Phase 237e): ExNext() after the end of a directory.  Fred Fish
 * DTree (disk 905) calls ExNext() once more on a directory it has already
 * finished; lxa started the directory over.  Creates T:probe-exnext with
 * two files, scans it, calls ExNext() three times more, then scans it with
 * two FileInfoBlocks in turn.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

#define DIR "T:probe-exnext"

int main(void)
{
    struct FileInfoBlock *a = AllocDosObject(DOS_FIB, NULL), *b = AllocDosObject(DOS_FIB, NULL);
    BPTR l, f;
    LONG n, i, ra, rb;

    if (!a || !b)
        return 20;
    l = CreateDir((STRPTR)DIR);
    if (l)
        UnLock(l);
    f = Open((STRPTR)DIR "/one", MODE_NEWFILE);
    if (f)
        Close(f);
    f = Open((STRPTR)DIR "/two", MODE_NEWFILE);
    if (f)
        Close(f);
    l = Lock((STRPTR)DIR, ACCESS_READ);
    if (!l)
        return 20;
    Examine(l, a);
    n = 0;
    while (ExNext(l, a))
        n++;
    P_LONG("entries", n);
    P_LONG("IoErr at the end", IoErr());
    for (i = 0; i < 3; i++) {
        LONG r = ExNext(l, a);
        probe_s("ExNext after the end: ");
        probe_dec(r ? 1 : 0);
        probe_s(" IoErr ");
        probe_dec(IoErr());
        probe_ch('\n');
    }
    /* two FIBs on one lock, in turn */
    Examine(l, a);
    Examine(l, b);
    for (i = 0; i < 3; i++) {
        ra = ExNext(l, a);
        rb = ExNext(l, b);
        probe_s("in turn: ");
        probe_dec(ra ? 1 : 0);
        probe_ch(' ');
        probe_dec(rb ? 1 : 0);
        probe_ch('\n');
    }
    UnLock(l);
    DeleteFile((STRPTR)DIR "/one");
    DeleteFile((STRPTR)DIR "/two");
    DeleteFile((STRPTR)DIR);
    FreeDosObject(DOS_FIB, a);
    FreeDosObject(DOS_FIB, b);
    return 0;
}
