/*
 * Probe (Phase 234): which window is active after the active window
 * closes.  BlitzBasic 2's editor window is active again after its about
 * requester window closes on AmigaOS 3.1.  Windows on a private custom
 * screen, so other programs' windows do not interfere.
 */
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <string.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;

static struct Screen *g_scr;

static struct Window *open_win(const char *title, WORD left, BOOL activate)
{
    return OpenWindowTags(NULL, WA_CustomScreen, (ULONG)g_scr, WA_Left, left, WA_Top, 20,
                          WA_Width, 150, WA_Height, 60, WA_Title, (ULONG)title,
                          WA_DragBar, TRUE, WA_Activate, activate, TAG_END);
}

static void state(const char *label, struct Window *a, struct Window *b, struct Window *c)
{
    probe_s(label);
    probe_s(":");
    if (a) { probe_s(" A="); probe_dec((a->Flags & WFLG_WINDOWACTIVE) ? 1 : 0); }
    if (b) { probe_s(" B="); probe_dec((b->Flags & WFLG_WINDOWACTIVE) ? 1 : 0); }
    if (c) { probe_s(" C="); probe_dec((c->Flags & WFLG_WINDOWACTIVE) ? 1 : 0); }
    probe_ch('\n');
}

int main(void)
{
    struct Window *a, *b, *c;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 20;
    g_scr = OpenScreenTags(NULL, SA_Depth, 2, SA_DisplayID, HIRES_KEY, SA_Title, (ULONG)"Activation",
                           SA_ShowTitle, TRUE, TAG_END);
    if (!g_scr)
        return 20;

    P_SECTION("A active, B opened active, B closed");
    a = open_win("A", 0, TRUE);
    Delay(5);
    b = open_win("B", 160, TRUE);
    Delay(5);
    state("open", a, b, NULL);
    CloseWindow(b);
    Delay(5);
    state("closed B", a, NULL, NULL);
    CloseWindow(a);

    P_SECTION("A inactive, B opened active, B closed");
    a = open_win("A", 0, FALSE);
    Delay(5);
    b = open_win("B", 160, TRUE);
    Delay(5);
    state("open", a, b, NULL);
    CloseWindow(b);
    Delay(5);
    state("closed B", a, NULL, NULL);
    CloseWindow(a);

    P_SECTION("A active, B active, C active, C closed, B closed");
    a = open_win("A", 0, TRUE);
    Delay(5);
    b = open_win("B", 160, TRUE);
    Delay(5);
    c = open_win("C", 320, TRUE);
    Delay(5);
    state("open", a, b, c);
    CloseWindow(c);
    Delay(5);
    state("closed C", a, b, NULL);
    CloseWindow(b);
    Delay(5);
    state("closed B", a, NULL, NULL);
    CloseWindow(a);

    P_SECTION("A active, B active, ActivateWindow A, B closed");
    a = open_win("A", 0, TRUE);
    Delay(5);
    b = open_win("B", 160, TRUE);
    Delay(5);
    c = open_win("C", 320, TRUE);
    Delay(5);
    ActivateWindow(a);
    Delay(5);
    state("activated A", a, b, c);
    CloseWindow(a);
    Delay(5);
    state("closed A", NULL, b, c);
    CloseWindow(c);
    Delay(5);
    state("closed C", NULL, b, NULL);
    CloseWindow(b);

    CloseScreen(g_scr);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
