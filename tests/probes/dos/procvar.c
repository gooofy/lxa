/*
 * Probe (Phase 237d): the shell's local variables a command sees.  Fred
 * Fish ShellTimer (disk 638) reads the local variable "process" (the CLI
 * number) and quits with "Can't get shell PID!" without it.  Prints which
 * of process, RC and Result2 a command run from the shell finds with
 * GetVar(GVF_LOCAL_ONLY) and whether process holds a CLI number.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static void show(const char *name)
{
    char buf[32];
    LONG n = GetVar((STRPTR)name, (STRPTR)buf, sizeof(buf), GVF_LOCAL_ONLY);
    probe_s(name);
    if (n < 0) {
        probe_s(": none\n");
        return;
    }
    probe_s(" = \"");
    probe_s(buf);
    probe_s("\"\n");
}

int main(void)
{
    struct LocalVar *lv;
    char buf[32];
    LONG n, v = 0, i;

    show("RC");
    show("Result2");
    n = GetVar((STRPTR)"process", (STRPTR)buf, sizeof(buf), GVF_LOCAL_ONLY);
    P_BOOL("process found", n >= 0);
    if (n >= 0) {
        for (i = 0; buf[i] >= '0' && buf[i] <= '9'; i++)
            v = v * 10 + buf[i] - '0';
        /* the number of the CLI the command was run from (the
         * reference's harness starts it with Run: not its own) */
        P_BOOL("process is a CLI number", v > 0 && i > 0);
    }
    lv = FindVar((STRPTR)"process", LV_VAR);
    P_BOOL("FindVar(process, LV_VAR)", lv != NULL);
    return 0;
}
