/*
 * backingstoretest.c - Phase 132 SMART_REFRESH backing store regression
 *
 * Drives the dialog-over-window stamping scenario from a single Amiga
 * binary so the host-side driver can probe pixels at three precise points
 * in the lifecycle:
 *
 *   1. After main window opens and a deterministic pattern is drawn.
 *   2. After a smaller "dialog" window opens on top, partially obscuring
 *      the pattern.
 *   3. After the dialog window closes, exposing the saved area again.
 *
 * The main window is SMART_REFRESH (the lxa default — WFLG_SMART_REFRESH==0
 * and not WFLG_SIMPLE_REFRESH/WFLG_SUPER_BITMAP), so the layers backing
 * store must save the obscured pixels on dialog open and restore them on
 * dialog close. After step 3 the visible area must contain the same colour
 * pattern that was drawn in step 1, with no app-driven repaint involved.
 *
 * The app uses input messages from the host (any IDCMP message) to advance
 * through the three lifecycle stages so the driver can take screen captures
 * and pixel reads at deterministic points without time-based races.
 *
 *   stage 0  -> main window open + pattern drawn         (initial)
 *   stage 1  -> dialog window open  on top               (after first signal)
 *   stage 2  -> dialog window closed, expose backing st. (after second signal)
 *   stage 3  -> exit                                     (after third signal)
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

struct Library *IntuitionBase;
struct Library *GfxBase;

#define MAIN_W 320
#define MAIN_H 160
#define DLG_W  120
#define DLG_H   60

/* Drawn pattern: four solid rectangles inside the main window, each with
 * a different pen.  Coordinates are window-local (the host driver adds
 * window->LeftEdge / TopEdge before reading pixels).
 *
 * Layout (window-local):
 *   pen 1:  ( 20, 30) - ( 80, 60)
 *   pen 2:  (100, 30) - (160, 60)   <- in dialog area when dialog opens
 *   pen 3:  ( 20, 80) - ( 80,110)   <- in dialog area when dialog opens
 *   pen 0:  always-visible background between rects
 *
 * The dialog opens at window-local (90,25) with size 120x60 — covering
 * the right side of the pen-2 rect and the upper-right of the pen-3 rect.
 */

static void draw_pattern(struct Window *win)
{
    struct RastPort *rp = win->RPort;

    /* Clear interior to background pen 0 */
    SetAPen(rp, 0);
    RectFill(rp, win->BorderLeft, win->BorderTop,
             win->Width  - win->BorderRight - 1,
             win->Height - win->BorderBottom - 1);

    SetAPen(rp, 1);
    RectFill(rp,  20,  30,  80,  60);

    SetAPen(rp, 2);
    RectFill(rp, 100,  30, 160,  60);

    SetAPen(rp, 3);
    RectFill(rp,  20,  80,  80, 110);
}

static void wait_for_signal(struct Window *win)
{
    struct IntuiMessage *msg;
    BOOL got = FALSE;

    while (!got) {
        WaitPort(win->UserPort);
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL) {
            ULONG cls = msg->Class;
            ReplyMsg((struct Message *)msg);
            /* Any non-refresh message advances the stage. */
            if (cls != IDCMP_REFRESHWINDOW && cls != IDCMP_NEWSIZE) {
                got = TRUE;
            }
        }
    }
}

int main(int argc, char **argv)
{
    struct Window *main_win = NULL;
    struct Window *dlg_win  = NULL;

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    if (!IntuitionBase) {
        printf("BackingStoreTest: failed to open intuition.library\n");
        return 20;
    }

    GfxBase = OpenLibrary((CONST_STRPTR)"graphics.library", 37);
    if (!GfxBase) {
        printf("BackingStoreTest: failed to open graphics.library\n");
        CloseLibrary(IntuitionBase);
        return 20;
    }

    main_win = OpenWindowTags(NULL,
        WA_Left,        20,
        WA_Top,         20,
        WA_Width,       MAIN_W,
        WA_Height,      MAIN_H,
        WA_IDCMP,       IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_VANILLAKEY,
        WA_Flags,       WFLG_ACTIVATE,   /* default refresh = SMART_REFRESH */
        WA_Title,       "BackingStoreTest",
        TAG_DONE);

    if (!main_win) {
        printf("BackingStoreTest: failed to open main window\n");
        CloseLibrary(GfxBase);
        CloseLibrary(IntuitionBase);
        return 20;
    }

    printf("BackingStoreTest: stage 0 - main window open\n");
    draw_pattern(main_win);
    printf("BackingStoreTest: stage 0 - pattern drawn\n");

    /* Stage 0 -> 1: wait for host signal, then open dialog */
    wait_for_signal(main_win);

    dlg_win = OpenWindowTags(NULL,
        WA_Left,        20 + 90,   /* main_win.LeftEdge + 90 */
        WA_Top,         20 + 25,   /* main_win.TopEdge  + 25 */
        WA_Width,       DLG_W,
        WA_Height,      DLG_H,
        WA_IDCMP,       IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_VANILLAKEY,
        WA_Flags,       WFLG_ACTIVATE,
        WA_Title,       "Dlg",
        TAG_DONE);

    if (dlg_win) {
        /* Fill dialog interior with pen 5 so pixel reads can distinguish
         * "dialog covers area" from "backing store restored". */
        struct RastPort *rp = dlg_win->RPort;
        SetAPen(rp, 5);
        RectFill(rp, dlg_win->BorderLeft, dlg_win->BorderTop,
                 dlg_win->Width  - dlg_win->BorderRight - 1,
                 dlg_win->Height - dlg_win->BorderBottom - 1);
        printf("BackingStoreTest: stage 1 - dialog open\n");
    } else {
        printf("BackingStoreTest: stage 1 - dialog open FAILED\n");
    }

    /* Stage 1 -> 2: wait for host signal, then close dialog.
     * Use the dialog's own port so an injected click on the dialog
     * (or any IDCMP message routed there) wakes us. */
    if (dlg_win) {
        wait_for_signal(dlg_win);
        CloseWindow(dlg_win);
        dlg_win = NULL;
        printf("BackingStoreTest: stage 2 - dialog closed\n");
    }

    /* Stage 2 -> 3: wait for host signal, then exit */
    wait_for_signal(main_win);

    CloseWindow(main_win);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    printf("BackingStoreTest: done\n");
    return 0;
}
