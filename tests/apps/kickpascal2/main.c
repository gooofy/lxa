/*
 * Test: apps/kickpascal2
 *
 * Automated test for KickPascal 2 IDE compatibility.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfxbase.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

int main(void)
{
    BPTR kp_seg = 0;
    struct Process *kp_proc = NULL;

    print("=== KickPascal 2 Automated Compatibility Test ===\n\n");

    /* Open required libraries */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase) {
        print("FAIL: Cannot open intuition.library\n");
        return 20;
    }

    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    if (!GfxBase) {
        print("FAIL: Cannot open graphics.library\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }

    /* Load KP2 from APPS: assign */
    kp_seg = LoadSeg((STRPTR)"APPS:KP2/KP");
    if (!kp_seg) {
        print("FAIL: Cannot load KP2 from APPS:KP2/KP\n");
        goto cleanup;
    }
    print("OK: KP2 binary loaded successfully\n");

    /* Create the background process */
    BPTR nilIn = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR nilOut = Open((STRPTR)"NIL:", MODE_NEWFILE);
    
    struct TagItem procTags[] = {
        { NP_Seglist, (ULONG)kp_seg },
        { NP_Name, (ULONG)"KickPascal" },
        { NP_StackSize, 16384 },
        { NP_Cli, TRUE },
        { NP_Input, (ULONG)nilIn },
        { NP_Output, (ULONG)nilOut },
        { NP_Arguments, (ULONG)"\n" },
        { NP_FreeSeglist, TRUE },
        { TAG_DONE, 0 }
    };
    
    kp_proc = CreateNewProc(procTags);
    if (!kp_proc) {
        print("FAIL: CreateNewProc failed\n");
        UnLoadSeg(kp_seg);
        if (nilIn) Close(nilIn);
        if (nilOut) Close(nilOut);
        goto cleanup;
    }
    
    print("OK: KP2 process created\n");
    print("PASS: KickPascal 2 launch test passed\n");

cleanup:
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);

    return 0;
}
