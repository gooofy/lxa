/*
 * Probe (Phase 237): the printer handler PRT: as programs see it before
 * they print (Fish PR, prspool open it at startup).  Nothing is written:
 * the reference has no printer.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>
#include "probe.h"

static void try_open(const char *label, const char *name, LONG mode)
{
    BPTR fh = Open((STRPTR)name, mode);
    probe_s(label);
    probe_s(fh ? " = non-NULL" : " = NULL");
    if (!fh) {
        probe_s(" IoErr ");
        probe_dec(IoErr());
    }
    probe_ch('\n');
    if (fh) {
        P_BOOL("  IsInteractive", IsInteractive(fh));
        P_LONG("  Close", Close(fh));
    }
}

int main(void)
{
    BPTR lock;
    LONG err;

    P_SECTION("PRT:");
    try_open("Open PRT: MODE_OLDFILE", "PRT:", MODE_OLDFILE);
    try_open("Open PRT: MODE_NEWFILE", "PRT:", MODE_NEWFILE);
    try_open("Open PRT:RAW MODE_NEWFILE", "PRT:RAW", MODE_NEWFILE);
    lock = Lock((STRPTR)"PRT:", SHARED_LOCK);
    err = IoErr();
    probe_s("Lock PRT:");
    probe_s(lock ? " = non-NULL\n" : " = NULL\n");
    if (lock)
        UnLock(lock);
    else
        P_LONG("  IoErr", err);
    return 0;
}
