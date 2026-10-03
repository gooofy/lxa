/*
 * Probe (Phase 222e): gadtools.library CreateMenusA, LayoutMenusA,
 * LayoutMenuItemsA, FreeMenus - menu/item/sub-item geometry, flags,
 * IntuiText placement and bar images for topaz 8 and the screen font, old and
 * new look, and CreateMenus error reporting.
 */
#include <exec/memory.h>
#include <libraries/gadtools.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;
struct Library *GadToolsBase;
struct Library *DiskfontBase;

static UBYTE img_data[2 * 2 * 4];
static struct Image item_image = { 0, 0, 20, 4, 2, (UWORD *)img_data, 3, 0, NULL };

static struct NewMenu nm[] = {
    { NM_TITLE, (STRPTR)"Project", NULL, 0, 0, (APTR)1 },
    { NM_ITEM, (STRPTR)"Open...", (STRPTR)"O", 0, 0, (APTR)2 },
    { NM_ITEM, (STRPTR)"Save", (STRPTR)"S", 0, 0, NULL },
    { NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
    { NM_ITEM, (STRPTR)"Print", NULL, 0, 0, NULL },
    { NM_SUB, (STRPTR)"Draft", (STRPTR)"D", 0, 0, NULL },
    { NM_SUB, NM_BARLABEL, NULL, 0, 0, NULL },
    { NM_SUB, (STRPTR)"Letter quality", NULL, NM_ITEMDISABLED, 0, NULL },
    { NM_ITEM, (STRPTR)"Disabled", NULL, NM_ITEMDISABLED, 0, NULL },
    { NM_ITEM, (STRPTR)"Quit", (STRPTR)"Q", 0, 0, NULL },
    { NM_TITLE, (STRPTR)"Settings", NULL, 0, 0, NULL },
    { NM_ITEM, (STRPTR)"Check", NULL, CHECKIT, 0, NULL },
    { NM_ITEM, (STRPTR)"Toggle on", (STRPTR)"T", CHECKIT | MENUTOGGLE | CHECKED, 0, NULL },
    { NM_ITEM, (STRPTR)"Radio A", NULL, CHECKIT | CHECKED, ~1, NULL },
    { NM_ITEM, (STRPTR)"Radio B", NULL, CHECKIT, ~2, NULL },
    { NM_ITEM, (STRPTR)"Command", (STRPTR)"Cmd", NM_COMMANDSTRING, 0, NULL },
    { NM_TITLE, (STRPTR)"Off", NULL, NM_MENUDISABLED, 0, NULL },
    { NM_ITEM, (STRPTR)"Nothing", NULL, 0, 0, NULL },
    { IM_ITEM, (STRPTR)&item_image, NULL, 0, 0, NULL },
    { NM_TITLE, (STRPTR)"A very long menu title", NULL, 0, 0, NULL },
    { NM_ITEM, (STRPTR)"x", NULL, 0, 0, NULL },
    { NM_END, NULL, NULL, 0, 0, NULL }
};

static struct NewMenu nm_bad[] = {
    { NM_TITLE, (STRPTR)"T", NULL, 0, 0, NULL },
    { NM_SUB, (STRPTR)"sub without item", NULL, 0, 0, NULL },
    { NM_END, NULL, NULL, 0, 0, NULL }
};

static struct NewMenu nm_items_only[] = {
    { NM_ITEM, (STRPTR)"One", NULL, 0, 0, NULL },
    { NM_ITEM, (STRPTR)"Two", (STRPTR)"W", 0, 0, NULL },
    { NM_END, NULL, NULL, 0, 0, NULL }
};

static void dump_items(const char *pfx, struct MenuItem *it, int depth)
{
    int n = 0;
    for (; it && n < 32; it = it->NextItem, n++)
    {
        probe_s(pfx); probe_s(depth ? "  sub" : " item"); probe_dec(n);
        ps_box(it->LeftEdge, it->TopEdge, it->Width, it->Height);
        ps_kx("flags", it->Flags, 4);
        ps_kx("mx", it->MutualExclude, 8);
        ps_kv("cmd", it->Command);
        probe_s(it->SelectFill ? " selfill" : " noselfill");
        probe_ch('\n');
        if (it->Flags & ITEMTEXT)
            dump_itext(depth ? "      " : "    ", (struct IntuiText *)it->ItemFill);
        else
            dump_image(depth ? "      " : "    ", (struct Image *)it->ItemFill);
        if (it->SubItem)
            dump_items(pfx, it->SubItem, depth + 1);
    }
}

static void dump_menus(struct Menu *m)
{
    int n = 0;
    for (; m && n < 16; m = m->NextMenu, n++)
    {
        probe_s("menu"); probe_dec(n);
        ps_str("name", (const char *)m->MenuName);
        ps_box(m->LeftEdge, m->TopEdge, m->Width, m->Height);
        ps_kx("flags", m->Flags, 4);
        probe_ch('\n');
        dump_items("", m->FirstItem, 0);
    }
}

static void run(const char *name, struct TextAttr *ta, BOOL newlook, APTR vi)
{
    struct Menu *menus;
    ULONG err = 0;

    P_SECTION(name);
    menus = CreateMenus(nm, GTMN_FrontPen, 0, TAG_END);
    if (!menus)
    {
        probe_s("CreateMenus failed\n");
        return;
    }
    if (!ta && !newlook)
    {
        /* geometry straight from CreateMenus, before any layout */
        dump_menus(menus);
        FreeMenus(menus);
        return;
    }
    P_BOOL("LayoutMenus", LayoutMenus(menus, vi, ta ? GTMN_TextAttr : TAG_IGNORE, (ULONG)ta, GTMN_NewLookMenus, newlook,
                                      TAG_END));
    dump_menus(menus);
    FreeMenus(menus);
    (void)err;
}

int main(void)
{
    struct TextAttr t8 = { (STRPTR)"topaz.font", 8, 0, 0 };
    struct Screen *scr;
    struct Menu *menus;
    APTR vi;
    ULONG err;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 37);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 37);
    if (!IntuitionBase || !GfxBase || !GadToolsBase || !DiskfontBase)
        return 20;
    scr = LockPubScreen(NULL);
    if (!scr)
        return 20;
    vi = GetVisualInfo(scr, TAG_END);

    run("CreateMenus only", NULL, FALSE, vi);
    run("LayoutMenus topaz8 newlook", &t8, TRUE, vi);
    run("LayoutMenus topaz8 oldlook", &t8, FALSE, vi);
    run("LayoutMenus screen font newlook", NULL, TRUE, vi);

    P_SECTION("CreateMenus error");
    err = 12345;
    menus = CreateMenus(nm_bad, GTMN_SecondaryError, (ULONG)&err, TAG_END);
    P_NULL("result", menus);
    P_LONG("secondary error", err);
    if (menus)
        FreeMenus(menus);

    P_SECTION("CreateMenus items only");
    menus = CreateMenus(nm_items_only, TAG_END);
    P_NULL("result", menus);
    if (menus)
    {
        P_BOOL("LayoutMenuItems", LayoutMenuItems((struct MenuItem *)menus, vi, TAG_END));
        dump_items("", (struct MenuItem *)menus, 0);
        FreeMenus(menus);
    }

    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, scr);
    CloseLibrary(DiskfontBase);
    CloseLibrary(GadToolsBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
