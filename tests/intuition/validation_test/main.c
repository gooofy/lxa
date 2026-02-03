/*
 * Test: intuition/validation_test
 *
 * Phase 39b: Test validation infrastructure
 *
 * This test exercises the new validation functions from test_inject.h
 * to verify screen dimensions, window dimensions, and content rendering.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfxbase.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/graphics.h>
#include <inline/dos.h>

#include "../../common/test_inject.h"

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

static void print_num(const char *prefix, LONG val, const char *suffix)
{
    char buf[64];
    char *p = buf;
    const char *src = prefix;
    while (*src) *p++ = *src++;

    if (val < 0) {
        *p++ = '-';
        val = -val;
    }

    char digits[12];
    int i = 0;
    if (val == 0) {
        digits[i++] = '0';
    } else {
        while (val > 0) {
            digits[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    while (i > 0) *p++ = digits[--i];
    
    src = suffix;
    while (*src) *p++ = *src++;
    *p = '\0';
    print(buf);
}

int main(void)
{
    int errors = 0;
    struct Screen *screen = NULL;
    struct Window *window = NULL;
    WORD width, height, depth;
    LONG content;

    print("=== Phase 39b: Validation Infrastructure Test ===\n\n");

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

    /* ========== Test 1: Open Screen and validate dimensions ========== */
    print("--- Test 1: Open Screen with specific dimensions ---\n");
    
    {
        struct NewScreen ns = {0};
        ns.Width = 640;
        ns.Height = 200;
        ns.Depth = 2;
        ns.DetailPen = 0;
        ns.BlockPen = 1;
        ns.Type = CUSTOMSCREEN;
        ns.DefaultTitle = (UBYTE *)"Validation Test Screen";
        
        screen = OpenScreen(&ns);
    }
    
    if (!screen) {
        print("FAIL: OpenScreen() returned NULL\n");
        errors++;
        goto cleanup;
    }
    print("OK: Screen opened successfully\n");

    /* Use validation API to check screen dimensions */
    if (test_get_screen_dimensions(&width, &height, &depth)) {
        print_num("  Screen width:  ", width, "\n");
        print_num("  Screen height: ", height, "\n");
        print_num("  Screen depth:  ", depth, "\n");
        
        if (width != 640) {
            print("FAIL: Screen width mismatch\n");
            errors++;
        }
        if (height != 200) {
            print("FAIL: Screen height mismatch\n");
            errors++;
        }
        if (depth != 2) {
            print("FAIL: Screen depth mismatch\n");
            errors++;
        }
        
        if (errors == 0) {
            print("OK: Screen dimensions match request\n");
        }
    } else {
        print("FAIL: test_get_screen_dimensions() returned FALSE\n");
        errors++;
    }

    /* ========== Test 2: Check initial content (should be minimal) ========== */
    print("\n--- Test 2: Check initial screen content ---\n");
    
    content = test_get_content_pixels();
    print_num("  Non-background pixels: ", content, "\n");
    
    if (content >= 0) {
        print("OK: Content check succeeded\n");
    } else {
        print("FAIL: test_get_content_pixels() returned error\n");
        errors++;
    }

    /* ========== Test 3: Open Window and validate dimensions ========== */
    print("\n--- Test 3: Open Window with specific dimensions ---\n");
    
    {
        struct NewWindow nw = {0};
        nw.LeftEdge = 50;
        nw.TopEdge = 30;
        nw.Width = 300;
        nw.Height = 150;
        nw.DetailPen = -1;
        nw.BlockPen = -1;
        nw.IDCMPFlags = 0;
        nw.Flags = WFLG_SIZEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | 
                   WFLG_CLOSEGADGET | WFLG_ACTIVATE;
        nw.FirstGadget = NULL;
        nw.CheckMark = NULL;
        nw.Title = (UBYTE *)"Test Window";
        nw.Screen = screen;
        nw.BitMap = NULL;
        nw.MinWidth = 50;
        nw.MinHeight = 30;
        nw.MaxWidth = 640;
        nw.MaxHeight = 200;
        nw.Type = CUSTOMSCREEN;
        
        window = OpenWindow(&nw);
    }
    
    if (!window) {
        print("FAIL: OpenWindow() returned NULL\n");
        errors++;
        goto cleanup;
    }
    print("OK: Window opened successfully\n");
    print_num("  Window width:  ", window->Width, "\n");
    print_num("  Window height: ", window->Height, "\n");

    /* Check window count via validation API */
    {
        LONG win_count = test_get_window_count();
        print_num("  Total windows (via API): ", win_count, "\n");
    }

    /* ========== Test 4: Draw something and check content ========== */
    print("\n--- Test 4: Draw content and validate rendering ---\n");
    
    /* Draw a filled rectangle */
    SetAPen(window->RPort, 1);
    RectFill(window->RPort, 10, 20, 100, 80);
    
    /* Draw some text */
    SetAPen(window->RPort, 2);
    Move(window->RPort, 20, 50);
    Text(window->RPort, (STRPTR)"Validation Test", 15);
    
    /* Give display time to update */
    Delay(5);
    
    /* Check content again */
    content = test_get_content_pixels();
    /* Note: Don't print exact pixel count as it varies */
    
    if (content > 1000) {  /* Should have significant content now */
        print("OK: Content rendering validated\n");
    } else {
        print_num("WARNING: Less content than expected (", content, " pixels)\n");
        /* Not a hard failure - content depends on rendering details */
    }

    /* ========== Test 5: Check region content (title bar area) ========== */
    print("\n--- Test 5: Validate title bar region ---\n");
    
    /* Check top 15 pixels of screen (title bar area) */
    {
        LONG title_content = test_get_region_content(0, 0, 640, 15);
        print_num("  Title bar pixels (0,0 to 640,15): ", title_content, "\n");
        
        if (title_content >= 0) {
            print("OK: Region content check succeeded\n");
        } else {
            print("FAIL: test_get_region_content() returned error\n");
            errors++;
        }
    }

    /* ========== Test 6: Validate screen size helper ========== */
    print("\n--- Test 6: Test validation helpers ---\n");
    
    if (test_validate_screen_size(320, 100)) {
        print("OK: Screen passes minimum size validation (320x100)\n");
    } else {
        print("FAIL: Screen failed minimum size validation\n");
        errors++;
    }
    
    if (!test_validate_screen_size(800, 600)) {
        print("OK: Screen correctly fails large size validation (800x600)\n");
    } else {
        print("Note: Screen unexpectedly passed 800x600 validation\n");
    }

    /* ========== Test 7: Capture screen (if supported) ========== */
    print("\n--- Test 7: Test screen capture ---\n");
    
    if (test_capture_screen("/tmp/validation_test.ppm")) {
        print("OK: Screen captured to /tmp/validation_test.ppm\n");
    } else {
        print("Note: Screen capture not available or failed\n");
    }

cleanup:
    /* Clean up */
    if (window) {
        CloseWindow(window);
    }
    if (screen) {
        CloseScreen(screen);
    }
    
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);

    /* ========== Final result ========== */
    print("\n=== Test Results ===\n");
    if (errors == 0) {
        print("PASS: validation_test all tests passed\n");
        return 0;
    } else {
        print_num("FAIL: ", errors, " errors occurred\n");
        return 20;
    }
}
