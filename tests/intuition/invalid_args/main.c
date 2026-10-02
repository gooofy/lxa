/*
 * Test: intuition/invalid_args
 * lxa's tolerance of invalid OpenWindow()/OpenScreen() arguments.
 *
 * Real AmigaOS 3.1 does not tolerate these (a negative WA_Height hangs the
 * machine, a bogus NW_EXTENDED Extension pointer is dereferenced, a
 * negative SA_Height other than STDSCREENHEIGHT fails), so this program is
 * lxa-only (tests/ref_suite.yaml).  lxa sanitizes such values instead of
 * wrapping them into huge unsigned sizes.
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;

static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;
    Write(Output(), (CONST APTR)s, len);
}

int main(void)
{
    struct NewScreen ns = {0};
    struct Screen *screen;
    struct Window *window;
    int errors = 0;

    print("Testing invalid argument tolerance...\n");

    ns.Width = 320;
    ns.Height = 200;
    ns.Depth = 2;
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.Type = CUSTOMSCREEN;
    ns.DefaultTitle = (UBYTE *)"Invalid Args";

    screen = OpenScreen(&ns);
    if (!screen) {
        print("FAIL: OpenScreen() returned NULL\n");
        return 20;
    }

    /* Negative WA_Height values must be sanitized rather than wrapped */
    {
        struct TagItem tags[] = {
            { WA_CustomScreen, (ULONG)screen },
            { WA_Left, 0 },
            { WA_Top, 12 },
            { WA_Width, 320 },
            { WA_Height, (ULONG)-3 },
            { TAG_DONE, 0 }
        };

        window = OpenWindowTagList(NULL, tags);
        if (!window) {
            print("FAIL: OpenWindowTagList() negative-height window returned NULL\n");
            errors++;
        } else if (window->Width != 320 || window->Height != (screen->Height - 12)) {
            print("FAIL: OpenWindowTagList() did not sanitize negative WA_Height\n");
            errors++;
        } else {
            print("OK: OpenWindowTagList() sanitizes negative WA_Height\n");
        }
        if (window)
            CloseWindow(window);
    }

    /* Bogus NW_EXTENDED tag pointers must be ignored instead of re-parsed */
    {
        struct ExtNewWindow ext = {0};

        ext.LeftEdge = 16;
        ext.TopEdge = 20;
        ext.Width = 120;
        ext.Height = 60;
        ext.DetailPen = 0;
        ext.BlockPen = 1;
        ext.Flags = WFLG_NW_EXTENDED | WFLG_CLOSEGADGET;
        ext.Title = (UBYTE *)"Ext Window";
        ext.Screen = screen;
        ext.Type = CUSTOMSCREEN;
        ext.Extension = (struct TagItem *)1;

        window = OpenWindow((struct NewWindow *)&ext);
        if (!window) {
            print("FAIL: OpenWindow() rejected bogus NW_EXTENDED window\n");
            errors++;
        } else if (window->LeftEdge != 16 || window->TopEdge != 20 ||
                   window->Width != 120 || window->Height != 60) {
            print("FAIL: OpenWindow() misread bogus NW_EXTENDED fields\n");
            errors++;
        } else {
            print("OK: OpenWindow() ignores bogus NW_EXTENDED tag pointers\n");
        }
        if (window)
            CloseWindow(window);
    }

    CloseScreen(screen);

    /* Negative height values must not wrap to huge unsigned sizes */
    {
        struct TagItem tags[] = {
            { SA_Width, 640 },
            { SA_Height, (ULONG)-3 },
            { SA_Depth, 2 },
            { TAG_DONE, 0 }
        };

        screen = OpenScreenTagList(NULL, tags);
        if (!screen) {
            print("FAIL: OpenScreenTagList(NULL, negative-height tags) returned NULL\n");
            errors++;
        } else if (screen->Width != 640 || screen->Height != 256 || screen->BitMap.Depth != 2) {
            print("FAIL: OpenScreenTagList(NULL, negative-height tags) did not sanitize height\n");
            errors++;
        } else {
            print("OK: OpenScreenTagList() sanitizes negative height values\n");
        }
        if (screen)
            CloseScreen(screen);
    }

    if (errors == 0) {
        print("PASS: invalid_args all tests passed\n");
        return 0;
    }
    print("FAIL: invalid_args had errors\n");
    return 20;
}
