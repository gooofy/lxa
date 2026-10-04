/*
 * IPREFS - install the system preferences (roadmap Phase 236)
 *
 * Reads the preferences files in ENV:Sys - written by the Prefs editors,
 * IFF FORM PREF as described by the NDK prefs/ headers - and installs
 * them in the system, as the AmigaOS 3.1 IPrefs does at boot:
 *
 *   input.prefs       default keymap (KEYMAPS:<name>), Preferences
 *                     (double-click, key repeat, pointer ticks, mouse
 *                     acceleration) and input.device key repeat
 *   locale.prefs      the default locale (locale.library)
 *   overscan.prefs    text and standard overscan of the display database
 *   font.prefs        system default font and screen font (the Workbench
 *                     icon font belongs to the Workbench - Phase 264)
 *   palette.prefs     Workbench colours and the 4/8 colour pens
 *   pointer.prefs     normal and busy pointer, pointer colours
 *   icontrol.prefs    Intuition control settings
 *   screenmode.prefs  the Workbench screen mode, size and depth
 *
 * wbpattern.prefs only concerns the Workbench windows (LoadWB, Phase 264)
 * and is not read.  Intuition receives its part through the private
 * SetIPrefs() (lxa_iprefs.h), the default locale through the private
 * LocalePrefsUpdate().  lxa runs IPrefs when it boots a program and ENV:Sys
 * exists (exec bootstrap); like 3.1's IPrefs it prints nothing.
 *
 * Unlike 3.1's IPrefs it does not stay resident to watch ENV:Sys for
 * changes: run it again after changing preferences.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <dos/dos.h>
#include <devices/input.h>
#include <devices/timer.h>
#include <devices/keymap.h>
#include <devices/console.h>
#include <graphics/gfx.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/preferences.h>
#include <libraries/iffparse.h>
#include <libraries/locale.h>
#include <prefs/prefhdr.h>
#include <prefs/font.h>
#include <prefs/palette.h>
#include <prefs/input.h>
#include <prefs/locale.h>
#include <prefs/overscan.h>
#include <prefs/screenmode.h>
#include <prefs/icontrol.h>
#include <prefs/pointer.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/diskfont_protos.h>
#include <clib/keymap_protos.h>
#include <clib/locale_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include <inline/diskfont.h>
#include <inline/keymap.h>
#include <inline/locale.h>

#include <string.h>

#include "lxa_iprefs.h"

#ifndef CONU_LIBRARY
#define CONU_LIBRARY (-1)
#endif

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *DiskfontBase;
struct Library *KeymapBase;
struct Library *LocaleBase;

/* intuition.library SetIPrefs() (-576, private) */
static ULONG set_iprefs(APTR data, ULONG length, ULONG type)
{
    register ULONG d0 __asm("d0") = length;
    register ULONG d1 __asm("d1") = type;
    register APTR a0 __asm("a0") = data;
    register struct IntuitionBase *a6 __asm("a6") = IntuitionBase;

    __asm__ __volatile__ ("jsr -576(%%a6)"
                          : "+r" (d0), "+r" (d1), "+r" (a0)
                          : "r" (a6)
                          : "a1", "cc", "memory");
    return d0;
}

/* locale.library LocalePrefsUpdate() (-168, private) */
static struct Locale *locale_prefs_update(struct Locale *locale)
{
    register struct Locale *a0 __asm("a0") = locale;
    register ULONG d0 __asm("d0");
    register struct Library *a6 __asm("a6") = LocaleBase;

    __asm__ __volatile__ ("jsr -168(%%a6)"
                          : "=r" (d0), "+r" (a0)
                          : "r" (a6)
                          : "d1", "a1", "cc", "memory");
    return (struct Locale *)d0;
}

/* ------------------------------------------------------------------------ */
/* reading a prefs file                                                     */
/* ------------------------------------------------------------------------ */

struct PrefsFile {
    UBYTE *data;
    ULONG size;
    ULONG pos;      /* next chunk */
};

static ULONG get_long(const UBYTE *p)
{
    return ((ULONG)p[0] << 24) | ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) | p[3];
}

static UWORD get_word(const UBYTE *p)
{
    return (UWORD)((p[0] << 8) | p[1]);
}

