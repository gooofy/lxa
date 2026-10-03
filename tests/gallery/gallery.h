/*
 * gallery.h - shared helpers for the rendering conformance gallery (Phase 223).
 *
 * Every gallery program opens one or more windows that show a page of UI
 * elements, then waits until any of its windows receives CLOSEWINDOW or the
 * process gets CTRL-C (the reference agent's QUIT).  The pages are compared
 * pen-index for pen-index with real AmigaOS 3.1 (tests/scenarios/gallery-*.yaml).
 */

#ifndef LXA_GALLERY_H
#define LXA_GALLERY_H

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <graphics/gfxbase.h>
#include <graphics/text.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GALLERY_MAX_WINDOWS 16

struct Library *IntuitionBase;
struct Library *GfxBase;
struct Library *GadToolsBase;
struct Library *DiskfontBase;

static struct Window *g_windows[GALLERY_MAX_WINDOWS];
static int g_nwindows;

static BOOL gallery_open_libs(void)
{
    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 37);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 37);
    return IntuitionBase && GfxBase && GadToolsBase;
}

static void gallery_close_libs(void)
{
    if (DiskfontBase) CloseLibrary(DiskfontBase);
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (GfxBase) CloseLibrary(GfxBase);
    if (IntuitionBase) CloseLibrary(IntuitionBase);
}

static void gallery_add_window(struct Window *w)
{
    if (w && g_nwindows < GALLERY_MAX_WINDOWS)
        g_windows[g_nwindows++] = w;
}

/* Wait until a window is closed or CTRL-C arrives.  GadTools messages are
 * filtered through GT_GetIMsg so GadTools gadgets keep working. */
static void gallery_wait(void)
{
    BOOL done = FALSE;

    fflush(stdout);     /* the snapshot is taken while we wait */
    while (!done)
    {
        ULONG mask = SIGBREAKF_CTRL_C;
        ULONG sigs;
        int i;

        for (i = 0; i < g_nwindows; i++)
            if (g_windows[i]->UserPort)
                mask |= 1UL << g_windows[i]->UserPort->mp_SigBit;
        sigs = Wait(mask);
        if (sigs & SIGBREAKF_CTRL_C)
            done = TRUE;
        for (i = 0; i < g_nwindows; i++)
        {
            struct IntuiMessage *im;
            if (!g_windows[i]->UserPort)
                continue;
            while ((im = GT_GetIMsg(g_windows[i]->UserPort)) != NULL)
            {
                ULONG cl = im->Class;
                GT_ReplyIMsg(im);
                if (cl == IDCMP_CLOSEWINDOW)
                    done = TRUE;
                else if (cl == IDCMP_REFRESHWINDOW)
                {
                    GT_BeginRefresh(g_windows[i]);
                    GT_EndRefresh(g_windows[i], TRUE);
                }
            }
        }
    }
}

static void gallery_close_windows(void)
{
    while (g_nwindows > 0)
        CloseWindow(g_windows[--g_nwindows]);
}

/* Label text in pen 1 at a window-relative position (baseline-adjusted). */
static void gallery_label(struct Window *w, WORD x, WORD y, const char *s)
{
    struct RastPort *rp = w->RPort;
    SetAPen(rp, 1);
    SetDrMd(rp, JAM1);
    Move(rp, x, y + rp->TxBaseline);
    Text(rp, (STRPTR)s, strlen(s));
}

#endif
