/*
 * GadTools DrawBevelBoxA Test
 * 
 * Tests GadTools bevel box rendering:
 * - GetVisualInfoA/FreeVisualInfo
 * - DrawBevelBoxA with various options
 * - GTBB_Recessed, GTBB_FrameType tags
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/graphics_protos.h>
#include <stdio.h>

struct Library *GadToolsBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

int main(void)
{
    struct Screen *scr;
    struct Window *win;
    struct RastPort *rp;
    APTR vi;
    struct TagItem tags[3];
    
    printf("GadTools DrawBevelBoxA Test\n");
    printf("===========================\n\n");
    
    /* Open libraries */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 36);
    if (!IntuitionBase) {
        printf("FAIL: Cannot open intuition.library\n");
        return 1;
    }
    
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 36);
    if (!GfxBase) {
        printf("FAIL: Cannot open graphics.library\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    
    GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 36);
    if (!GadToolsBase) {
        printf("FAIL: Cannot open gadtools.library\n");
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    printf("Opened gadtools.library v%d\n", GadToolsBase->lib_Version);
    
    /* Get default public screen */
    scr = LockPubScreen(NULL);
    if (!scr) {
        printf("FAIL: Cannot lock public screen\n");
        CloseLibrary(GadToolsBase);
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    printf("Locked public screen\n");
    
    /* Test 1: GetVisualInfoA */
    printf("\nTest 1: GetVisualInfoA...\n");
    vi = GetVisualInfoA(scr, NULL);
    if (!vi) {
        printf("FAIL: GetVisualInfoA returned NULL\n");
        UnlockPubScreen(NULL, scr);
        CloseLibrary(GadToolsBase);
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    printf("  Got VisualInfo\n");
    printf("  PASS: GetVisualInfoA works\n");
    
    /* Open a window for testing */
    win = OpenWindowTags(NULL,
        WA_Left, 50,
        WA_Top, 50,
        WA_Width, 200,
        WA_Height, 150,
        WA_Title, (ULONG)"BevelBox Test",
        WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE,
        WA_IDCMP, IDCMP_CLOSEWINDOW,
        TAG_DONE);
    
    if (!win) {
        printf("FAIL: Cannot open window\n");
        FreeVisualInfo(vi);
        UnlockPubScreen(NULL, scr);
        CloseLibrary(GadToolsBase);
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    printf("Opened test window\n");
    
    rp = win->RPort;
    
    /* Test 2: DrawBevelBoxA - raised */
    printf("\nTest 2: DrawBevelBoxA (raised)...\n");
    tags[0].ti_Tag = GT_VisualInfo;
    tags[0].ti_Data = (ULONG)vi;
    tags[1].ti_Tag = TAG_DONE;
    
    DrawBevelBoxA(rp, 10, 20, 80, 40, tags);
    printf("  Drew raised bevel box at (10,20) size 80x40\n");
    printf("  PASS: DrawBevelBoxA (raised) completed\n");
    
    /* Test 3: DrawBevelBoxA - recessed */
    printf("\nTest 3: DrawBevelBoxA (recessed)...\n");
    tags[0].ti_Tag = GT_VisualInfo;
    tags[0].ti_Data = (ULONG)vi;
    tags[1].ti_Tag = GTBB_Recessed;
    tags[1].ti_Data = TRUE;
    tags[2].ti_Tag = TAG_DONE;
    
    DrawBevelBoxA(rp, 100, 20, 80, 40, tags);
    printf("  Drew recessed bevel box at (100,20) size 80x40\n");
    printf("  PASS: DrawBevelBoxA (recessed) completed\n");
    
    /* Test 4: DrawBevelBoxA - BBFT_RIDGE frame type */
    printf("\nTest 4: DrawBevelBoxA (BBFT_RIDGE)...\n");
    tags[0].ti_Tag = GT_VisualInfo;
    tags[0].ti_Data = (ULONG)vi;
    tags[1].ti_Tag = GTBB_FrameType;
    tags[1].ti_Data = BBFT_RIDGE;
    tags[2].ti_Tag = TAG_DONE;
    
    DrawBevelBoxA(rp, 10, 80, 80, 40, tags);
    printf("  Drew ridge bevel box at (10,80) size 80x40\n");
    printf("  PASS: DrawBevelBoxA (BBFT_RIDGE) completed\n");
    
    /* Clean up */
    printf("\nTest 5: Cleanup...\n");
    CloseWindow(win);
    printf("  Closed window\n");
    
    FreeVisualInfo(vi);
    printf("  Freed VisualInfo\n");
    
    UnlockPubScreen(NULL, scr);
    printf("  Unlocked screen\n");
    
    printf("  PASS: Cleanup completed\n");
    
    CloseLibrary(GadToolsBase);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    
    printf("\nPASS\n");
    return 0;
}