/* load ENV:Sys/<name>.prefs; FALSE when it is missing or no FORM PREF */
static BOOL prefs_open(struct PrefsFile *pf, const char *name)
{
    char path[64];
    BPTR fh;
    LONG len;

    strcpy(path, "ENV:Sys/");
    strcat(path, name);
    strcat(path, ".prefs");
    pf->data = NULL;
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh)
        return FALSE;
    Seek(fh, 0, OFFSET_END);
    len = Seek(fh, 0, OFFSET_BEGINNING);
    if (len >= 12 && len < 65536)
        pf->data = AllocVec(len, MEMF_PUBLIC);
    if (pf->data && Read(fh, pf->data, len) != len) {
        FreeVec(pf->data);
        pf->data = NULL;
    }
    Close(fh);
    if (!pf->data)
        return FALSE;
    pf->size = len;
    pf->pos = 12;
    if (get_long(pf->data) != ID_FORM || get_long(pf->data + 8) != ID_PREF) {
        FreeVec(pf->data);
        pf->data = NULL;
        return FALSE;
    }
    if (get_long(pf->data + 4) + 8 < pf->size)
        pf->size = get_long(pf->data + 4) + 8;
    return TRUE;
}

/* next chunk: its ID, *body and *len; 0 at the end */
static ULONG prefs_next(struct PrefsFile *pf, UBYTE **body, ULONG *len)
{
    ULONG id, n;

    if (pf->pos + 8 > pf->size)
        return 0;
    id = get_long(pf->data + pf->pos);
    n = get_long(pf->data + pf->pos + 4);
    if (pf->pos + 8 + n > pf->size)
        return 0;
    *body = pf->data + pf->pos + 8;
    *len = n;
    pf->pos += 8 + ((n + 1) & ~1UL);
    return id;
}

static void prefs_close(struct PrefsFile *pf)
{
    if (pf->data)
        FreeVec(pf->data);
    pf->data = NULL;
}

/* ------------------------------------------------------------------------ */
/* input.prefs                                                              */
/* ------------------------------------------------------------------------ */

static struct KeyMap *open_keymap(const char *name)
{
    struct KeyMapResource *kr = (struct KeyMapResource *)OpenResource((STRPTR)"keymap.resource");
    struct KeyMapNode *kmn = NULL;
    struct Node *n;
    char path[64];
    BPTR seg;

    if (!name[0])
        return NULL;
    if (kr) {
        Forbid();
        for (n = kr->kr_List.lh_Head; n->ln_Succ; n = n->ln_Succ)
            if (n->ln_Name && !stricmp(n->ln_Name, name)) {
                kmn = (struct KeyMapNode *)n;
                break;
            }
        Permit();
        if (kmn)
            return &kmn->kn_KeyMap;
    }
    /* the ROM keymap */
    if (!stricmp(name, "usa"))
        return NULL;
    /* a keymap file: its first hunk starts with the KeyMapNode */
    strcpy(path, "KEYMAPS:");
    strncat(path, name, sizeof(path) - strlen(path) - 1);
    seg = LoadSeg((STRPTR)path);
    if (!seg) {
        strcpy(path, "DEVS:Keymaps/");
        strncat(path, name, sizeof(path) - strlen(path) - 1);
        seg = LoadSeg((STRPTR)path);
    }
    if (!seg)
        return NULL;
    kmn = (struct KeyMapNode *)((UBYTE *)BADDR(seg) + sizeof(BPTR));
    if (!kmn->kn_Node.ln_Name || stricmp(kmn->kn_Node.ln_Name, name)) {
        UnLoadSeg(seg);
        return NULL;
    }
    /* the keymap stays loaded, listed in keymap.resource */
    if (kr) {
        Forbid();
        AddTail(&kr->kr_List, &kmn->kn_Node);
        Permit();
    }
    return &kmn->kn_KeyMap;
}

