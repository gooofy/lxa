/*
 * layerbackfilltest.c - Phase 150 layer creation backfill regression
 *
 * Tests that a newly-created layer is initialised to pen 0 (background),
 * not inheriting pixels from windows that previously occupied the same
 * screen area.
 *
 * Two scenarios driven by host signals:
 *
 *   Scenario A (overlapping windows):
 *     stage 0: Window A open, interior filled with pen 2
 *     stage 1: Window B open on top, fully overlapping A
 *              Window B's interior must be pen 0 (backfill applied)
 *     stage 2: exit
 *
 *   Scenario B (sequential windows at same coordinates):
 *     Triggered by a second run (via rc=1 argv[1]="b"):
 *     stage 0: Window A open, interior filled with pen 3, then closed
 *     stage 1: Window B open at same coords
 *              Window B's interior must be pen 0 (not pen 3 ghost)
 *     stage 2: exit
 *
 * Both scenarios print their stage tags so the host GTest driver knows
 * when to probe pixels.
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <stdio.h>
#include <string.h>

struct Library *IntuitionBase;
struct Library *GfxBase;

#define WIN_LEFT  40
#define WIN_TOP   40
#define WIN_W    200
#define WIN_H    120

static void fill_interior(struct Window *win, int pen)
{
    struct RastPort *rp = win->RPort;
    SetAPen(rp, pen);
    RectFill(rp,
             win->BorderLeft,
             win->BorderTop,
             win->Width  - win->BorderRight  - 1,
             win->Height - win->BorderBottom - 1);
}

static void wait_signal(struct Window *win)
{
    struct IntuiMessage *msg;
    BOOL got = FALSE;

    while (!got) {
        WaitPort(win->UserPort);
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL) {
            ULONG cls = msg->Class;
            ReplyMsg((struct Message *)msg);
            if (cls != IDCMP_REFRESHWINDOW && cls != IDCMP_NEWSIZE)
                got = TRUE;
        }
    }
}

static int run_scenario_a(void)
{
    struct Window *win_a = NULL;
    struct Window *win_b = NULL;

    /* Open Window A */
    win_a = OpenWindowTags(NULL,
        WA_Left,    WIN_LEFT,
        WA_Top,     WIN_TOP,
        WA_Width,   WIN_W,
        WA_Height,  WIN_H,
        WA_IDCMP,   IDCMP_VANILLAKEY | IDCMP_MOUSEBUTTONS,
        WA_Flags,   WFLG_ACTIVATE,
        WA_Title,   "LayerBackFill-A",
        TAG_DONE);

    if (!win_a) {
        printf("LayerBackFillTest: scenario A: failed to open window A\n");
        return 20;
    }

    fill_interior(win_a, 2);
    printf("LayerBackFillTest: stage a0 - window A open with pen 2\n");

    /* Wait for host to confirm stage 0 */
    wait_signal(win_a);

    /* Open Window B fully overlapping A */
    win_b = OpenWindowTags(NULL,
        WA_Left,    WIN_LEFT,
        WA_Top,     WIN_TOP,
        WA_Width,   WIN_W,
        WA_Height,  WIN_H,
        WA_IDCMP,   IDCMP_VANILLAKEY | IDCMP_MOUSEBUTTONS,
        WA_Flags,   WFLG_ACTIVATE,
        WA_Title,   "LayerBackFill-B",
        TAG_DONE);

    if (!win_b) {
        printf("LayerBackFillTest: scenario A: failed to open window B\n");
        CloseWindow(win_a);
        return 20;
    }

    /* Window B's interior MUST be pen 0 — backfill was applied at layer creation */
    printf("LayerBackFillTest: stage a1 - window B open (interior must be pen 0)\n");

    /* Wait for host pixel check, then exit */
    wait_signal(win_b);

    CloseWindow(win_b);
    CloseWindow(win_a);
    printf("LayerBackFillTest: stage a2 - done\n");
    return 0;
}

static int run_scenario_b(void)
{
    struct Window *win_a = NULL;
    struct Window *win_b = NULL;

    /* Open Window A and fill with pen 3 */
    win_a = OpenWindowTags(NULL,
        WA_Left,    WIN_LEFT,
        WA_Top,     WIN_TOP,
        WA_Width,   WIN_W,
        WA_Height,  WIN_H,
        WA_IDCMP,   IDCMP_VANILLAKEY | IDCMP_MOUSEBUTTONS,
        WA_Flags,   WFLG_ACTIVATE,
        WA_Title,   "LayerBackFill-A",
        TAG_DONE);

    if (!win_a) {
        printf("LayerBackFillTest: scenario B: failed to open window A\n");
        return 20;
    }

    fill_interior(win_a, 3);
    printf("LayerBackFillTest: stage b0 - window A open with pen 3\n");

    /* Wait for host to confirm, then close Window A */
    wait_signal(win_a);
    CloseWindow(win_a);
    win_a = NULL;
    printf("LayerBackFillTest: stage b1 - window A closed\n");

    /* Open Window B at the exact same position */
    win_b = OpenWindowTags(NULL,
        WA_Left,    WIN_LEFT,
        WA_Top,     WIN_TOP,
        WA_Width,   WIN_W,
        WA_Height,  WIN_H,
        WA_IDCMP,   IDCMP_VANILLAKEY | IDCMP_MOUSEBUTTONS,
        WA_Flags,   WFLG_ACTIVATE,
        WA_Title,   "LayerBackFill-B",
        TAG_DONE);

    if (!win_b) {
        printf("LayerBackFillTest: scenario B: failed to open window B\n");
        return 20;
    }

    /* Window B's interior MUST be pen 0 — not inheriting pen 3 from A */
    printf("LayerBackFillTest: stage b2 - window B open (interior must be pen 0)\n");

    wait_signal(win_b);

    CloseWindow(win_b);
    printf("LayerBackFillTest: stage b3 - done\n");
    return 0;
}

int main(int argc, char **argv)
{
    int rc;

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    if (!IntuitionBase) {
        printf("LayerBackFillTest: failed to open intuition.library\n");
        return 20;
    }

    GfxBase = OpenLibrary((CONST_STRPTR)"graphics.library", 37);
    if (!GfxBase) {
        printf("LayerBackFillTest: failed to open graphics.library\n");
        CloseLibrary(IntuitionBase);
        return 20;
    }

    if (argc > 1 && strcmp(argv[1], "b") == 0)
        rc = run_scenario_b();
    else
        rc = run_scenario_a();

    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return rc;
}
