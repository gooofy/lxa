/*
 * GalleryPrefs - what the system preferences (ENV:Sys/<name>.prefs, applied by
 * C:IPrefs at boot) change, observed through public APIs (Phase 236).
 *
 * Prints the system default font, the Workbench screen (mode, size, font,
 * palette, pens), intuition Preferences (GetPrefs/GetDefPrefs), the overscan
 * rectangles, the default locale, the default keymap and two custom screens
 * (new look with SA_SysFont 1, old look with the default font), then opens a
 * "Prefs Gallery" window on the Workbench screen whose contents use the
 * screen font, the system font and the screen's pens.
 *
 * The scenarios tests/scenarios/gallery-prefs-*.yaml run it under different
 * prefs configurations (tests/prefs/<config>/) on lxa and real AmigaOS 3.1.
 */

#include "gallery.h"

#include <devices/keymap.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <libraries/locale.h>
#include <utility/hooks.h>
#include <clib/locale_protos.h>
#include <clib/keymap_protos.h>

struct Library *LocaleBase;
struct Library *KeymapBase;

/* FormatDate PutChar hook: a0 = hook, a1 = character; h_Data -> buffer */
__asm__(
    "    .text\n"
    "    .globl _gp_putch\n"
    "_gp_putch:\n"
    "    move.l 16(a0),a0\n"
    "    move.l (a0),d0\n"
    "    cmpi.l #120,d0\n"
    "    bcc.s 1f\n"
    "    addq.l #1,(a0)\n"
    "    move.l a1,d1\n"
    "    move.b d1,4(a0,d0.l)\n"
    "1:  rts\n");
extern ULONG gp_putch(void);

struct DateBuf {
    ULONG n;
    char buf[128];
};

static void print_font(const char *what, struct TextFont *tf)
{
    if (tf)
        printf("%s %s %d %d\n", what, tf->tf_Message.mn_Node.ln_Name ? tf->tf_Message.mn_Node.ln_Name : "(null)",
               tf->tf_YSize, tf->tf_XSize);
    else
        printf("%s (none)\n", what);
}

