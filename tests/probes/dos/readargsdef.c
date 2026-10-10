/*
 * Probe (Phase 237d): ReadArgs() leaves the array entries of arguments
 * that are not given alone, so a program can put its defaults there first.
 * Fred Fish Leggi (disk 669) defaults its PortName/K to "LEGGI" that way;
 * lxa cleared the entry and Leggi named its ARexx port with NULL.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static void t(const char *line)
{
    struct RDArgs *rda = AllocDosObject(DOS_RDARGS, NULL);
    LONG arg[4];
    static char buf[64];
    int i;

    arg[0] = (LONG)"default-name";
    arg[1] = 0x12345678;
    arg[2] = 0x2468ace0;
    arg[3] = 0x13579bdf;
    for (i = 0; line[i] && i < 62; i++)
        buf[i] = line[i];
    buf[i++] = '\n';
    buf[i] = 0;
    rda->RDA_Source.CS_Buffer = (UBYTE *)buf;
    rda->RDA_Source.CS_Length = i;
    rda->RDA_Source.CS_CurChr = 0;
    probe_s("\"");
    probe_s(line);
    probe_s("\": ");
    if (!ReadArgs((STRPTR)"NAME/K,QUIET/S,NUM/K/N,FILES/M", arg, rda)) {
        probe_s("fails\n");
        FreeDosObject(DOS_RDARGS, rda);
        return;
    }
    probe_s("NAME ");
    probe_s(arg[0] == 0 ? "NULL" : (char *)arg[0]);
    probe_s(", QUIET ");
    probe_s(arg[1] == 0x12345678 ? "untouched" : arg[1] ? "TRUE" : "0");
    probe_s(", NUM ");
    probe_s(arg[2] == 0x2468ace0 ? "untouched" : arg[2] ? "set" : "NULL");
    probe_s(", FILES ");
    probe_s(arg[3] == 0x13579bdf ? "untouched" : arg[3] ? "set" : "NULL");
    probe_ch('\n');
    FreeArgs(rda);
    FreeDosObject(DOS_RDARGS, rda);
}

int main(void)
{
    t("");
    t("QUIET");
    t("NAME=xyz NUM=3");
    t("a b");
    return 0;
}