static void device_cmd(const char *dev, LONG unit, UWORD cmd, APTR data, ULONG length,
                       const struct timeval *tv)
{
    struct MsgPort *port = CreateMsgPort();
    struct timerequest *io = port ? (struct timerequest *)CreateIORequest(port, sizeof(*io)) : NULL;

    if (io && !OpenDevice((STRPTR)dev, unit, (struct IORequest *)io, 0)) {
        io->tr_node.io_Command = cmd;
        if (tv) {
            io->tr_time = *tv;
        } else {
            ((struct IOStdReq *)io)->io_Data = data;
            ((struct IOStdReq *)io)->io_Length = length;
        }
        DoIO((struct IORequest *)io);
        CloseDevice((struct IORequest *)io);
    }
    if (io)
        DeleteIORequest((struct IORequest *)io);
    if (port)
        DeleteMsgPort(port);
}

static void input_prefs(void)
{
    struct PrefsFile pf;
    UBYTE *b;
    ULONG len, id;

    if (!prefs_open(&pf, "input"))
        return;
    while ((id = prefs_next(&pf, &b, &len)))
    {
        struct Preferences p;
        struct timeval delay, speed;
        char keymap[17];
        struct KeyMap *km;

        if (id != ID_INPT || len < 44)
            continue;
        memcpy(keymap, b, 16);
        keymap[16] = 0;
        if (KeymapBase && (km = open_keymap(keymap))) {
            SetKeyMapDefault(km);
            device_cmd("console.device", CONU_LIBRARY, CD_SETDEFAULTKEYMAP, km, sizeof(*km), NULL);
        }

        delay.tv_secs = get_long(b + 26);
        delay.tv_micro = get_long(b + 30);
        speed.tv_secs = get_long(b + 34);
        speed.tv_micro = get_long(b + 38);
        GetPrefs(&p, sizeof(p));
        p.PointerTicks = get_word(b + 16);
        p.DoubleClick.tv_secs = get_long(b + 18);
        p.DoubleClick.tv_micro = get_long(b + 22);
        p.KeyRptDelay = delay;
        p.KeyRptSpeed = speed;
        if (get_word(b + 42))
            p.EnableCLI |= MOUSE_ACCEL;
        else
            p.EnableCLI &= ~MOUSE_ACCEL;
        SetPrefs(&p, sizeof(p), FALSE);
        device_cmd("input.device", 0, IND_SETTHRESH, NULL, 0, &delay);
        device_cmd("input.device", 0, IND_SETPERIOD, NULL, 0, &speed);
    }
    prefs_close(&pf);
}

/* ------------------------------------------------------------------------ */
/* locale.prefs                                                             */
/* ------------------------------------------------------------------------ */

static void locale_prefs(void)
{
    struct Locale *loc, *old;
    BPTR lock;

    if (!LocaleBase || !(lock = Lock((STRPTR)"ENV:Sys/locale.prefs", ACCESS_READ)))
        return;
    UnLock(lock);
    loc = OpenLocale((STRPTR)"ENV:Sys/locale.prefs");
    if (!loc)
        return;
    old = locale_prefs_update(loc);
    if (old)
        CloseLocale(old);
}

/* ------------------------------------------------------------------------ */
/* overscan.prefs, screenmode.prefs, icontrol.prefs                         */
/* ------------------------------------------------------------------------ */

static void overscan_prefs(void)
{
    struct PrefsFile pf;
    UBYTE *b;
    ULONG len, id;

    if (!prefs_open(&pf, "overscan"))
        return;
    while ((id = prefs_next(&pf, &b, &len)))
    {
        struct LxaIOverscanPrefs op;

        if (id != ID_OSCN || len < 36)
            continue;
        op.os_DisplayID = get_long(b + 16);
        op.os_ViewPos.x = (WORD)get_word(b + 20);
        op.os_ViewPos.y = (WORD)get_word(b + 22);
        op.os_Text.x = (WORD)get_word(b + 24);
        op.os_Text.y = (WORD)get_word(b + 26);
        op.os_Standard.MinX = (WORD)get_word(b + 28);
        op.os_Standard.MinY = (WORD)get_word(b + 30);
        op.os_Standard.MaxX = (WORD)get_word(b + 32);
        op.os_Standard.MaxY = (WORD)get_word(b + 34);
        set_iprefs(&op, sizeof(op), LXA_IPREFS_OVERSCAN);
    }
    prefs_close(&pf);
}

