/*
 * GalleryMenus - menus with checkmarks, sub-items, COMMSEQ, bars and
 * ghosted items/menus (Phase 223).
 *
 * Runs on its own Workbench-like screen so the full-screen menu snapshots
 * are not disturbed by Workbench icons.  The scenario opens every menu
 * (`menus` step) and snapshots the screen.
 */

#include "gallery.h"

static struct NewMenu newmenu[] = {
    { NM_TITLE, (STRPTR)"Project",       0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Open...",     (STRPTR)"O", 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Save",        (STRPTR)"S", 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Save As...",    0, 0, 0, 0 },
    { NM_ITEM,  NM_BARLABEL,             0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Print",         0, 0, 0, 0 },
    { NM_SUB,   (STRPTR)"Draft",         0, CHECKIT, ~1, 0 },
    { NM_SUB,   (STRPTR)"NLQ",           0, CHECKIT | CHECKED, ~2, 0 },
    { NM_ITEM,  NM_BARLABEL,             0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Quit",        (STRPTR)"Q", 0, 0, 0 },
    { NM_TITLE, (STRPTR)"Edit",          0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Cut",         (STRPTR)"X", NM_ITEMDISABLED, 0, 0 },
    { NM_ITEM,  (STRPTR)"Copy",        (STRPTR)"C", 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Paste",       (STRPTR)"V", NM_ITEMDISABLED, 0, 0 },
    { NM_ITEM,  NM_BARLABEL,             0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Toggle",        0, CHECKIT | MENUTOGGLE | CHECKED, 0, 0 },
    { NM_ITEM,  (STRPTR)"Option A",      0, CHECKIT, ~8, 0 },
    { NM_ITEM,  (STRPTR)"Option B",      0, CHECKIT | CHECKED, ~16, 0 },
    { NM_ITEM,  (STRPTR)"Option C",      0, CHECKIT, ~32, 0 },
    { NM_TITLE, (STRPTR)"Ghost",         0, NM_MENUDISABLED, 0, 0 },
    { NM_ITEM,  (STRPTR)"Nothing",       0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Here",        (STRPTR)"H", 0, 0, 0 },
    { NM_TITLE, (STRPTR)"Long Menu Title", 0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"A rather long menu item", (STRPTR)"L", 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Short",         0, 0, 0, 0 },
    { NM_END,   NULL,                    0, 0, 0, 0 }
};

static UWORD pens[] = { (UWORD)~0 };

int main(int argc, char **argv)
{
    BOOL newlook = argc < 2;
    struct Screen *scr;
    struct Window *win;
    struct Menu *menu;
    APTR vi;

    if (!gallery_open_libs())
        return 20;

    scr = OpenScreenTags(NULL,
        SA_LikeWorkbench, TRUE,
        SA_Pens, (ULONG)pens,
        SA_Title, (ULONG)"Gallery Menu Screen",
        SA_Depth, 2,
        TAG_END);
    if (!scr)
    {
        printf("GalleryMenus: cannot open screen\n");
        return 10;
    }
    vi = GetVisualInfo(scr, TAG_END);
    menu = CreateMenus(newmenu, TAG_END);
    if (!menu || !LayoutMenus(menu, vi, GTMN_NewLookMenus, newlook, TAG_END))
    {
        printf("GalleryMenus: cannot create menus\n");
        return 10;
    }
    win = OpenWindowTags(NULL,
        WA_Left, 100, WA_Top, 60, WA_Width, 300, WA_Height, 80,
        WA_Title, (ULONG)"Menu Gallery",
        WA_CustomScreen, (ULONG)scr,
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_NewLookMenus, newlook,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_MENUPICK | IDCMP_REFRESHWINDOW,
        TAG_END);
    if (!win)
        return 10;
    SetMenuStrip(win, menu);
    gallery_add_window(win);

    gallery_wait();

    ClearMenuStrip(win);
    gallery_close_windows();
    FreeMenus(menu);
    FreeVisualInfo(vi);
    CloseScreen(scr);
    gallery_close_libs();
    return 0;
}
