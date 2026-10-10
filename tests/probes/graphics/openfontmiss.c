/*
 * Probe (Phase 244): graphics OpenFont() for fonts that are not in memory.
 * ACE's AIDE asks for thinpaz.font and centres its alert text with the
 * result; AmigaOS 3.1 returns NULL for an unknown name.
 */
#include <exec/types.h>
#include <graphics/text.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct GfxBase *GfxBase;

static void try(const char *name, UWORD size, UBYTE style, UBYTE flags)
{
    struct TextAttr ta;
    struct TextFont *f;

    ta.ta_Name = (STRPTR)name;
    ta.ta_YSize = size;
    ta.ta_Style = style;
    ta.ta_Flags = flags;
    f = OpenFont(&ta);
    probe_s(name);
    probe_ch(' ');
    probe_dec(size);
    if (!f)
        probe_s(": NULL\n");
    else
    {
        probe_s(": ");
        probe_s(f->tf_Message.mn_Node.ln_Name);
        probe_ch(' ');
        probe_dec(f->tf_YSize);
        probe_s(" x ");
        probe_dec(f->tf_XSize);
        probe_ch('\n');
        CloseFont(f);
    }
}

int main(void)
{
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 37);
    if (!GfxBase)
        return 0;
    try("thinpaz.font", 8, 0, 0);
    try("thinpaz.font", 8, 0, FPF_ROMFONT);
    try("nosuch.font", 11, 0, 0);
    try("topaz.font", 8, 0, 0);
    try("topaz.font", 9, 0, 0);
    try("topaz.font", 11, 0, 0);
    try("topaz.font", 8, FSF_BOLD, 0);
    try("topaz.font", 8, 0, FPF_DESIGNED);
    try("topaz.font", 0, 0, 0);
    CloseLibrary((struct Library *)GfxBase);
    return 0;
}
