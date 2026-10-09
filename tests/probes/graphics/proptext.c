/*
 * Probe (Phase 244): text metrics of proportional disk fonts.  FinalWriter
 * centres its gadget labels with TextLength() in its own proportional
 * screen font; the window and button widths differ on lxa.
 * rdd: fonts wb31
 */
#include <exec/types.h>
#include <graphics/text.h>
#include <graphics/rastport.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/diskfont.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct GfxBase *GfxBase;
struct Library *DiskfontBase;

static const char *strs[] = { "OK", "Abbruch", "8", "Workbench verwenden", "Neue Anzeige oeffnen",
                              "iiii", "WWWW", " ", "Wie Workbench", NULL };

static void font(const char *name, UWORD size)
{
    struct TextAttr ta = { (STRPTR)name, size, 0, 0 };
    struct TextFont *f = OpenDiskFont(&ta);
    struct RastPort rp;
    int i;

    probe_s("-- ");
    probe_s(name);
    probe_ch(' ');
    probe_dec(size);
    probe_ch('\n');
    if (!f)
    {
        probe_s("not found\n");
        return;
    }
    probe_s("ysize ");
    probe_dec(f->tf_YSize);
    probe_s(" xsize ");
    probe_dec(f->tf_XSize);
    probe_s(" flags ");
    probe_hex(f->tf_Flags, 2);
    probe_s(" style ");
    probe_hex(f->tf_Style, 2);
    probe_s(" lo ");
    probe_dec(f->tf_LoChar);
    probe_s(" hi ");
    probe_dec(f->tf_HiChar);
    probe_s(f->tf_CharSpace ? " space" : " nospace");
    probe_s(f->tf_CharKern ? " kern" : " nokern");
    probe_ch('\n');
    InitRastPort(&rp);
    SetFont(&rp, f);
    for (i = 0; strs[i]; i++)
    {
        struct TextExtent te;
        UWORD n = 0;
        while (strs[i][n])
            n++;
        TextExtent(&rp, (STRPTR)strs[i], n, &te);
        probe_ch('"');
        probe_s(strs[i]);
        probe_s("\" len ");
        probe_dec(TextLength(&rp, (STRPTR)strs[i], n));
        probe_s(" extent ");
        probe_dec(te.te_Width);
        probe_ch(' ');
        probe_dec(te.te_Extent.MinX);
        probe_ch(',');
        probe_dec(te.te_Extent.MinY);
        probe_ch(',');
        probe_dec(te.te_Extent.MaxX);
        probe_ch(',');
        probe_dec(te.te_Extent.MaxY);
        probe_ch('\n');
    }
    CloseFont(f);
}

int main(void)
{
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 37);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 37);
    if (!GfxBase || !DiskfontBase)
        return 0;
    font("helvetica.font", 11);
    font("helvetica.font", 13);
    font("times.font", 11);
    font("courier.font", 13);
    font("garnet.font", 9);
    CloseLibrary(DiskfontBase);
    CloseLibrary((struct Library *)GfxBase);
    return 0;
}