static void screenmode_prefs(void)
{
    struct PrefsFile pf;
    UBYTE *b;
    ULONG len, id;

    if (!prefs_open(&pf, "screenmode"))
        return;
    while ((id = prefs_next(&pf, &b, &len)))
    {
        struct LxaIScreenModePrefs sm;

        if (id != ID_SCRM || len < 28)
            continue;
        sm.smp_DisplayID = get_long(b + 16);
        sm.smp_Width = get_word(b + 20);
        sm.smp_Height = get_word(b + 22);
        sm.smp_Depth = get_word(b + 24);
        sm.smp_Control = get_word(b + 26);
        set_iprefs(&sm, sizeof(sm), LXA_IPREFS_SCREENMODE);
    }
    prefs_close(&pf);
}

static void icontrol_prefs(void)
{
    struct PrefsFile pf;
    UBYTE *b;
    ULONG len, id;

    if (!prefs_open(&pf, "icontrol"))
        return;
    while ((id = prefs_next(&pf, &b, &len)))
    {
        struct LxaIIControlPrefs ic;

        if (id != ID_ICTL || len < 28)
            continue;
        ic.ic_TimeOut = get_word(b + 16);
        ic.ic_MetaDrag = (WORD)get_word(b + 18);
        ic.ic_Flags = get_long(b + 20);
        ic.ic_WBtoFront = b[24];
        ic.ic_FrontToBack = b[25];
        ic.ic_ReqTrue = b[26];
        ic.ic_ReqFalse = b[27];
        set_iprefs(&ic, sizeof(ic), LXA_IPREFS_ICONTROL);
    }
    prefs_close(&pf);
}

/* ------------------------------------------------------------------------ */
/* font.prefs                                                               */
/* ------------------------------------------------------------------------ */

static void font_prefs(void)
{
    struct PrefsFile pf;
    UBYTE *b;
    ULONG len, id;

    if (!prefs_open(&pf, "font"))
        return;
    while ((id = prefs_next(&pf, &b, &len)))
    {
        struct LxaIFontPrefs fp;
        struct TextFont *tf;
        UWORD type;

        if (id != ID_FONT || len < 156)
            continue;
        type = get_word(b + 14);
        if (type != FP_SYSFONT && type != FP_SCREENFONT)
            continue;       /* FP_WBFONT: the Workbench icon font */
        memset(&fp, 0, sizeof(fp));
        memcpy(fp.fp_Name, b + 28, sizeof(fp.fp_Name) - 1);
        fp.fp_TextAttr.ta_Name = (STRPTR)fp.fp_Name;
        fp.fp_TextAttr.ta_YSize = get_word(b + 24);
        fp.fp_TextAttr.ta_Style = b[26];
        fp.fp_TextAttr.ta_Flags = b[27];
        fp.fp_ScrFont = (type == FP_SCREENFONT);
        /* load a disk font into memory first; Intuition opens it itself */
        tf = DiskfontBase ? OpenDiskFont(&fp.fp_TextAttr) : OpenFont(&fp.fp_TextAttr);
        if (!tf)
            continue;
        set_iprefs(&fp, sizeof(fp), LXA_IPREFS_FONT);
        CloseFont(tf);
    }
    prefs_close(&pf);
}

/* ------------------------------------------------------------------------ */
/* palette.prefs                                                            */
/* ------------------------------------------------------------------------ */

static void set_pens(const UBYTE *table, UWORD type)
{
    struct LxaIPenPrefs pp;
    UWORD i;

    memset(&pp, 0, sizeof(pp));
    pp.Type = type;
    for (i = 0; i < LXA_IPREFS_NUMPENS; i++) {
        pp.PenTable[i] = get_word(table + i * 2);
        if (pp.PenTable[i] == (UWORD)~0)
            break;
    }
    pp.Count = i;
    pp.PenTable[i] = (UWORD)~0;
    set_iprefs(&pp, sizeof(pp), LXA_IPREFS_PENS);
}

