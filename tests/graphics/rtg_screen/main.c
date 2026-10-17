/*
 * rtg_screen - an RTG screen for the host display path (rtg_gtest).
 *
 * Opens a Picasso96 screen of the mode given as argument (hex display ID,
 * default 0x50031100 = 640x480 R5G6B5PC), draws four colour rectangles with
 * p96RectFill and one with a graphics.library pen, prints READY and keeps
 * the screen open for 5 s.  The driver checks what the host display shows.
 */
#define LXA_P96_INLINES
#include <exec/types.h>
#include <dos/dos.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <lxa_p96.h>
#include <stdio.h>
#include <stdlib.h>

struct Library *P96Base = NULL;

int main(int argc, char **argv)
{
    ULONG id = argc > 1 ? strtoul(argv[1], NULL, 16) : 0x50031100;
    struct Screen *s;
    struct RastPort *rp;

    P96Base = OpenLibrary((CONST_STRPTR)P96NAME, 2);
    if (!P96Base) {
        printf("FAIL: no %s\n", P96NAME);
        return 20;
    }
    s = OpenScreenTags(NULL, SA_DisplayID, id, SA_Width, 640, SA_Height, 480, SA_Depth, 24,
                       SA_Quiet, TRUE, SA_ShowTitle, FALSE, TAG_DONE);
    if (!s) {
        printf("FAIL: OpenScreen 0x%08lx\n", id);
        CloseLibrary(P96Base);
        return 20;
    }
    rp = &s->RastPort;
    printf("depth %ld fmt %ld\n", (long)p96GetBitMapAttr(rp->BitMap, P96BMA_DEPTH),
           (long)p96GetBitMapAttr(rp->BitMap, P96BMA_RGBFORMAT));
    p96RectFill(rp, 0, 0, 639, 479, 0x00000000);
    p96RectFill(rp, 10, 10, 109, 109, 0x00ff0000);
    p96RectFill(rp, 120, 10, 219, 109, 0x0000ff00);
    p96RectFill(rp, 230, 10, 329, 109, 0x000000ff);
    p96RectFill(rp, 340, 10, 439, 109, 0x00406080);
    SetRGB32(&s->ViewPort, 5, 0x11111111, 0x88888888, 0xeeeeeeee);
    SetAPen(rp, 5);
    RectFill(rp, 450, 10, 549, 109);
    printf("READY\n");
    fflush(stdout);
    Delay(250);
    CloseScreen(s);
    CloseLibrary(P96Base);
    printf("PASS\n");
    return 0;
}
