/*
 * Probe (Phase 237c): which scratch registers the text functions of
 * graphics.library leave intact.  Fred Fish CopperPrefs (disk 769) calls
 * TextFit() and then takes the length of the string at A0 - the program
 * works on AmigaOS 3.1, so A0 must still point into the string there.
 * Prints the result and what A0 holds afterwards relative to the inputs
 * (D1 and A1 hold scratch values that differ between implementations).
 */
#include <graphics/text.h>
#include <clib/graphics_protos.h>
#include "gfxprobe.h"

/* gcall(lvo, in[8] = d0 d1 d2 d3 a0 a1 a2 a3, out[4] = d0 d1 a0 a1) */
void gcall(LONG lvo, ULONG *in, ULONG *out);
__asm__(
    "    .text\n"
    "    .globl _gcall\n"
    "_gcall:\n"
    "    movem.l d2-d7/a2-a6,-(sp)\n"
    "    move.l 52(sp),a0\n"
    "    move.l 48(sp),d0\n"
    "    move.l _GfxBase,a6\n"
    "    lea 0(a6,d0.l),a4\n"
    "    movem.l (a0),d0-d3/a0-a3\n"
    "    jsr (a4)\n"
    "    move.l 56(sp),a4\n"
    "    movem.l d0-d1/a0-a1,(a4)\n"
    "    movem.l (sp)+,d2-d7/a2-a6\n"
    "    rts\n");

static struct RastPort rp;
static struct TextExtent te __attribute__((aligned(4)));
static char str[] = "Could not open ENV:Sys/Copper.prefs !";

static void show(const char *reg, ULONG v, ULONG *in)
{
    probe_s(reg);
    if (v == in[0]) probe_s("=d0in");
    else if (v == in[1]) probe_s("=d1in");
    else if (v == in[4]) probe_s("=a0in");
    else if (v == in[5]) probe_s("=a1in");
    else if (v == in[6]) probe_s("=a2in");
    else if (v >= (ULONG)str && v <= (ULONG)str + sizeof(str)) {
        probe_s("=str+");
        probe_dec((LONG)(v - (ULONG)str));
    } else
        probe_s(" other");
}

static void t(const char *name, LONG lvo, ULONG d0, ULONG d1, ULONG d2, ULONG d3,
              ULONG a0, ULONG a1, ULONG a2, ULONG a3)
{
    ULONG in[8], out[4];
    in[0] = d0; in[1] = d1; in[2] = d2; in[3] = d3;
    in[4] = a0; in[5] = a1; in[6] = a2; in[7] = a3;
    gcall(lvo, in, out);
    probe_s(name);
    if (lvo != -690) {          /* TextExtent() is VOID */
        probe_s(": d0 ");
        probe_dec((LONG)out[0]);
    } else
        probe_ch(':');
    show(" a0", out[2], in);     /* D1/A1: different scratch values */
    probe_ch('\n');
}

int main(void)
{
    struct TextAttr ta;
    struct TextFont *f;
    ta.ta_Name = (STRPTR)"topaz.font";
    ta.ta_YSize = 8;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    f = OpenFont(&ta);
    InitRastPort(&rp);
    if (f)
        SetFont(&rp, f);
    t("TextFit", -696, 37, 1, 79, 8, (ULONG)str, (ULONG)&rp, (ULONG)&te, 0);
    t("TextFit (all)", -696, 4, 1, 200, 8, (ULONG)str, (ULONG)&rp, (ULONG)&te, 0);
    t("TextFit (empty)", -696, 0, 1, 79, 8, (ULONG)str, (ULONG)&rp, (ULONG)&te, 0);
    t("TextFit (narrow)", -696, 10, 1, 3, 8, (ULONG)str, (ULONG)&rp, (ULONG)&te, 0);
    t("TextFit (back)", -696, 10, (ULONG)-1, 40, 8, (ULONG)str + 20, (ULONG)&rp, (ULONG)&te, 0);
    t("TextLength", -54, 10, 0x13579bdf, 0, 0, (ULONG)str, (ULONG)&rp, 0, 0);
    t("TextExtent", -690, 10, 0x13579bdf, 0, 0, (ULONG)str, (ULONG)&rp, (ULONG)&te, 0);
    if (f)
        CloseFont(f);
    return 0;
}
