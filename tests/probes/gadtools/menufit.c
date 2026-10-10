/*
 * Probe (Phase 244): how LayoutMenusA keeps menus on the screen.  ADPro's
 * and MaxonBASIC's right-most menus move their items left so they end at
 * the screen's right edge, Scout's long "List" menu continues in a second
 * column (AmigaOS 3.1 sweep).  Custom screens of a known size and font.
 */
#include <exec/memory.h>
#include <libraries/gadtools.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/graphics_protos.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;
struct Library *GadToolsBase;

static struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, 0, 0 };
static UWORD pens[] = { (UWORD)~0 };

static struct NewMenu nm[80];
static char names[80][40];
static int nn;

static void add(UBYTE type, const char *label)
{
    nm[nn].nm_Type = type;
    nm[nn].nm_Label = label ? (STRPTR)label : NM_BARLABEL;
    nm[nn].nm_CommKey = NULL;
    nm[nn].nm_Flags = 0;
    nm[nn].nm_MutualExclude = 0;
    nm[nn].nm_UserData = NULL;
    nn++;
}

static const char *num(const char *pfx, int i)
{
    char *s = names[i];
    int k = 0;
    while (*pfx)
        s[k++] = *pfx++;
    s[k++] = '0' + (i / 10) % 10;
    s[k++] = '0' + i % 10;
    s[k] = 0;
    return s;
}

static void dump_items(struct MenuItem *it, int depth)
{
    int n = 0;
    for (; it && n < 64; it = it->NextItem, n++)
    {
        probe_s(depth ? "    sub" : "  item"); probe_dec(n);
        ps_box(it->LeftEdge, it->TopEdge, it->Width, it->Height);
        probe_ch('\n');
        if (it->SubItem)
            dump_items(it->SubItem, depth + 1);
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
        probe_ch('\n');
        dump_items(m->FirstItem, 0);
    }
}

static void run(const char *name, WORD w, WORD h, BOOL newlook)
{
    struct Screen *scr;
    APTR vi;
    struct Menu *menus;

    P_SECTION(name);
    scr = OpenScreenTags(NULL, SA_Width, w, SA_Height, h, SA_Depth, 2, SA_DisplayID, 0x8000,
                         SA_Font, (ULONG)&topaz8, SA_Pens, (ULONG)pens, SA_Title, (ULONG)"Probe",
                         TAG_END);
    if (!scr)
    {
        probe_s("OpenScreen failed\n");
        return;
    }
    P_LONG("BarHeight", scr->BarHeight);
    vi = GetVisualInfo(scr, TAG_END);
    menus = CreateMenus(nm, TAG_END);
    if (menus && vi)
    {
        P_BOOL("LayoutMenus", LayoutMenus(menus, vi, GTMN_NewLookMenus, newlook, TAG_END));
        dump_menus(menus);
    }
    else
        probe_s("CreateMenus failed\n");
    FreeMenus(menus);
    FreeVisualInfo(vi);
    CloseScreen(scr);
}

int main(void)
{
    int i;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 37);
    if (!IntuitionBase || !GfxBase || !GadToolsBase)
        return 0;

    /* right-most menus: a wide item list, sub-menus, a narrow one */
    nn = 0;
    add(NM_TITLE, "Project0000");
    add(NM_ITEM, "Open");
    add(NM_TITLE, "Edit00000000");
    add(NM_ITEM, "Cut");
    add(NM_TITLE, "Display00000000");
    add(NM_ITEM, "Show");
    add(NM_TITLE, "Settings0000");
    add(NM_ITEM, "Set ADPro's Screen....");
    add(NM_ITEM, "Set Visual");
    add(NM_SUB, "Screen Mode...");
    add(NM_SUB, "Deep");
    add(NM_ITEM, NULL);
    add(NM_ITEM, "Set Modules");
    add(NM_SUB, "Module #1");
    add(NM_TITLE, "User");
    add(NM_ITEM, "Open User Command List.....");
    add(NM_ITEM, "Short");
    add(NM_SUB, "A rather long sub item text");
    add(NM_TITLE, "X");
    add(NM_ITEM, "A");
    add(NM_SUB, "Sub");
    add(NM_END, NULL);
    run("right edge 640x256", 640, 256, FALSE);
    run("right edge 640x256 newlook", 640, 256, TRUE);
    run("right edge 480x256", 480, 256, FALSE);

    /* a long menu: columns */
    nn = 0;
    add(NM_TITLE, "Project");
    add(NM_ITEM, "About");
    add(NM_TITLE, "List");
    for (i = 0; i < 40; i++)
    {
        if (i == 20)
            add(NM_ITEM, NULL);
        add(NM_ITEM, num(i < 26 ? "Item number " : "Item ", i));
        if (i == 30)
            add(NM_SUB, "Sub of 30");
    }
    add(NM_TITLE, "Other");
    add(NM_ITEM, "Flush");
    add(NM_END, NULL);
    run("columns 640x256", 640, 256, FALSE);
    run("columns 640x200", 640, 200, FALSE);
    run("columns 640x256 newlook", 640, 256, TRUE);
    run("columns 640x194", 640, 194, FALSE);
    run("columns 640x196", 640, 196, FALSE);
    run("columns 640x198", 640, 198, FALSE);
    run("columns 640x199", 640, 199, FALSE);

    CloseLibrary(GadToolsBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