static void palette_prefs(void)
{
    struct PrefsFile pf;
    UBYTE *b;
    ULONG len, id;

    if (!prefs_open(&pf, "palette"))
        return;
    while ((id = prefs_next(&pf, &b, &len)))
    {
        struct ColorSpec cs[33];
        UWORD i, n = 0;

        if (id != ID_PALT || len < 16 + 64 + 64 + 8)
            continue;
        for (i = 0; i < 32 && 16 + 128 + (i + 1) * 8 <= len; i++) {
            const UBYTE *c = b + 16 + 128 + i * 8;
            if ((WORD)get_word(c) == -1)
                break;
            cs[n].ColorIndex = (WORD)get_word(c);
            cs[n].Red = get_word(c + 2);
            cs[n].Green = get_word(c + 4);
            cs[n].Blue = get_word(c + 6);
            n++;
        }
        cs[n].ColorIndex = -1;
        set_iprefs(cs, sizeof(cs[0]) * (n + 1), LXA_IPREFS_PALETTE);
        set_pens(b + 16, 0);
        set_pens(b + 16 + 64, 1);
    }
    prefs_close(&pf);
}

/* ------------------------------------------------------------------------ */
/* pointer.prefs                                                            */
/* ------------------------------------------------------------------------ */

static void pointer_prefs(void)
{
    struct PrefsFile pf;
    UBYTE *b;
    ULONG len, id;

    if (!prefs_open(&pf, "pointer"))
        return;
    while ((id = prefs_next(&pf, &b, &len)))
    {
        struct LxaIPointerPrefs pp;
        struct BitMap bm;
        struct ColorSpec cs[4];
        UWORD width, height, depth, words, ncol, i;
        ULONG plane_size, off;
        UBYTE *planes;

        if (id != ID_PNTR || len < 32)
            continue;
        width = get_word(b + 20);
        height = get_word(b + 22);
        depth = get_word(b + 24);
        if (!width || !height || depth < 1 || depth > 2)
            continue;
        ncol = (1 << depth) - 1;
        words = (width + 15) / 16;
        plane_size = (ULONG)words * 2 * height;
        off = 32 + ncol * 3;
        if (off + plane_size * depth > len)
            continue;
        planes = AllocVec(plane_size * 2, MEMF_CHIP | MEMF_CLEAR);
        if (!planes)
            continue;
        memcpy(planes, b + off, plane_size * depth);
        InitBitMap(&bm, 2, width, height);
        bm.Planes[0] = planes;
        bm.Planes[1] = planes + plane_size;

        memset(&pp, 0, sizeof(pp));
        pp.BitMap = &bm;
        pp.XOffset = (WORD)get_word(b + 28);
        pp.YOffset = (WORD)get_word(b + 30);
        pp.BytesPerRow = words * 2;
        pp.Size = get_word(b + 18);
        pp.YSize = height;
        pp.Which = get_word(b + 16);
        set_iprefs(&pp, sizeof(pp), LXA_IPREFS_POINTER);
        FreeVec(planes);

        /* the pointer colours (colours 17-19) */
        for (i = 0; i < 3 && i < ncol; i++) {
            const UBYTE *rgb = b + 32 + i * 3;
            cs[i].ColorIndex = 8 + i;
            cs[i].Red = rgb[0] * 0x101;
            cs[i].Green = rgb[1] * 0x101;
            cs[i].Blue = rgb[2] * 0x101;
        }
        cs[i].ColorIndex = -1;
        set_iprefs(cs, sizeof(cs[0]) * (i + 1), LXA_IPREFS_PALETTE);
    }
    prefs_close(&pf);
}

int main(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase) {
        if (GfxBase)
            CloseLibrary((struct Library *)GfxBase);
        if (IntuitionBase)
            CloseLibrary((struct Library *)IntuitionBase);
        return RETURN_FAIL;
    }
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 37);
    KeymapBase = OpenLibrary((STRPTR)"keymap.library", 37);
    LocaleBase = OpenLibrary((STRPTR)"locale.library", 38);

    input_prefs();
    locale_prefs();
    overscan_prefs();
    font_prefs();
    palette_prefs();
    pointer_prefs();
    icontrol_prefs();
    screenmode_prefs();

    /* the default locale and the loaded keymaps stay in use */
    if (DiskfontBase)
        CloseLibrary(DiskfontBase);
    if (KeymapBase)
        CloseLibrary(KeymapBase);
    if (LocaleBase)
        CloseLibrary(LocaleBase);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return RETURN_OK;
}
