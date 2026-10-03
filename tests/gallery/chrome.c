/*
 * GalleryChrome - window chrome for the common flag combinations (Phase 223).
 *
 * Opens a grid of small windows on the Workbench screen; the last one
 * ("CA Full") is active, all others are inactive.  Each window is
 * snapshotted on its own by tests/scenarios/gallery-chrome.yaml.
 */

#include "gallery.h"

#define CW 150
#define CH 70

static struct Window *open_cell(int col, int row, const char *title, ULONG flags,
                                ULONG tag1, ULONG data1, ULONG tag2, ULONG data2)
{
    struct Window *w = OpenWindowTags(NULL,
        WA_Left, 6 + col * 158,
        WA_Top, 14 + row * 76,
        WA_Width, CW,
        WA_Height, CH,
        WA_Title, (ULONG)title,
        WA_Flags, flags,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
        WA_MinWidth, 40,
        WA_MinHeight, 30,
        WA_MaxWidth, -1,
        WA_MaxHeight, -1,
        tag1, data1,
        tag2, data2,
        TAG_END);
    gallery_add_window(w);
    return w;
}

int main(void)
{
    struct Window *w;
    WORD zoom[4] = { 0, 12, 200, 40 };

    if (!gallery_open_libs())
        return 20;

    open_cell(1, 0, "CB Full", WFLG_CLOSEGADGET | WFLG_DEPTHGADGET | WFLG_SIZEGADGET | WFLG_DRAGBAR,
              TAG_IGNORE, 0, TAG_IGNORE, 0);
    open_cell(2, 0, "CC Depth", WFLG_DEPTHGADGET | WFLG_DRAGBAR, TAG_IGNORE, 0, TAG_IGNORE, 0);
    open_cell(3, 0, "CD Close", WFLG_CLOSEGADGET | WFLG_DRAGBAR, TAG_IGNORE, 0, TAG_IGNORE, 0);
    open_cell(0, 1, "CE Zoom", WFLG_CLOSEGADGET | WFLG_DEPTHGADGET | WFLG_SIZEGADGET | WFLG_DRAGBAR,
              WA_Zoom, (ULONG)zoom, TAG_IGNORE, 0);
    open_cell(1, 1, "CF GZZ", WFLG_CLOSEGADGET | WFLG_DEPTHGADGET | WFLG_SIZEGADGET | WFLG_DRAGBAR |
              WFLG_GIMMEZEROZERO, TAG_IGNORE, 0, TAG_IGNORE, 0);
    open_cell(2, 1, "CG SizeBB", WFLG_CLOSEGADGET | WFLG_DEPTHGADGET | WFLG_SIZEGADGET | WFLG_DRAGBAR |
              WFLG_SIZEBBOTTOM, TAG_IGNORE, 0, TAG_IGNORE, 0);
    open_cell(3, 1, "CH NoDrag", WFLG_CLOSEGADGET | WFLG_DEPTHGADGET, TAG_IGNORE, 0, TAG_IGNORE, 0);
    open_cell(0, 2, "CI Plain", 0, TAG_IGNORE, 0, TAG_IGNORE, 0);
    open_cell(1, 2, "CJ SizeOnly", WFLG_SIZEGADGET | WFLG_SIZEBRIGHT | WFLG_DRAGBAR,
              TAG_IGNORE, 0, TAG_IGNORE, 0);
    w = open_cell(2, 2, "CK Borderless", WFLG_BORDERLESS, TAG_IGNORE, 0, TAG_IGNORE, 0);
    if (w)
    {
        SetAPen(w->RPort, 3);
        RectFill(w->RPort, 4, 4, CW - 5, CH - 5);
        gallery_label(w, 8, 8, "Borderless");
    }
    /* active window last */
    open_cell(0, 0, "CA Full", WFLG_CLOSEGADGET | WFLG_DEPTHGADGET | WFLG_SIZEGADGET | WFLG_DRAGBAR |
              WFLG_ACTIVATE, TAG_IGNORE, 0, TAG_IGNORE, 0);

    gallery_wait();
    gallery_close_windows();
    gallery_close_libs();
    return 0;
}
