/*
 * LibIdent <name.library> - print the identity of a library (Phase 235).
 * Used to verify LXA_OVERRIDE: with the override the user's AmigaOS 3.1
 * binary answers, without it lxa's implementation.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

int main(int argc, char **argv)
{
    struct Library *lib;
    const char *name = argc > 1 ? argv[1] : "iffparse.library";
    ULONG args[3];

    lib = OpenLibrary((STRPTR)name, 0);
    if (!lib) {
        Printf((STRPTR)"%s: cannot open\n", (ULONG)name);
        return 20;
    }
    args[0] = (ULONG)name;
    args[1] = lib->lib_Version;
    args[2] = lib->lib_Revision;
    VPrintf((STRPTR)"%s %lu.%lu ", args);
    Printf((STRPTR)"id=%s ", (ULONG)(lib->lib_IdString ? (char *)lib->lib_IdString : "(none)"));
    /* where Open() (LVO -6: jmp abs.l) lives: lxa's built-in libraries are
     * in its ROM, a library loaded from LIBS: is in RAM */
    {
        ULONG open = *(ULONG *)((UBYTE *)lib - 4);
        Printf((STRPTR)"code=%s\n", (ULONG)(open >= 0xf80000 && open < 0x1000000 ? "rom" : "ram"));
    }
    CloseLibrary(lib);
    return 0;
}
