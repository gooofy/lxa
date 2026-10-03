/*
 * Probe (Phase 237): OpenLibrary() version comparison - which requested
 * versions open a library (Fred Fish AddPower asks for version -1).
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static void try(const char *name, ULONG version)
{
    struct Library *lib = OpenLibrary((STRPTR)name, version);
    probe_s(name);
    probe_s(" version ");
    probe_hex(version, 8);
    probe_s(lib ? " = opened\n" : " = NULL\n");
    if (lib)
        CloseLibrary(lib);
}

int main(void)
{
    static const ULONG versions[] = { 0, 37, 40, 41, 0x7fff, 0x8000, 0xffff, 0x10000, 0x10028,
                                      0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff };
    unsigned i;
    for (i = 0; i < sizeof(versions) / sizeof(versions[0]); i++)
        try("dos.library", versions[i]);
    for (i = 0; i < sizeof(versions) / sizeof(versions[0]); i++)
        try("amigaguide.library", versions[i]);
    return 0;
}
