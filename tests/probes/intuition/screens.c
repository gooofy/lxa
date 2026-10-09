/*
 * Probe (Phase 222d): intuition.library OpenScreen/OpenScreenTagList/
 * CloseScreen/GetScreenDrawInfo - screen title bar geometry (BarHeight,
 * BarVBorder/BarHBorder, Menu borders, WBor*), flags (SHOWTITLE default
 * for NewScreen and tag screens), titles, fonts and DrawInfo for the
 * common modes and fonts, plus the Workbench screen itself.
 */
#include <exec/memory.h>
#include <graphics/displayinfo.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>
#include <string.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;
struct Library *DiskfontBase;

static void dump_screen(struct Screen *s, BOOL wb)
{
    struct DrawInfo *dri;
    int i;

    probe_s("screen");
    ps_box(s->LeftEdge, s->TopEdge, s->Width, s->Height);
    ps_kx("flags", s->Flags, 4);
    if (!wb)    /* the Workbench screen title is the active window's */
        ps_str("title", (const char *)s->Title);
    ps_str("deftitle", (const char *)s->DefaultTitle);
    probe_ch('\n');
    probe_s(" ");
    ps_kv("barh", s->BarHeight); ps_kv("barv", s->BarVBorder); ps_kv("barhb", s->BarHBorder);
    ps_kv("menuv", s->MenuVBorder); ps_kv("menuh", s->MenuHBorder);
    ps_kv("wbt", s->WBorTop); ps_kv("wbl", s->WBorLeft);
    ps_kv("wbr", s->WBorRight); ps_kv("wbb", s->WBorBottom);
    probe_ch('\n');
    probe_s(" ");
    probe_s(" font=");
    if (s->Font) { probe_s((const char *)s->Font->ta_Name); probe_ch('/'); probe_dec(s->Font->ta_YSize); }
    else probe_s("NULL");
    ps_kv("rpfont", s->RastPort.Font ? s->RastPort.Font->tf_YSize : -1);
    ps_kv("depth", s->BitMap.Depth);
    ps_kv("vpmodes", s->ViewPort.Modes);
    ps_kv("detailpen", s->DetailPen); ps_kv("blockpen", s->BlockPen);
    probe_s(s->BarLayer ? " barlayer" : " nobarlayer");
    if (s->BarLayer)
        ps_box(s->BarLayer->bounds.MinX, s->BarLayer->bounds.MinY,
               s->BarLayer->bounds.MaxX - s->BarLayer->bounds.MinX + 1,
               s->BarLayer->bounds.MaxY - s->BarLayer->bounds.MinY + 1);
    probe_s(s->FirstGadget ? " gadgets" : " nogadgets");
    probe_ch('\n');
    dump_glist(" sg ", s->FirstGadget);

    dri = GetScreenDrawInfo(s);
    if (dri)
    {
        probe_s(" dri");
        ps_kv("version", dri->dri_Version);
        ps_kv("numpens", dri->dri_NumPens);
        ps_kx("flags", dri->dri_Flags, 8);
        ps_kv("depth", dri->dri_Depth);
        ps_kv("resx", dri->dri_Resolution.X); ps_kv("resy", dri->dri_Resolution.Y);
        ps_kv("font", dri->dri_Font ? dri->dri_Font->tf_YSize : -1);
        probe_s(" pens");
        for (i = 0; i < dri->dri_NumPens && i < 16; i++)
        {
            probe_ch(i ? ',' : '=');
            probe_dec((WORD)dri->dri_Pens[i]);
        }
        probe_ch('\n');
        FreeScreenDrawInfo(s, dri);
    }
}

static void show(const char *name, struct Screen *s)
{
    P_SECTION(name);
    if (!s)
    {
        probe_s("OpenScreen = NULL\n");
        return;
    }
    dump_screen(s, FALSE);
    CloseScreen(s);
}

/* the pens of the screen title bar: its fill, the line below it and the
 * pens of the title text */
