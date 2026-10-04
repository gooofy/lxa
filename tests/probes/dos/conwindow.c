/*
 * Probe (Phase 222g): the window the CON: handler opens for
 * "CON:x/y/w/h/title/options" - Intuition flags, system gadgets, size
 * limits and geometry for the 3.1 options (CLOSE, NOCLOSE, SIMPLE, SMART,
 * NOSIZE, NODRAG, NODEPTH, BACKDROP, NOBORDER, INACTIVE, ALT) and for
 * missing or out-of-range geometry.  Left out: AUTO and WAIT (they need
 * user interaction) and SCREEN.
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct IntuitionBase *IntuitionBase;

static int same(const char *a, const char *b)
{
    if (!a || !b)
        return 0;
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* the window the handler opened: the one that was not there before */
#define MAXWIN 64
static struct Window *before[MAXWIN];
static int nbefore;

static void snapshot(void)
{
    struct Screen *s;
    struct Window *w;
    ULONG lock = LockIBase(0);
    nbefore = 0;
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
        for (w = s->FirstWindow; w && nbefore < MAXWIN; w = w->NextWindow)
            before[nbefore++] = w;
    UnlockIBase(lock);
}

static struct Window *find_window(void)
{
    struct Screen *s;
    struct Window *w, *found = NULL;
    int i;
    ULONG lock = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s && !found; s = s->NextScreen)
        for (w = s->FirstWindow; w && !found; w = w->NextWindow) {
            for (i = 0; i < nbefore && before[i] != w; i++)
                ;
            if (i == nbefore)
                found = w;
        }
    UnlockIBase(lock);
    return found;
}

static const char *gadname(UWORD type)
{
    switch (type & GTYP_SYSTYPEMASK) {
    case GTYP_SIZING: return "size";
    case GTYP_WDRAGGING: return "drag";
    case GTYP_WUPFRONT: return "depth";
    case GTYP_WDOWNBACK: return "zoom";
    case GTYP_CLOSE: return "close";
    }
    return "user";
}

static void show2(const char *spec, int zip)
{
    BPTR fh;
    struct Window *w;
    struct Gadget *g;

    probe_s("-- ");
    probe_s(spec);
    probe_ch('\n');
    snapshot();
    fh = Open((STRPTR)spec, MODE_NEWFILE);
    if (!fh) {
        P_LONG("Open failed, IoErr", IoErr());
        return;
    }
    /* the handler opens its window on the first I/O at the latest */
    Write(fh, (STRPTR)"x", 1);
    Delay(25);
    w = find_window();
    if (!w) {
        probe_s("window not found\n");
    } else {
        probe_s("title ");
        if (w->Title) {
            probe_ch('"');
            probe_s((char *)w->Title);
            probe_ch('"');
        } else
            probe_s("NULL");
        probe_ch('\n');
        /* WFLG_WINDOWACTIVE depends on the input focus, not on the handler */
        P_HEX("Flags", w->Flags & ~WFLG_WINDOWACTIVE);
        P_HEX("IDCMPFlags", w->IDCMPFlags);
        probe_s("box ");
        probe_dec(w->LeftEdge);
        probe_ch('/');
        probe_dec(w->TopEdge);
        probe_ch('/');
        probe_dec(w->Width);
        probe_ch('/');
        probe_dec(w->Height);
        probe_s(" min ");
        probe_dec(w->MinWidth);
        probe_ch('/');
        probe_dec(w->MinHeight);
        probe_s(" max ");
        probe_dec((UWORD)w->MaxWidth);
        probe_ch('/');
        probe_dec((UWORD)w->MaxHeight);
        probe_s(" borders ");
        probe_dec(w->BorderLeft);
        probe_ch('/');
        probe_dec(w->BorderTop);
        probe_ch('/');
        probe_dec(w->BorderRight);
        probe_ch('/');
        probe_dec(w->BorderBottom);
        probe_ch('\n');
        probe_s("gadgets");
        for (g = w->FirstGadget; g; g = g->NextGadget) {
            probe_ch(' ');
            probe_s(gadname(g->GadgetType));
        }
        probe_ch('\n');
        if (w->Flags & WFLG_BORDERLESS) {
            for (g = w->FirstGadget; g; g = g->NextGadget) {
                probe_s("  ");
                probe_s(gadname(g->GadgetType));
                probe_s(" flags ");
                probe_hex(g->Flags, 4);
                probe_s(" box ");
                probe_dec(g->LeftEdge);
                probe_ch('/');
                probe_dec(g->TopEdge);
                probe_ch('/');
                probe_dec(g->Width);
                probe_ch('/');
                probe_dec(g->Height);
                probe_ch('\n');
            }
        }
        if (zip) {
            /* the alternate (zoom) box */
            ZipWindow(w);
            Delay(10);
            probe_s("zipped ");
            probe_dec(w->LeftEdge);
            probe_ch('/');
            probe_dec(w->TopEdge);
            probe_ch('/');
            probe_dec(w->Width);
            probe_ch('/');
            probe_dec(w->Height);
            probe_ch('\n');
        }
    }
    Close(fh);
}

static void show(const char *spec)
{
    show2(spec, 0);
}

int main(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 20;
    show2("CON:10/20/300/100/CW plain", 1);
    show("CON:10/20/300/100/CW close/CLOSE");
    show("CON:10/20/300/100/CW noclose/NOCLOSE");
    show("CON:10/20/300/100/CW simple/SIMPLE");
    show("CON:10/20/300/100/CW smart/SMART");
    show("CON:10/20/300/100/CW nosize/NOSIZE");
    show("CON:10/20/300/100/CW nodrag/NODRAG");
    show2("CON:10/20/300/100/CW nodepth/NODEPTH", 1);
    show("CON:10/20/300/100/CW backdrop/BACKDROP");
    show("CON:10/20/300/100/CW noborder/NOBORDER");
    show("CON:10/20/300/100/CW inactive/INACTIVE");
    show2("CON:10/20/300/100/CW alt/ALT30/40/200/60", 1);
    show2("CON:10/20/300/100/CW alt nodepth/NODEPTH/ALT30/40/200/60", 1);
    show("CON:10/20/300/100/CW many/CLOSE/SMART/NOSIZE/INACTIVE");
    show("CON:10/20/300/100/CW lower/close/simple");
    show2("CON:10/20/30/20/CW tiny", 1);
    show("RAW:10/20/300/100/CW raw");
    show("CON:////CW nogeometry");
    show("CON:0/0/0/0/CW zero");
    show("CON:10/20/300//CW noheight");
    show("CON:10/20//100/CW nowidth");
    show("CON:10/20/79/49/CW 79x49");
    show("CON:10/20/81/51/CW 81x51");
    show("CON:10/20/2000/1000/CW huge");
    show("CON:500/200/300/100/CW offscreen");
    show("CON:10/20/300/100");
    show("CON:10/20/300/100/");
    show2("CON:", 1);
    show("CON:10/20/300/100/CW unknown/FOO");
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
