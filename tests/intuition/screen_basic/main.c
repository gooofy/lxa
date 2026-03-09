/*
 * Test: intuition/screen_basic
 * Tests OpenScreen() and CloseScreen() functions
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
    struct Screen *screen;
    struct Screen *tagged_screen = NULL;
    struct Screen *default_screen = NULL;
    struct Screen *locked_screen = NULL;
    struct Window *window = NULL;
    struct List *pub_list;
    struct PubScreenNode *pub_node;
    char default_name[MAXPUBSCREENNAME + 1];
    int errors = 0;

    print("Testing OpenScreen()/CloseScreen()...\n");

    /* Set up NewScreen structure */
    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = 320;
    ns.Height = 200;
    ns.Depth = 2;  /* 4 colors */
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.ViewModes = 0;
    ns.Type = CUSTOMSCREEN;
    ns.Font = NULL;
    ns.DefaultTitle = (UBYTE *)"Test Screen";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    /* Open the screen */
    screen = OpenScreen(&ns);

    if (screen == NULL) {
        print("FAIL: OpenScreen() returned NULL\n");
        return 20;
    }
    print("OK: OpenScreen() succeeded\n");

    /* Verify screen dimensions */
    if (screen->Width != 320) {
        print("FAIL: screen->Width != 320\n");
        errors++;
    } else {
        print("OK: screen->Width = 320\n");
    }

    if (screen->Height != 200) {
        print("FAIL: screen->Height != 200\n");
        errors++;
    } else {
        print("OK: screen->Height = 200\n");
    }

    /* Verify BitMap initialization */
    if (screen->BitMap.Depth != 2) {
        print("FAIL: BitMap.Depth != 2\n");
        errors++;
    } else {
        print("OK: BitMap.Depth = 2\n");
    }

    if (screen->BitMap.BytesPerRow != 40) {  /* 320/8 = 40 */
        print("FAIL: BitMap.BytesPerRow != 40\n");
        errors++;
    } else {
        print("OK: BitMap.BytesPerRow = 40\n");
    }

    if (screen->BitMap.Rows != 200) {
        print("FAIL: BitMap.Rows != 200\n");
        errors++;
    } else {
        print("OK: BitMap.Rows = 200\n");
    }

    /* Verify bitplanes are allocated */
    if (screen->BitMap.Planes[0] == NULL) {
        print("FAIL: BitMap.Planes[0] is NULL\n");
        errors++;
    } else {
        print("OK: BitMap.Planes[0] allocated\n");
    }

    if (screen->BitMap.Planes[1] == NULL) {
        print("FAIL: BitMap.Planes[1] is NULL\n");
        errors++;
    } else {
        print("OK: BitMap.Planes[1] allocated\n");
    }

    /* Verify RastPort is connected to BitMap */
    if (screen->RastPort.BitMap != &screen->BitMap) {
        print("FAIL: RastPort.BitMap not connected\n");
        errors++;
    } else {
        print("OK: RastPort.BitMap connected\n");
    }

    /* Verify ViewPort dimensions */
    if (screen->ViewPort.DWidth != 320) {
        print("FAIL: ViewPort.DWidth != 320\n");
        errors++;
    } else {
        print("OK: ViewPort.DWidth = 320\n");
    }

    if (screen->ViewPort.DHeight != 200) {
        print("FAIL: ViewPort.DHeight != 200\n");
        errors++;
    } else {
        print("OK: ViewPort.DHeight = 200\n");
    }

    /* Test drawing to the screen */
    SetAPen(&screen->RastPort, 1);
    RectFill(&screen->RastPort, 10, 10, 50, 50);
    
    /* Verify pixel was set */
    if (ReadPixel(&screen->RastPort, 30, 30) != 1) {
        print("FAIL: Drawing to screen failed\n");
        errors++;
    } else {
        print("OK: Drawing to screen works\n");
    }

    /* CloseScreen() must refuse to close screens with open windows */
    {
        struct NewWindow nw = {0};
        nw.LeftEdge = 10;
        nw.TopEdge = 10;
        nw.Width = 120;
        nw.Height = 60;
        nw.DetailPen = (UBYTE)-1;
        nw.BlockPen = (UBYTE)-1;
        nw.Flags = WFLG_BORDERLESS;
        nw.Title = (UBYTE *)"CloseScreen guard";
        nw.Screen = screen;
        nw.Type = CUSTOMSCREEN;

        window = OpenWindow(&nw);
        if (!window) {
            print("FAIL: OpenWindow() for CloseScreen test returned NULL\n");
            errors++;
        } else if (CloseScreen(screen) != FALSE) {
            print("FAIL: CloseScreen() should fail while a window is open\n");
            errors++;
        } else {
            print("OK: CloseScreen() refuses screens with open windows\n");
        }
    }

    if (window) {
        CloseWindow(window);
        window = NULL;
    }

    /* Public screen helpers must expose and lock the screen correctly */
    default_name[0] = '\0';
    GetDefaultPubScreen(default_name);
    if (default_name[0] == '\0') {
        print("FAIL: GetDefaultPubScreen() returned empty name\n");
        errors++;
    } else {
        print("OK: GetDefaultPubScreen() returned a name\n");
    }

    locked_screen = LockPubScreen((UBYTE *)default_name);
    if (locked_screen != screen) {
        print("FAIL: LockPubScreen() did not return the opened screen\n");
        errors++;
    } else if (CloseScreen(screen) != FALSE) {
        print("FAIL: CloseScreen() should fail while the screen is locked\n");
        errors++;
    } else {
        print("OK: LockPubScreen() prevents CloseScreen()\n");
    }

    pub_list = LockPubScreenList();
    if (!pub_list) {
        print("FAIL: LockPubScreenList() returned NULL\n");
        errors++;
    } else {
        pub_node = (struct PubScreenNode *)pub_list->lh_Head;
        if (!pub_node || !pub_node->psn_Node.ln_Succ || pub_node->psn_Screen != screen) {
            print("FAIL: LockPubScreenList() did not expose the public screen node\n");
            errors++;
        } else {
            print("OK: LockPubScreenList() exposes the public screen node\n");
        }
        UnlockPubScreenList();
    }

    if (PubScreenStatus(screen, PSNF_PRIVATE) != 0) {
        print("FAIL: PubScreenStatus() privatized a locked screen\n");
        errors++;
    } else {
        print("OK: PubScreenStatus() refuses to privatize a locked screen\n");
    }

    UnlockPubScreen(NULL, locked_screen);
    locked_screen = NULL;

    if (PubScreenStatus(screen, PSNF_PRIVATE) != 0) {
        print("FAIL: PubScreenStatus() returned wrong old flags when privatizing\n");
        errors++;
    } else if (LockPubScreen((UBYTE *)default_name) != NULL) {
        print("FAIL: LockPubScreen() should fail for a private screen\n");
        errors++;
    } else {
        print("OK: PubScreenStatus() can privatize the screen\n");
    }

    if (PubScreenStatus(screen, 0) != PSNF_PRIVATE) {
        print("FAIL: PubScreenStatus() did not report prior private state\n");
        errors++;
    } else {
        locked_screen = LockPubScreen((UBYTE *)default_name);
        if (locked_screen != screen) {
            print("FAIL: LockPubScreen() did not restore access after making screen public\n");
            errors++;
        } else {
            print("OK: PubScreenStatus() restores public-screen access\n");
        }
    }

    if (locked_screen) {
        UnlockPubScreen(NULL, locked_screen);
        locked_screen = NULL;
    }

    ShowTitle(screen, FALSE);
    if (screen->Flags & SHOWTITLE) {
        print("FAIL: ShowTitle(FALSE) did not clear SHOWTITLE\n");
        errors++;
    } else {
        print("OK: ShowTitle(FALSE) clears SHOWTITLE\n");
    }

    ShowTitle(screen, TRUE);
    if (!(screen->Flags & SHOWTITLE)) {
        print("FAIL: ShowTitle(TRUE) did not set SHOWTITLE\n");
        errors++;
    } else {
        print("OK: ShowTitle(TRUE) sets SHOWTITLE\n");
    }

    /* OpenScreenTagList() must honor tag overrides and allow clearing flags */
    {
        static UBYTE tag_title[] = "Tagged Screen";
        struct NewScreen base = {0};
        struct TagItem tags[] = {
            { SA_Width, 640 },
            { SA_Height, 256 },
            { SA_Depth, 3 },
            { SA_Title, (ULONG)tag_title },
            { SA_Behind, FALSE },
            { SA_Quiet, FALSE },
            { SA_ShowTitle, FALSE },
            { SA_AutoScroll, FALSE },
            { TAG_DONE, 0 }
        };

        base.Width = 160;
        base.Height = 100;
        base.Depth = 1;
        base.DetailPen = 0;
        base.BlockPen = 1;
        base.Type = CUSTOMSCREEN | SCREENBEHIND | SCREENQUIET | SHOWTITLE | AUTOSCROLL;
        base.DefaultTitle = (UBYTE *)"Base Screen";

        tagged_screen = OpenScreenTagList(&base, tags);
        if (!tagged_screen) {
            print("FAIL: OpenScreenTagList() returned NULL\n");
            errors++;
        } else {
            if (tagged_screen->Width != 640 || tagged_screen->Height != 256 || tagged_screen->BitMap.Depth != 3) {
                print("FAIL: OpenScreenTagList() did not apply size/depth tags\n");
                errors++;
            } else if (tagged_screen->Title != tag_title) {
                print("FAIL: OpenScreenTagList() did not apply title tag\n");
                errors++;
            } else if ((tagged_screen->Flags & (SCREENBEHIND | SCREENQUIET | SHOWTITLE | AUTOSCROLL)) != 0) {
                print("FAIL: OpenScreenTagList() did not clear boolean tags\n");
                errors++;
            } else {
                print("OK: OpenScreenTagList() applies and clears screen tags\n");
            }
        }
    }

    if (tagged_screen) {
        if (!CloseScreen(tagged_screen)) {
            print("FAIL: CloseScreen(tagged_screen) failed\n");
            errors++;
        }
        tagged_screen = NULL;
    }

    /* OpenScreenTagList() also supports a NULL NewScreen with tag-only defaults */
    {
        struct TagItem tags[] = {
            { SA_Width, 320 },
            { SA_Height, 40 },
            { SA_Depth, 2 },
            { TAG_DONE, 0 }
        };

        default_screen = OpenScreenTagList(NULL, tags);
        if (!default_screen) {
            print("FAIL: OpenScreenTagList(NULL, tags) returned NULL\n");
            errors++;
        } else if (default_screen->Width != 320 || default_screen->Height != 256 || default_screen->BitMap.Depth != 2) {
            print("FAIL: OpenScreenTagList(NULL, tags) did not apply defaults/height expansion\n");
            errors++;
        } else {
            print("OK: OpenScreenTagList(NULL, tags) uses defaults and height expansion\n");
        }
    }

    if (default_screen) {
        if (!CloseScreen(default_screen)) {
            print("FAIL: CloseScreen(default_screen) failed\n");
            errors++;
        }
        default_screen = NULL;
    }

    /* Close the screen */
    if (!CloseScreen(screen)) {
        print("FAIL: CloseScreen() failed after windows were closed\n");
        errors++;
    } else {
        print("OK: CloseScreen() completed\n");
    }

    /* Final result */
    if (errors == 0) {
        print("PASS: screen_basic all tests passed\n");
        return 0;
    } else {
        print("FAIL: screen_basic had errors\n");
        return 20;
    }
}