static void show_bar(const char *name, struct Screen *s)
{
    struct RastPort *rp;
    UBYTE seen[256];
    WORD x, y;
    int i;

    P_SECTION(name);
    if (!s)
    {
        probe_s("OpenScreen = NULL\n");
        return;
    }
    dump_screen(s, FALSE);
    Delay(5);
    rp = &s->RastPort;
    probe_s(" bar");
    ps_kv("fill", ReadPixel(rp, 300, 1));
    ps_kv("line", ReadPixel(rp, 300, s->BarHeight));
    ps_kv("below", ReadPixel(rp, 300, s->BarHeight + 1));
    memset(seen, 0, sizeof(seen));
    for (y = 0; y < s->BarHeight; y++)
        for (x = 0; x < 48; x++)
            seen[ReadPixel(rp, x, y) & 255] = 1;
    probe_s(" titlepens");
    for (i = 0; i < 256; i++)
        if (seen[i])
        {
            probe_ch(' ');
            probe_dec(i);
        }
    probe_ch('\n');
    CloseScreen(s);
}

int main(void)
{
    struct TextAttr t8 = { (STRPTR)"topaz.font", 8, 0, 0 };
    struct NewScreen ns;
    struct Screen *wb;
    UWORD pens[] = { (UWORD)~0 };

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 37);
    if (!IntuitionBase || !GfxBase || !DiskfontBase)
        return 20;

    P_SECTION("Workbench");
    wb = LockPubScreen(NULL);
    if (wb)
    {
        dump_screen(wb, TRUE);
        UnlockPubScreen(NULL, wb);
    }

    memset(&ns, 0, sizeof(ns));
    ns.Width = 640; ns.Height = 200; ns.Depth = 2;
    ns.DetailPen = 0; ns.BlockPen = 1;
    ns.ViewModes = HIRES;
    ns.Type = CUSTOMSCREEN;
    show("NewScreen plain", OpenScreen(&ns));

    ns.Type = CUSTOMSCREEN | SHOWTITLE;
    ns.DefaultTitle = (UBYTE *)"Old";
    show("NewScreen showtitle title", OpenScreen(&ns));

    ns.Type = CUSTOMSCREEN;
    ns.Font = &t8;
    ns.ViewModes = 0;
    ns.Width = 320; ns.Height = 256; ns.Depth = 3;
    show("NewScreen lores", OpenScreen(&ns));

    ns.Font = &t8;
    ns.ViewModes = HIRES | LACE;
    ns.Width = 640; ns.Height = 512; ns.Depth = 1;
    show("NewScreen interlace depth1", OpenScreen(&ns));

    ns.Width = 640; ns.Height = 480; ns.ViewModes = 0; ns.Depth = 2;
    show("NewScreen 640x480 viewmodes 0", OpenScreen(&ns));

    ns.Width = 0; ns.Height = 0; ns.ViewModes = 0; ns.Depth = 2;
    show("NewScreen size 0 viewmodes 0", OpenScreen(&ns));
    ns.Height = -3;
    show("NewScreen width 0 height -3", OpenScreen(&ns));
    {
        struct ExtNewScreen ens;
        struct TagItem ext[] = { { SA_Pens, (ULONG)pens }, { TAG_DONE, 0 } };
        struct TagItem noext[] = { { TAG_DONE, 0 } };

        memcpy(&ens, &ns, sizeof(ns));
        ens.Type |= NS_EXTENDED;
        ens.Extension = ext;
        show("ExtNewScreen width 0 height -3 pens", OpenScreen((struct NewScreen *)&ens));
        ens.Extension = noext;
        show("ExtNewScreen width 0 height -3 no tags", OpenScreen((struct NewScreen *)&ens));
        ens.Width = 320; ens.Height = 200;
        ens.Extension = ext;
        show("ExtNewScreen 320x200 pens", OpenScreen((struct NewScreen *)&ens));
    }

    ns.Width = STDSCREENWIDTH; ns.Height = STDSCREENHEIGHT; ns.ViewModes = HIRES; ns.Depth = 2;
    ns.Type = CUSTOMSCREEN | SCREENQUIET;
    show("NewScreen std quiet", OpenScreen(&ns));

    show("tags default", OpenScreenTags(NULL, SA_Depth, 2, TAG_END));
    show("tags pens hires", OpenScreenTags(NULL, SA_Depth, 2, SA_DisplayID, HIRES_KEY, SA_Pens, (ULONG)pens,
                               SA_Title, (ULONG)"Tags", TAG_END));
    show("tags showtitle false", OpenScreenTags(NULL, SA_Depth, 2, SA_DisplayID, HIRES_KEY, SA_Pens, (ULONG)pens,
                                    SA_ShowTitle, FALSE, TAG_END));
    show("tags sysfont1", OpenScreenTags(NULL, SA_Depth, 2, SA_DisplayID, HIRES_KEY, SA_Pens, (ULONG)pens,
                             SA_SysFont, 1, TAG_END));
    show("tags lores", OpenScreenTags(NULL, SA_Depth, 4, SA_DisplayID, LORES_KEY, SA_Pens, (ULONG)pens, TAG_END));
    show("tags lace", OpenScreenTags(NULL, SA_Depth, 2, SA_DisplayID, HIRESLACE_KEY, SA_Pens, (ULONG)pens, TAG_END));
    show("tags size", OpenScreenTags(NULL, SA_Depth, 2, SA_DisplayID, HIRES_KEY, SA_Left, 0, SA_Top, 20, SA_Width, 400,
                         SA_Height, 150, SA_Pens, (ULONG)pens, TAG_END));
    show("tags oldlook", OpenScreenTags(NULL, SA_Depth, 2, SA_DisplayID, HIRES_KEY, TAG_END));
    show("tags behind quiet", OpenScreenTags(NULL, SA_Depth, 2, SA_DisplayID, HIRES_KEY, SA_Pens, (ULONG)pens,
                                 SA_Behind, TRUE, SA_Quiet, TRUE, TAG_END));
    show("tags draggable no", OpenScreenTags(NULL, SA_Depth, 2, SA_DisplayID, HIRES_KEY, SA_Pens, (ULONG)pens,
                                 SA_Draggable, FALSE, SA_Exclusive, TRUE, TAG_END));

    /* Phase 234: the bar pens of screens with other DetailPen/BlockPen
     * (BlitzBasic 2 opens its editor screen like this) */
    memset(&ns, 0, sizeof(ns));
    ns.Width = 640; ns.Height = 200; ns.Depth = 2;
    ns.ViewModes = HIRES;
    ns.Type = CUSTOMSCREEN | SHOWTITLE;
    ns.DefaultTitle = (UBYTE *)"Pens";
    ns.DetailPen = 1; ns.BlockPen = 2;
    show_bar("NewScreen detail 1 block 2", OpenScreen(&ns));
    ns.DetailPen = 3; ns.BlockPen = 2;
    show_bar("NewScreen detail 3 block 2", OpenScreen(&ns));
    ns.DetailPen = 2; ns.BlockPen = 0;
    show_bar("NewScreen detail 2 block 0", OpenScreen(&ns));
    show_bar("tags oldlook detail 3 block 1",
             OpenScreenTags(NULL, SA_Depth, 2, SA_DisplayID, HIRES_KEY, SA_DetailPen, 3, SA_BlockPen, 1,
                            SA_Title, (ULONG)"Pens", TAG_END));
    show_bar("tags pens detail 3 block 1",
             OpenScreenTags(NULL, SA_Depth, 2, SA_DisplayID, HIRES_KEY, SA_DetailPen, 3, SA_BlockPen, 1,
                            SA_Pens, (ULONG)pens, SA_Title, (ULONG)"Pens", TAG_END));

    CloseLibrary(DiskfontBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