static void print_colors(const char *what, struct Screen *scr, int from, int to)
{
    int i;
    ULONG rgb[3];
    printf("%s", what);
    for (i = from; i < to; i++)
    {
        GetRGB32(scr->ViewPort.ColorMap, i, 1, rgb);
        printf(" %02lx%02lx%02lx", rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
    }
    printf("\n");
}

static void print_screen(const char *what, struct Screen *scr)
{
    struct DrawInfo *dri;
    int ncol = 1 << scr->RastPort.BitMap->Depth;

    printf("%s %dx%dx%d mode %08lx bar %d wbortop %d font %s/%d\n", what,
           scr->Width, scr->Height, scr->RastPort.BitMap->Depth,
           GetVPModeID(&scr->ViewPort), scr->BarHeight, scr->WBorTop,
           scr->Font && scr->Font->ta_Name ? (char *)scr->Font->ta_Name : "(null)",
           scr->Font ? scr->Font->ta_YSize : 0);
    print_font("  rpfont", scr->RastPort.Font);
    {
        int c;
        for (c = 0; c < ncol && c < 32; c += 8)
        {
            char lbl[16];
            sprintf(lbl, "  colors%d", c);
            print_colors(lbl, scr, c, c + 8 < ncol ? c + 8 : ncol);
        }
    }
    dri = GetScreenDrawInfo(scr);
    if (dri)
    {
        int i;
        printf("  pens");
        for (i = 0; i < dri->dri_NumPens && i < 12; i++)
            printf(" %d", dri->dri_Pens[i]);
        printf("\n");
        print_font("  drifont", dri->dri_Font);
        FreeScreenDrawInfo(scr, dri);
    }
}

static ULONG hash_words(const UWORD *w, int n)
{
    ULONG h = 5381;
    int i;
    for (i = 0; i < n; i++)
        h = h * 33 + w[i];
    return h;
}

static void print_prefs(const char *what, struct Preferences *p)
{
    printf("%s fontheight %d ticks %d dclick %ld.%06ld rptdelay %ld.%06ld rptspeed %ld.%06ld\n", what,
           p->FontHeight, p->PointerTicks,
           p->DoubleClick.tv_secs, p->DoubleClick.tv_micro,
           p->KeyRptDelay.tv_secs, p->KeyRptDelay.tv_micro,
           p->KeyRptSpeed.tv_secs, p->KeyRptSpeed.tv_micro);
    printf("  colors %03x %03x %03x %03x ptrcolors %03x %03x %03x\n",
           p->color0, p->color1, p->color2, p->color3, p->color17, p->color18, p->color19);
    printf("  pointer %08lx offset %d %d\n", hash_words(p->PointerMatrix, POINTERSIZE),
           p->XOffset, p->YOffset);
    {
        int i;
        printf("  matrix");
        for (i = 0; i < POINTERSIZE; i++)
            printf(" %04x", p->PointerMatrix[i]);
        printf("\n");
    }
    printf("  view %d %d init %d %d lacewb %d\n", p->ViewXOffset, p->ViewYOffset,
           p->ViewInitX, p->ViewInitY, p->LaceWB);
}

static void print_oscan(ULONG id)
{
    static const char *names[] = { "", "text", "standard", "max", "video" };
    struct Rectangle r;
    int t;
    printf("oscan %08lx", id);
    for (t = OSCAN_TEXT; t <= OSCAN_VIDEO; t++)
    {
        if (QueryOverscan(id, &r, t))
            printf(" %s %d,%d-%d,%d", names[t], r.MinX, r.MinY, r.MaxX, r.MaxY);
        else
            printf(" %s none", names[t]);
    }
    printf("\n");
}

static void print_locale(void)
{
    struct Locale *loc;
    struct DateStamp ds;
    struct Hook hook;
    struct DateBuf db;
    int i;

    if (!LocaleBase)
    {
        printf("locale none\n");
        return;
    }
    loc = OpenLocale(NULL);
    if (!loc)
    {
        printf("locale NULL\n");
        return;
    }
    printf("locale %s %s", loc->loc_LocaleName, loc->loc_LanguageName);
    for (i = 0; i < 3 && loc->loc_PrefLanguages[i]; i++)
        printf(" [%s]", loc->loc_PrefLanguages[i]);
    printf("\n");
    printf("  country %08lx tel %ld gmt %ld ms %d ct %d flags %d\n", loc->loc_CountryCode,
           loc->loc_TelephoneCode, loc->loc_GMTOffset, loc->loc_MeasuringSystem,
           loc->loc_CalendarType, loc->loc_Flags);
    printf("  formats [%s] [%s] [%s] [%s] [%s] [%s]\n", loc->loc_DateTimeFormat, loc->loc_DateFormat,
           loc->loc_TimeFormat, loc->loc_ShortDateTimeFormat, loc->loc_ShortDateFormat,
           loc->loc_ShortTimeFormat);
    printf("  numbers [%s] [%s] [%s] grouping %d mon [%s] [%s] cs [%s] [%s] [%s] digits %d %d\n",
           loc->loc_DecimalPoint, loc->loc_GroupSeparator, loc->loc_FracGroupSeparator,
           loc->loc_Grouping ? loc->loc_Grouping[0] : -1,
           loc->loc_MonDecimalPoint, loc->loc_MonGroupSeparator,
           loc->loc_MonCS, loc->loc_MonSmallCS, loc->loc_MonIntCS,
           loc->loc_MonFracDigits, loc->loc_MonIntFracDigits);
    printf("  sign [%s] %d %d %d [%s] %d %d %d\n",
           loc->loc_MonPositiveSign, loc->loc_MonPositiveSpaceSep, loc->loc_MonPositiveSignPos,
           loc->loc_MonPositiveCSPos, loc->loc_MonNegativeSign, loc->loc_MonNegativeSpaceSep,
           loc->loc_MonNegativeSignPos, loc->loc_MonNegativeCSPos);
    printf("  strings %s %s %s\n", GetLocaleStr(loc, DAY_2), GetLocaleStr(loc, MON_3), GetLocaleStr(loc, YESSTR));

    /* 2024-03-05 13:07 */
    ds.ds_Days = 8830;
    ds.ds_Minute = 13 * 60 + 7;
    ds.ds_Tick = 0;
    hook.h_Entry = (ULONG (*)())gp_putch;
    hook.h_SubEntry = 0;
    hook.h_Data = &db;
    db.n = 0;
    FormatDate(loc, loc->loc_DateTimeFormat, &ds, &hook);
    db.buf[db.n < 120 ? db.n : 120] = 0;
    printf("  date [%s]", db.buf);
    db.n = 0;
    FormatDate(loc, loc->loc_ShortDateTimeFormat, &ds, &hook);
    db.buf[db.n < 120 ? db.n : 120] = 0;
    printf(" [%s]\n", db.buf);
    CloseLocale(loc);
}

static void print_keymap(void)
{
    struct KeyMap *km;
    struct InputEvent ie;
    UBYTE buf[8];
    static const UWORD keys[] = { 0x10, 0x15, 0x31, 0x1a, 0x2a, 0x0b };
    int i;

    if (!KeymapBase)
    {
        printf("keymap none\n");
        return;
    }
    km = AskKeyMapDefault();
    printf("keymap");
    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWKEY;
    for (i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++)
    {
        WORD n;
        ie.ie_Code = keys[i];
        ie.ie_Qualifier = 0;
        n = MapRawKey(&ie, (STRPTR)buf, sizeof(buf), km);
        printf(" %02x:%s", keys[i], n == 1 && buf[0] >= 0x20 && buf[0] < 0x7f ? (char[]){ buf[0], 0 } : "?");
    }
    printf("\n");
}

/* the font of a window's RastPort on a screen */
static void print_window_font(struct Screen *scr)
{
    struct Window *w = OpenWindowTags(NULL, WA_Left, 0, WA_Top, 20, WA_Width, 100, WA_Height, 50,
                                      WA_CustomScreen, (ULONG)scr, WA_Title, (ULONG)"W",
                                      WA_DragBar, TRUE, TAG_END);
    if (w)
    {
        print_font("  winfont", w->RPort->Font);
        CloseWindow(w);
    }
}

static void print_custom_screens(void)
{
    static UWORD newlook[] = { (UWORD)~0 };
    struct Screen *scr;

    scr = OpenScreenTags(NULL, SA_Depth, 4, SA_DisplayID, HIRES_KEY, SA_Width, 640, SA_Height, 200,
                         SA_Title, (ULONG)"Prefs A", SA_Pens, (ULONG)newlook, SA_SysFont, 1,
                         SA_ShowTitle, FALSE, SA_Behind, TRUE, TAG_END);
    if (scr)
    {
        print_screen("scr-newlook", scr);
        print_window_font(scr);
        CloseScreen(scr);
    }
    scr = OpenScreenTags(NULL, SA_Depth, 5, SA_DisplayID, 0, SA_Width, 320, SA_Height, 200,
                         SA_Title, (ULONG)"Prefs B", SA_ShowTitle, FALSE, SA_Behind, TRUE, TAG_END);
    if (scr)
    {
        print_screen("scr-old", scr);
        print_window_font(scr);
        CloseScreen(scr);
    }
}

int main(void)
{
    struct Screen *wb;
    struct Preferences prefs;
    struct GfxBase *gb;
    struct Window *win;
    struct Gadget *glist = NULL, *gad;
    APTR vi;
    struct NewGadget ng;
    WORD fh, y;
    static STRPTR cyc[] = { (STRPTR)"Alpha", (STRPTR)"Beta", NULL };

    if (!gallery_open_libs())
        return 20;
    LocaleBase = OpenLibrary((STRPTR)"locale.library", 38);
    KeymapBase = OpenLibrary((STRPTR)"keymap.library", 37);
    gb = (struct GfxBase *)GfxBase;

    print_font("sysfont", gb->DefaultFont);
    wb = LockPubScreen((STRPTR)"Workbench");
    if (!wb)
    {
        printf("no Workbench\n");
        return 10;
    }
    print_screen("wb", wb);
    GetPrefs(&prefs, sizeof(prefs));
    print_prefs("prefs", &prefs);
    GetDefPrefs(&prefs, sizeof(prefs));
    print_prefs("defprefs", &prefs);
    print_oscan(PAL_MONITOR_ID | HIRES_KEY);
    print_oscan(HIRES_KEY);
    print_locale();
    print_keymap();
    print_custom_screens();
    fflush(stdout);

    fh = wb->Font ? wb->Font->ta_YSize : 8;
    vi = GetVisualInfo(wb, TAG_END);
    gad = CreateContext(&glist);
    y = wb->WBorTop + fh + 1 + 6;
    memset(&ng, 0, sizeof(ng));
    ng.ng_TextAttr = wb->Font;
    ng.ng_VisualInfo = vi;
    ng.ng_LeftEdge = 100;
    ng.ng_TopEdge = y;
    ng.ng_Width = 120;
    ng.ng_Height = fh + 6;
    ng.ng_GadgetText = (STRPTR)"_Button";
    ng.ng_GadgetID = 1;
    ng.ng_Flags = PLACETEXT_IN;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, GT_Underscore, '_', TAG_END);
    ng.ng_TopEdge += fh + 10;
    ng.ng_GadgetText = (STRPTR)"Check";
    ng.ng_GadgetID = 2;
    ng.ng_Flags = PLACETEXT_LEFT;
    gad = CreateGadget(CHECKBOX_KIND, gad, &ng, GTCB_Checked, TRUE, TAG_END);
    ng.ng_TopEdge += fh + 10;
    ng.ng_GadgetText = (STRPTR)"Cycle";
    ng.ng_GadgetID = 3;
    gad = CreateGadget(CYCLE_KIND, gad, &ng, GTCY_Labels, (ULONG)cyc, TAG_END);
    ng.ng_TopEdge += fh + 10;
    ng.ng_GadgetText = (STRPTR)"String";
    ng.ng_GadgetID = 4;
    gad = CreateGadget(STRING_KIND, gad, &ng, GTST_String, (ULONG)"Text 123", TAG_END);
    y = ng.ng_TopEdge + fh + 12;

    win = OpenWindowTags(NULL,
        WA_Left, 10, WA_Top, 20, WA_Width, 360, WA_Height, y + 3 * (fh + 4) + 24,
        WA_Title, (ULONG)"Prefs Gallery",
        WA_PubScreen, (ULONG)wb,
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE,
        WA_Activate, TRUE, WA_Gadgets, (ULONG)glist,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | BUTTONIDCMP | CHECKBOXIDCMP,
        TAG_END);
    if (win)
    {
        struct RastPort rp;
        int p;

        GT_RefreshWindow(win, NULL);
        gallery_label(win, 240, y - fh - 12, "Screen font");
        /* the system default font on a copy of the window's RastPort */
        rp = *win->RPort;
        SetFont(&rp, gb->DefaultFont);
        SetAPen(&rp, 1);
        SetDrMd(&rp, JAM1);
        Move(&rp, 10, y + rp.TxBaseline);
        Text(&rp, (STRPTR)"System font 0123", 16);
        for (p = 0; p < 8 && p < (1 << wb->RastPort.BitMap->Depth); p++)
        {
            SetAPen(win->RPort, p);
            RectFill(win->RPort, 10 + p * 20, y + fh + 6, 10 + p * 20 + 15, y + fh + 6 + 10);
        }
        gallery_add_window(win);
    }
    UnlockPubScreen(NULL, wb);

    gallery_wait();
    gallery_close_windows();
    FreeGadgets(glist);
    FreeVisualInfo(vi);
    if (KeymapBase) CloseLibrary(KeymapBase);
    if (LocaleBase) CloseLibrary(LocaleBase);
    gallery_close_libs();
    return 0;
}
