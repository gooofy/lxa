/*
 * Probe (Phase 237d): OpenDiskFont() of a font that is not in FONTS: but
 * in the current directory.  Fred Fish vt100 (disk 138) keeps j132.font
 * next to the program and opens it by its plain name.  The probe copies
 * courier.font and courier/13 to T:probe-curdir/ as xcour.font, makes
 * that the current directory and opens "xcour.font" 13.
 *
 * rdd: fonts wb31
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <graphics/text.h>
#include <diskfont/diskfont.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/graphics.h>
#include <inline/diskfont.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *DiskfontBase;
struct GfxBase *GfxBase;

static BOOL copy(const char *from, const char *to)
{
    BPTR a = Open((STRPTR)from, MODE_OLDFILE), b;
    UBYTE *buf;
    LONG n;
    if (!a)
        return FALSE;
    b = Open((STRPTR)to, MODE_NEWFILE);
    buf = AllocMem(4096, MEMF_ANY);
    if (b && buf)
        while ((n = Read(a, buf, 4096)) > 0)
            Write(b, buf, n);
    if (buf)
        FreeMem(buf, 4096);
    if (b)
        Close(b);
    Close(a);
    return b != 0;
}

static void t(const char *name)
{
    struct TextAttr ta;
    struct TextFont *f;
    ta.ta_Name = (STRPTR)name;
    ta.ta_YSize = 13;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    f = OpenDiskFont(&ta);
    probe_s(name);
    if (!f)
        probe_s(": NULL\n");
    else {
        probe_s(": ok, ysize ");
        probe_dec(f->tf_YSize);
        probe_ch('\n');
        CloseFont(f);
    }
}

int main(void)
{
    BPTR l, old;

    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 0);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    if (!DiskfontBase || !GfxBase)
        return 20;
    l = CreateDir((STRPTR)"T:probe-curdir");
    if (l)
        UnLock(l);
    l = CreateDir((STRPTR)"T:probe-curdir/courier");
    if (l)
        UnLock(l);
    if (!copy("FONTS:courier.font", "T:probe-curdir/xcour.font") ||
        !copy("FONTS:courier/13", "T:probe-curdir/courier/13")) {
        probe_s("no FONTS:courier\n");
        return 0;
    }
    l = Lock((STRPTR)"T:probe-curdir", SHARED_LOCK);
    old = CurrentDir(l);
    t("xcour.font");
    t("T:probe-curdir/xcour.font");
    CurrentDir(old);
    UnLock(l);
    DeleteFile((STRPTR)"T:probe-curdir/courier/13");
    DeleteFile((STRPTR)"T:probe-curdir/courier");
    DeleteFile((STRPTR)"T:probe-curdir/xcour.font");
    DeleteFile((STRPTR)"T:probe-curdir");
    return 0;
}
