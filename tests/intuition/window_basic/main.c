/*
 * Test: intuition/window_basic
 * Tests OpenWindow() and CloseWindow() functions
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
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
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct Window *tag_window;
    int errors = 0;

    print("Testing OpenWindow()/CloseWindow()...\n");

    /* First open a screen for the window */
    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = 320;
    ns.Height = 200;
    ns.Depth = 2;
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.ViewModes = 0;
    ns.Type = CUSTOMSCREEN;
    ns.Font = NULL;
    ns.DefaultTitle = (UBYTE *)"Test Screen";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (screen == NULL) {
        print("FAIL: Could not open screen for window test\n");
        return 20;
    }
    print("OK: Screen opened for window test\n");

    /* Set up NewWindow structure */
    nw.LeftEdge = 10;
    nw.TopEdge = 15;
    nw.Width = 200;
    nw.Height = 100;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_ACTIVATE;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"Test Window";
    nw.Screen = screen;
    nw.BitMap = NULL;
    nw.MinWidth = 50;
    nw.MinHeight = 30;
    nw.MaxWidth = 300;
    nw.MaxHeight = 200;
    nw.Type = CUSTOMSCREEN;

    /* Open the window */
    window = OpenWindow(&nw);

    if (window == NULL) {
        print("FAIL: OpenWindow() returned NULL\n");
        CloseScreen(screen);
        return 20;
    }
    print("OK: OpenWindow() succeeded\n");

    /* Verify window position */
    if (window->LeftEdge != 10) {
        print("FAIL: window->LeftEdge != 10\n");
        errors++;
    } else {
        print("OK: window->LeftEdge = 10\n");
    }

    if (window->TopEdge != 15) {
        print("FAIL: window->TopEdge != 15\n");
        errors++;
    } else {
        print("OK: window->TopEdge = 15\n");
    }

    /* Verify window dimensions */
    if (window->Width != 200) {
        print("FAIL: window->Width != 200\n");
        errors++;
    } else {
        print("OK: window->Width = 200\n");
    }

    if (window->Height != 100) {
        print("FAIL: window->Height != 100\n");
        errors++;
    } else {
        print("OK: window->Height = 100\n");
    }

    /* Verify screen linkage */
    if (window->WScreen != screen) {
        print("FAIL: window->WScreen not linked to screen\n");
        errors++;
    } else {
        print("OK: window->WScreen linked correctly\n");
    }

    /* Verify screen's window list */
    if (screen->FirstWindow != window) {
        print("FAIL: screen->FirstWindow not pointing to window\n");
        errors++;
    } else {
        print("OK: screen->FirstWindow linked correctly\n");
    }

    /* Verify RastPort linkage */
    if (window->RPort == NULL) {
        print("FAIL: window->RPort is NULL\n");
        errors++;
    } else {
        print("OK: window->RPort is set\n");
    }

    /* Verify IDCMP port was created */
    if (window->UserPort == NULL) {
        print("FAIL: window->UserPort is NULL (IDCMP not set up)\n");
        errors++;
    } else {
        print("OK: window->UserPort created (IDCMP active)\n");
    }

    /* Verify IDCMP flags */
    if (!(window->IDCMPFlags & IDCMP_CLOSEWINDOW)) {
        print("FAIL: IDCMP_CLOSEWINDOW not set\n");
        errors++;
    } else {
        print("OK: IDCMP_CLOSEWINDOW set\n");
    }

    /* Verify window is active */
    if (!(window->Flags & WFLG_WINDOWACTIVE)) {
        print("FAIL: Window not activated\n");
        errors++;
    } else {
        print("OK: Window is active\n");
    }

    /* Verify borders are set (non-borderless window) */
    if (window->BorderTop == 0) {
        print("FAIL: window->BorderTop is 0 (should have title bar)\n");
        errors++;
    } else {
        print("OK: window->BorderTop set for title bar\n");
    }

    /* Verify min/max sizes */
    if (window->MinWidth != 50 || window->MinHeight != 30) {
        print("FAIL: Min sizes not set correctly\n");
        errors++;
    } else {
        print("OK: Min sizes set correctly\n");
    }

    /* Close the window */
    CloseWindow(window);
    print("OK: CloseWindow() completed\n");

    /* Verify window was unlinked from screen */
    if (screen->FirstWindow != NULL) {
        print("FAIL: screen->FirstWindow not cleared after close\n");
        errors++;
    } else {
        print("OK: Window unlinked from screen\n");
    }

    /* OpenWindowTagList() must apply tag overrides and clear boolean flags */
    {
        struct NewWindow base = {0};
        struct TagItem tags[] = {
            { WA_CustomScreen, (ULONG)screen },
            { WA_Left, 20 },
            { WA_Top, 25 },
            { WA_Width, 180 },
            { WA_Height, 90 },
            { WA_Borderless, FALSE },
            { WA_Activate, FALSE },
            { WA_CloseGadget, TRUE },
            { WA_DepthGadget, TRUE },
            { WA_DragBar, FALSE },
            { TAG_DONE, 0 }
        };

        base.LeftEdge = 1;
        base.TopEdge = 2;
        base.Width = 50;
        base.Height = 40;
        base.DetailPen = (UBYTE)-1;
        base.BlockPen = (UBYTE)-1;
        base.Flags = WFLG_BORDERLESS | WFLG_ACTIVATE | WFLG_DRAGBAR;
        base.Title = (UBYTE *)"Base Window";
        base.Screen = screen;
        base.Type = CUSTOMSCREEN;

        tag_window = OpenWindowTagList(&base, tags);
        if (!tag_window) {
            print("FAIL: OpenWindowTagList() override window returned NULL\n");
            errors++;
        } else if (tag_window->LeftEdge != 20 || tag_window->TopEdge != 25 ||
                   tag_window->Width != 180 || tag_window->Height != 90) {
            print("FAIL: OpenWindowTagList() did not apply position/size tags\n");
            errors++;
        } else if (tag_window->Flags & WFLG_BORDERLESS) {
            print("FAIL: OpenWindowTagList() did not clear WA_Borderless\n");
            errors++;
        } else if (tag_window->Flags & WFLG_WINDOWACTIVE) {
            print("FAIL: OpenWindowTagList() did not clear WA_Activate\n");
            errors++;
        } else if (!(tag_window->Flags & WFLG_CLOSEGADGET) ||
                   !(tag_window->Flags & WFLG_DEPTHGADGET) ||
                   (tag_window->Flags & WFLG_DRAGBAR)) {
            print("FAIL: OpenWindowTagList() did not apply boolean gadget flags\n");
            errors++;
        } else if (tag_window->BorderTop == 0) {
            print("FAIL: OpenWindowTagList() did not restore bordered window frame\n");
            errors++;
        } else {
            print("OK: OpenWindowTagList() applies overrides and clears boolean tags\n");
        }

        if (tag_window) {
            CloseWindow(tag_window);
            tag_window = NULL;
        }
    }

    /* WA_InnerWidth/Height, WA_GimmeZeroZero, and size-border tags must agree */
    {
        static UBYTE inner_title[] = "Inner Window";
        struct TagItem tags[] = {
            { WA_CustomScreen, (ULONG)screen },
            { WA_Left, 12 },
            { WA_Top, 18 },
            { WA_Title, (ULONG)inner_title },
            { WA_SizeGadget, TRUE },
            { WA_SizeBBottom, TRUE },
            { WA_SizeBRight, FALSE },
            { WA_GimmeZeroZero, TRUE },
            { WA_InnerWidth, 100 },
            { WA_InnerHeight, 40 },
            { TAG_DONE, 0 }
        };

        tag_window = OpenWindowTagList(NULL, tags);
        if (!tag_window) {
            print("FAIL: OpenWindowTagList() inner/GZZ window returned NULL\n");
            errors++;
        } else if (tag_window->WScreen != screen) {
            print("FAIL: OpenWindowTagList() did not use WA_CustomScreen\n");
            errors++;
        } else if (!(tag_window->Flags & WFLG_GIMMEZEROZERO)) {
            print("FAIL: OpenWindowTagList() did not set WA_GimmeZeroZero\n");
            errors++;
        } else if (tag_window->GZZWidth != 100 || tag_window->GZZHeight != 40) {
            print("FAIL: OpenWindowTagList() did not preserve inner dimensions in GZZ fields\n");
            errors++;
        } else if (tag_window->BorderBottom <= screen->WBorBottom ||
                   tag_window->BorderRight != screen->WBorRight) {
            print("FAIL: OpenWindowTagList() size border placement is wrong\n");
            errors++;
        } else {
            print("OK: OpenWindowTagList() handles inner size, GZZ, and size-border tags\n");
        }

        if (tag_window) {
            CloseWindow(tag_window);
            tag_window = NULL;
        }
    }

    /* OpenWindowTagList(NULL, tags) must default to Workbench when no screen is supplied */
    {
        struct TagItem tags[] = {
            { WA_Left, 3 },
            { WA_Top, 4 },
            { WA_Borderless, TRUE },
            { WA_InnerWidth, 80 },
            { WA_InnerHeight, 30 },
            { TAG_DONE, 0 }
        };

        tag_window = OpenWindowTagList(NULL, tags);
        if (!tag_window) {
            print("FAIL: OpenWindowTagList(NULL, tags) returned NULL\n");
            errors++;
        } else if (!tag_window->WScreen || !(tag_window->WScreen->Flags & WBENCHSCREEN)) {
            print("FAIL: OpenWindowTagList(NULL, tags) did not open on Workbench\n");
            errors++;
        } else if (tag_window->Width != 80 || tag_window->Height != 30) {
            print("FAIL: OpenWindowTagList(NULL, tags) did not honor borderless inner size\n");
            errors++;
        } else {
            print("OK: OpenWindowTagList(NULL, tags) opens on Workbench defaults\n");
        }

        if (tag_window) {
            CloseWindow(tag_window);
            tag_window = NULL;
        }
    }

    /* WA_Zoom data must drive ZipWindow() toggling */
    {
        WORD zoom_box[4] = { 40, 50, 140, 70 };
        struct TagItem tags[] = {
            { WA_CustomScreen, (ULONG)screen },
            { WA_Left, 10 },
            { WA_Top, 12 },
            { WA_Width, 120 },
            { WA_Height, 60 },
            { WA_Zoom, (ULONG)zoom_box },
            { TAG_DONE, 0 }
        };

        tag_window = OpenWindowTagList(NULL, tags);
        if (!tag_window) {
            print("FAIL: OpenWindowTagList() WA_Zoom window returned NULL\n");
            errors++;
        } else {
            ZipWindow(tag_window);
            if (tag_window->LeftEdge != 40 || tag_window->TopEdge != 50 ||
                tag_window->Width != 140 || tag_window->Height != 70) {
                print("FAIL: ZipWindow() did not apply WA_Zoom coordinates\n");
                errors++;
            } else {
                ZipWindow(tag_window);
                if (tag_window->LeftEdge != 10 || tag_window->TopEdge != 12 ||
                    tag_window->Width != 120 || tag_window->Height != 60) {
                    print("FAIL: ZipWindow() did not restore original window box\n");
                    errors++;
                } else {
                    print("OK: WA_Zoom data drives ZipWindow() toggling\n");
                }
            }
        }

        if (tag_window) {
            CloseWindow(tag_window);
            tag_window = NULL;
        }
    }

    /* Close the screen */
    CloseScreen(screen);
    print("OK: Screen closed\n");

    /* Final result */
    if (errors == 0) {
        print("PASS: window_basic all tests passed\n");
        return 0;
    } else {
        print("FAIL: window_basic had errors\n");
        return 20;
    }
}
