/*
 * GalleryFrames - frameiclass frames and GadTools bevel boxes (Phase 223).
 *
 * Left: frameiclass FRAME_DEFAULT/BUTTON/RIDGE/ICONDROPBOX x normal/recessed
 * (rows) x IDS_NORMAL/SELECTED/DISABLED/edges-only (columns).
 * Right: DrawBevelBox() BBFT_BUTTON/RIDGE/ICONDROPBOX x normal/recessed.
 */

#include "gallery.h"

static const ULONG ftypes[] = { FRAME_DEFAULT, FRAME_BUTTON, FRAME_RIDGE, FRAME_ICONDROPBOX };
static const ULONG fstates[] = { IDS_NORMAL, IDS_SELECTED, IDS_DISABLED };

int main(void)
{
    struct Screen *scr;
    struct DrawInfo *dri;
    struct Window *win;
    APTR vi;
    UWORD t, r, c;
    WORD bx, by;

    if (!gallery_open_libs())
        return 20;
    scr = LockPubScreen(NULL);
    dri = GetScreenDrawInfo(scr);
    vi = GetVisualInfo(scr, TAG_END);

    win = OpenWindowTags(NULL,
        WA_Left, 8, WA_Top, 12, WA_Width, 600, WA_Height, 200,
        WA_Title, (ULONG)"Frame Gallery",
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
        WA_PubScreen, (ULONG)scr,
        TAG_END);
    if (!win)
        return 10;
    gallery_add_window(win);
    bx = win->BorderLeft;
    by = win->BorderTop;

    for (t = 0; t < 4; t++)
    {
        for (r = 0; r < 2; r++)
        {
            for (c = 0; c < 4; c++)
            {
                struct Image *im = (struct Image *)NewObject(NULL, (UBYTE *)"frameiclass",
                    IA_Width, 56, IA_Height, 14,
                    IA_FrameType, ftypes[t],
                    IA_Recessed, r,
                    IA_EdgesOnly, c == 3,
                    TAG_END);
                if (!im)
                {
                    if (c == 0)
                        printf("frame %lu recessed %u: none\n", ftypes[t], r);
                    continue;
                }
                if (c == 0)
                    printf("frame %lu recessed %u: %dx%d\n", ftypes[t], r, im->Width, im->Height);
                DrawImageState(win->RPort, im, bx + 6 + c * 66, by + 4 + (t * 2 + r) * 20,
                               c == 3 ? IDS_NORMAL : fstates[c], dri);
                DisposeObject(im);
            }
        }
    }

    for (t = 0; t < 3; t++)
    {
        for (r = 0; r < 2; r++)
        {
            DrawBevelBox(win->RPort, bx + 290 + r * 140, by + 4 + t * 40, 120, 30,
                         GT_VisualInfo, (ULONG)vi,
                         GTBB_Recessed, r,
                         GTBB_FrameType, t + 1,
                         TAG_END);
        }
    }
    /* nested bevels as GadTools apps draw group boxes */
    DrawBevelBox(win->RPort, bx + 290, by + 128, 260, 48, GT_VisualInfo, (ULONG)vi, TAG_END);
    DrawBevelBox(win->RPort, bx + 298, by + 134, 244, 36, GT_VisualInfo, (ULONG)vi,
                 GTBB_Recessed, TRUE, TAG_END);

    gallery_wait();
    gallery_close_windows();
    FreeVisualInfo(vi);
    FreeScreenDrawInfo(scr, dri);
    UnlockPubScreen(NULL, scr);
    gallery_close_libs();
    return 0;
}
