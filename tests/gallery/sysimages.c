/*
 * GallerySysI - BOOPSI sysiclass images (Phase 223).
 *
 * Columns: every SYSIA_Which value; rows: SYSISIZE_MEDRES/LOWRES/HIRES x
 * IDS_NORMAL/SELECTED/INACTIVENORMAL/DISABLED.  stdout lists the size of
 * each image so the structure is compared too.
 *
 *   GallerySysI        on the window background (pen 0)
 *   GallerySysI bg     on a pen-3 background (shows the transparent pixels)
 */

#include "gallery.h"

static const UWORD which[] = { DEPTHIMAGE, ZOOMIMAGE, SIZEIMAGE, CLOSEIMAGE, SDEPTHIMAGE,
                               LEFTIMAGE, UPIMAGE, RIGHTIMAGE, DOWNIMAGE, CHECKIMAGE,
                               MXIMAGE, MENUCHECK, AMIGAKEY };
static const UWORD sizes[] = { SYSISIZE_MEDRES, SYSISIZE_LOWRES, SYSISIZE_HIRES };
static const ULONG states[] = { IDS_NORMAL, IDS_SELECTED, IDS_INACTIVENORMAL, IDS_DISABLED };

#define NW (sizeof(which) / sizeof(which[0]))
#define CELLW 44
#define CELLH 17
#define CELLH_HIRES 23   /* SYSISIZE_HIRES arrows are 22 pixels high */
#define ROWY(si, st) ((si) < 2 ? ((si) * 4 + (st)) * CELLH : 8 * CELLH + (st) * CELLH_HIRES)

int main(int argc, char **argv)
{
    BOOL bg = argc > 1 && !strcmp(argv[1], "bg");
    struct Screen *scr;
    struct DrawInfo *dri;
    struct Window *win;
    UWORD wi, si, st;

    if (!gallery_open_libs())
        return 20;
    scr = LockPubScreen(NULL);
    dri = GetScreenDrawInfo(scr);

    win = OpenWindowTags(NULL,
        WA_Left, 8, WA_Top, 11, WA_Width, 6 + NW * CELLW + 8, WA_Height, 13 + 8 * CELLH + 4 * CELLH_HIRES + 4,
        WA_Title, (ULONG)"SysI Gallery",
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
        WA_PubScreen, (ULONG)scr,
        TAG_END);
    if (!win)
        return 10;
    gallery_add_window(win);
    if (bg)
    {
        /* pen-3 background: pixels an image leaves untouched show up */
        SetAPen(win->RPort, 3);
        RectFill(win->RPort, win->BorderLeft, win->BorderTop,
                 win->Width - win->BorderRight - 1, win->Height - win->BorderBottom - 1);
    }

    for (si = 0; si < 3; si++)
    {
        for (wi = 0; wi < NW; wi++)
        {
            struct Image *im = (struct Image *)NewObject(NULL, (UBYTE *)"sysiclass",
                SYSIA_DrawInfo, (ULONG)dri,
                SYSIA_Which, which[wi],
                SYSIA_Size, sizes[si],
                TAG_END);
            if (!im)
            {
                printf("which %u size %u: none\n", which[wi], sizes[si]);
                continue;
            }
            printf("which %u size %u: %dx%d\n", which[wi], sizes[si], im->Width, im->Height);
            for (st = 0; st < 4; st++)
            {
                DrawImageState(win->RPort, im,
                               win->BorderLeft + 4 + wi * CELLW,
                               win->BorderTop + 2 + ROWY(si, st),
                               states[st], dri);
            }
            DisposeObject(im);
        }
    }

    gallery_wait();
    gallery_close_windows();
    FreeScreenDrawInfo(scr, dri);
    UnlockPubScreen(NULL, scr);
    gallery_close_libs();
    return 0;
}
