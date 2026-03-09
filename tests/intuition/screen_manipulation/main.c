/*
 * Test: intuition/screen_manipulation
 * Tests ScreenToBack, ScreenToFront, ScreenDepth, ScreenPosition functions
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct IntuitionBase *IntuitionBase;

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
    struct NewScreen ns1, ns2;
    struct Screen *screen1, *screen2;
    struct Screen screen_copy;
    int errors = 0;
    
    print("Testing Screen Manipulation Functions...\n\n");
    
    /* Open first screen */
    ns1.LeftEdge = 0;
    ns1.TopEdge = 0;
    ns1.Width = 640;
    ns1.Height = 480;
    ns1.Depth = 2;
    ns1.DetailPen = 0;
    ns1.BlockPen = 1;
    ns1.ViewModes = 0;
    ns1.Type = CUSTOMSCREEN;
    ns1.Font = NULL;
    ns1.DefaultTitle = (UBYTE *)"Screen 1";
    ns1.Gadgets = NULL;
    ns1.CustomBitMap = NULL;
    
    screen1 = OpenScreen(&ns1);
    if (!screen1) {
        print("ERROR: Could not open screen1\n");
        return 1;
    }
    print("OK: Screen 1 opened\n");
    
    /* Open second screen */
    ns2.LeftEdge = 0;
    ns2.TopEdge = 0;
    ns2.Width = 320;
    ns2.Height = 200;
    ns2.Depth = 2;
    ns2.DetailPen = 0;
    ns2.BlockPen = 1;
    ns2.ViewModes = 0;
    ns2.Type = CUSTOMSCREEN;
    ns2.Font = NULL;
    ns2.DefaultTitle = (UBYTE *)"Screen 2";
    ns2.Gadgets = NULL;
    ns2.CustomBitMap = NULL;
    
    screen2 = OpenScreen(&ns2);
    if (!screen2) {
        print("ERROR: Could not open screen2\n");
        CloseScreen(screen1);
        return 1;
    }
    print("OK: Screen 2 opened\n\n");
    
    /* Test 1: ScreenToBack */
    print("Test 1: ScreenToBack(screen2)...\n");
    ScreenToBack(screen2);
    if (IntuitionBase->FirstScreen != screen1 || screen1->NextScreen != screen2 || screen2->NextScreen != NULL) {
        print("  FAIL: ScreenToBack did not move screen2 behind screen1\n\n");
        errors++;
    } else {
        print("  OK: ScreenToBack moves screen2 behind screen1\n\n");
    }
    
    /* Test 2: ScreenToFront */
    print("Test 2: ScreenToFront(screen2)...\n");
    ScreenToFront(screen2);
    if (IntuitionBase->FirstScreen != screen2 || screen2->NextScreen != screen1) {
        print("  FAIL: ScreenToFront did not restore screen2 to the front\n\n");
        errors++;
    } else {
        print("  OK: ScreenToFront restores screen2 to the front\n\n");
    }
    
    /* Test 3: ScreenDepth with SDEPTH_TOBACK */
    print("Test 3: ScreenDepth(screen1, SDEPTH_TOBACK, NULL)...\n");
    ScreenDepth(screen1, SDEPTH_TOBACK, NULL);
    if (IntuitionBase->FirstScreen != screen2 || screen2->NextScreen != screen1 || screen1->NextScreen != NULL) {
        print("  FAIL: ScreenDepth(TOBACK) did not keep screen1 at the back\n\n");
        errors++;
    } else {
        print("  OK: ScreenDepth(TOBACK) keeps screen1 at the back\n\n");
    }
    
    /* Test 4: ScreenDepth with SDEPTH_TOFRONT */
    print("Test 4: ScreenDepth(screen1, SDEPTH_TOFRONT, NULL)...\n");
    ScreenDepth(screen1, SDEPTH_TOFRONT, NULL);
    if (IntuitionBase->FirstScreen != screen1 || screen1->NextScreen != screen2) {
        print("  FAIL: ScreenDepth(TOFRONT) did not move screen1 to the front\n\n");
        errors++;
    } else {
        print("  OK: ScreenDepth(TOFRONT) moves screen1 to the front\n\n");
    }
    
    /* Test 5: ScreenPosition */
    print("Test 5: ScreenPosition(screen1, SPOS_RELATIVE, 10, 20, 0, 0)...\n");
    ScreenPosition(screen1, SPOS_RELATIVE, 10, 20, 0, 0);
    if (screen1->LeftEdge != 10 || screen1->TopEdge != 20) {
        print("  FAIL: ScreenPosition(SPOS_RELATIVE) did not update coordinates\n\n");
        errors++;
    } else {
        print("  OK: ScreenPosition(SPOS_RELATIVE) updates coordinates\n\n");
    }

    print("Test 6: MoveScreen(screen1, -4, 6)...\n");
    MoveScreen(screen1, -4, 6);
    if (screen1->LeftEdge != 6 || screen1->TopEdge != 26) {
        print("  FAIL: MoveScreen did not apply the requested delta\n\n");
        errors++;
    } else {
        print("  OK: MoveScreen applies relative movement\n\n");
    }

    print("Test 7: ScreenPosition(screen1, SPOS_ABSOLUTE, 3, 7, 0, 0)...\n");
    ScreenPosition(screen1, SPOS_ABSOLUTE, 3, 7, 0, 0);
    if (screen1->LeftEdge != 3 || screen1->TopEdge != 7) {
        print("  FAIL: ScreenPosition(SPOS_ABSOLUTE) did not set absolute coordinates\n\n");
        errors++;
    } else {
        print("  OK: ScreenPosition(SPOS_ABSOLUTE) sets absolute coordinates\n\n");
    }

    print("Test 8: GetScreenData(screen1)...\n");
    if (!GetScreenData(&screen_copy, sizeof(screen_copy), CUSTOMSCREEN, screen1)) {
        print("  FAIL: GetScreenData() returned FALSE for a live screen\n\n");
        errors++;
    } else if (screen_copy.LeftEdge != screen1->LeftEdge ||
               screen_copy.TopEdge != screen1->TopEdge ||
               screen_copy.Width != screen1->Width ||
               screen_copy.Height != screen1->Height ||
               screen_copy.Title != screen1->Title) {
        print("  FAIL: GetScreenData() did not copy the public screen fields\n\n");
        errors++;
    } else {
        print("  OK: GetScreenData() copies the screen structure\n\n");
    }
    
    /* Cleanup */
    CloseScreen(screen2);
    print("OK: Screen 2 closed\n");
    CloseScreen(screen1);
    print("OK: Screen 1 closed\n\n");
    
    if (errors == 0) {
        print("PASS: screen_manipulation all tests completed\n");
        return 0;
    }

    print("FAIL: screen_manipulation had errors\n");
    return 20;
}
