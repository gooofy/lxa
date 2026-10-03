/*
 * GalleryScreen - a custom screen with its title bar, a backdrop window and
 * a normal window (Phase 223).
 *
 *   GalleryScreen         8-colour hires screen, SA_Pens {~0} (new look)
 *   GalleryScreen old     same without SA_Pens (pre-V36 look)
 *   GalleryScreen lores   lores 320 wide, new look
 *
 * The whole screen is snapshotted, so the screen title bar, the screen depth
 * gadget, the default palette and the border sizes are all compared.
 */

#include "gallery.h"

static UWORD pens[] = { (UWORD)~0 };

int main(int argc, char **argv)
{
    struct Screen *scr;
    struct Window *back, *win;
    BOOL old = argc > 1 && !strcmp(argv[1], "old");
    BOOL lores = argc > 1 && !strcmp(argv[1], "lores");

    if (!gallery_open_libs())
        return 20;

    scr = OpenScreenTags(NULL,
        SA_Depth, 3,
        SA_Width, lores ? 320 : 640,
        SA_Height, 256,
        SA_DisplayID, lores ? 0x0000 : 0x8000,
        SA_Title, (ULONG)"Gallery Screen",
        old ? TAG_IGNORE : SA_Pens, (ULONG)pens,
        TAG_END);
    if (!scr)
    {
        printf("GalleryScreen: cannot open screen\n");
        return 10;
    }
    printf("wbor %d %d %d %d bar %d vb %d hb %d menu %d %d\n",
           scr->WBorLeft, scr->WBorTop, scr->WBorRight, scr->WBorBottom,
           scr->BarHeight, scr->BarVBorder, scr->BarHBorder, scr->MenuVBorder, scr->MenuHBorder);

    {
        struct DrawInfo *dri = GetScreenDrawInfo(scr);
        if (dri)
        {
            int i;
            printf("dri v%d flags %lx res %d %d pens", dri->dri_Version, dri->dri_Flags,
                   dri->dri_Resolution.X, dri->dri_Resolution.Y);
            for (i = 0; i < dri->dri_NumPens && i < 12; i++)
                printf(" %d", dri->dri_Pens[i]);
            printf("\n");
            FreeScreenDrawInfo(scr, dri);
        }
    }
    back = OpenWindowTags(NULL,
        WA_Left, 0, WA_Top, scr->BarHeight + 1,
        WA_Width, scr->Width, WA_Height, scr->Height - scr->BarHeight - 1,
        WA_Title, (ULONG)"Backdrop",
        WA_Backdrop, TRUE, WA_Borderless, TRUE,
        WA_CustomScreen, (ULONG)scr,
        WA_IDCMP, 0,
        TAG_END);
    if (back)
    {
        UWORD p;
        for (p = 0; p < 8; p++)
        {
            SetAPen(back->RPort, p);
            RectFill(back->RPort, 4 + p * 18, 150, 4 + p * 18 + 15, 170);
        }
        gallery_label(back, 4, 180, "Backdrop window");
        gallery_add_window(back);
    }
    win = OpenWindowTags(NULL,
        WA_Left, 20, WA_Top, 30, WA_Width, lores ? 200 : 300, WA_Height, 100,
        WA_Title, (ULONG)"Screen Win",
        WA_CustomScreen, (ULONG)scr,
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_SizeGadget, TRUE,
        WA_Activate, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
        TAG_END);
    if (win)
    {
        printf("win border %d %d %d %d\n", win->BorderLeft, win->BorderTop,
               win->BorderRight, win->BorderBottom);
        gallery_add_window(win);
    }

    gallery_wait();
    gallery_close_windows();
    CloseScreen(scr);
    gallery_close_libs();
    return 0;
}
