/*
 * Probe (Phase 237): the full 32-bit d0 of library functions declared to
 * return BOOL (16 bits).  d0 is preloaded with garbage; old programs test
 * d0 as a LONG.
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <workbench/workbench.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/gadtools.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct IntuitionBase *IntuitionBase;
struct Library *GadToolsBase, *IconBase;

static LONG call(APTR base, LONG lvo, APTR a0, APTR a1, APTR a2)
{
    register LONG r_d0 __asm("d0") = 0x12345678;
    register APTR r_a0 __asm("a0") = a0;
    register APTR r_a1 __asm("a1") = a1;
    register APTR r_a2 __asm("a2") = a2;
    register APTR r_a3 __asm("a3") = (APTR)((UBYTE *)base + lvo);
    register APTR r_d1 __asm("d1") = base;
    __asm__ __volatile__ ("move.l a6,-(sp)\n\tmove.l d1,a6\n\tjsr (a3)\n\tmove.l (sp)+,a6"
                          : "+r" (r_d0), "+r" (r_d1), "+r" (r_a0), "+r" (r_a1), "+r" (r_a2), "+r" (r_a3)
                          : : "cc", "memory");
    return r_d0;
}

static struct NewMenu nm[] = {
    { NM_TITLE, (STRPTR)"Project", 0, 0, 0, 0 },
    { NM_ITEM, (STRPTR)"Quit", (STRPTR)"Q", 0, 0, 0 },
    { NM_END, 0, 0, 0, 0, 0 }
};

int main(void)
{
    struct Screen *scr;
    struct Window *win;
    struct Menu *menu;
    APTR vi;
    struct DiskObject dobj;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 37);
    IconBase = OpenLibrary((STRPTR)"icon.library", 37);
    if (!IntuitionBase || !GadToolsBase || !IconBase)
        return 20;
    scr = LockPubScreen(NULL);
    win = OpenWindowTags(NULL, WA_Width, 200, WA_Height, 60, WA_Title, (ULONG)"narrowret", TAG_END);
    vi = GetVisualInfoA(scr, NULL);
    menu = CreateMenusA(nm, NULL);
    if (!win || !vi || !menu)
        return 20;
    P_HEX("LayoutMenusA d0", call(GadToolsBase, -66, menu, vi, NULL));
    P_HEX("SetMenuStrip d0", call(IntuitionBase, -264, win, menu, NULL));
    P_HEX("ResetMenuStrip d0", call(IntuitionBase, -702, win, menu, NULL));
    ClearMenuStrip(win);
    P_HEX("ActivateGadget(none) d0", call(IntuitionBase, -462, NULL, win, NULL));
    dobj.do_Magic = 0;          /* invalid: PutDiskObject fails */
    P_HEX("PutDiskObject(fail) d0", call(IconBase, -84, (APTR)"RAM:nonexistent-dir/x", &dobj, NULL));
    FreeMenus(menu);
    FreeVisualInfo(vi);
    CloseWindow(win);
    UnlockPubScreen(NULL, scr);
    CloseLibrary(IconBase);
    CloseLibrary(GadToolsBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
