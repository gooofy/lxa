/*
 * ConRepair (Phase 222g): a CON: window is a simple-refresh window on
 * AmigaOS 3.1, so the console must redraw what another window uncovers.
 * The program writes coloured text, covers part of the window with a
 * second window, closes it and compares the console window's pixels
 * before and after.  No input is needed; it runs on the reference too.
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/rastport.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static LONG len(const char *s)
{
    LONG n = 0;
    while (s[n])
        n++;
    return n;
}

static void wr(BPTR fh, const char *s)
{
    Write(fh, (APTR)s, len(s));
}

static void print(const char *s)
{
    wr(Output(), s);
}

static struct Window *find_title(const char *title)
{
    struct Screen *s;
    struct Window *w, *found = NULL;
    ULONG lock = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s && !found; s = s->NextScreen)
        for (w = s->FirstWindow; w && !found; w = w->NextWindow) {
            const char *a = (const char *)w->Title, *b = title;
            if (!a)
                continue;
            while (*a && *a == *b)
                a++, b++;
            if (*a == *b)
                found = w;
        }
    UnlockIBase(lock);
    return found;
}

/* pixels of the window's inner area, as a sum weighted by position */
static ULONG checksum(struct Window *w)
{
    ULONG sum = 0;
    WORD x, y;
    for (y = w->BorderTop; y < w->Height - w->BorderBottom; y++)
        for (x = w->BorderLeft; x < w->Width - w->BorderRight; x++)
            sum = sum * 31 + ReadPixel(w->RPort, x, y) + 1;
    return sum;
}

int main(void)
{
    BPTR con;
    struct Window *cw, *cover;
    ULONG before, covered, after;
    int rc = 0;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 20;
    con = Open((STRPTR)"CON:10/20/300/100/ConRepair", MODE_NEWFILE);
    if (!con)
        return 20;
    wr(con, "plain line one\n");
    wr(con, "\x9b" "32mpen 2\x9b" "0m and \x9b" "1mbold\x9b" "0m\n");
    wr(con, "\x9b" "42m background 2 \x9b" "0m\n");
    wr(con, "\x9b" "4munderlined\x9b" "0m last\n");
    Delay(10);
    cw = find_title("ConRepair");
    if (!cw) {
        print("FAIL: console window not found\n");
        Close(con);
        return 20;
    }
    before = checksum(cw);

    cover = OpenWindowTags(NULL, WA_Left, 40, WA_Top, 30, WA_Width, 200, WA_Height, 60,
                           WA_Title, (ULONG)"Cover", WA_DragBar, TRUE, WA_DepthGadget, TRUE,
                           WA_Activate, FALSE, TAG_END);
    if (!cover) {
        print("FAIL: cover window did not open\n");
        Close(con);
        return 20;
    }
    Delay(10);
    covered = checksum(cw);
    CloseWindow(cover);
    Delay(25);
    after = checksum(cw);

    if (covered == before) {
        print("FAIL: the cover window did not change the console window\n");
        rc = 20;
    } else if (after != before) {
        print("FAIL: the console did not redraw the uncovered area\n");
        rc = 20;
    } else {
        print("OK: the console redraws the uncovered area\n");
    }
    Close(con);
    if (!rc)
        print("PASS\n");
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
