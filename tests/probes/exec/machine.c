/*
 * Probe (Phase 240): the machine identity field dump.
 *
 * Programs decide what machine they run on by reading ExecBase and GfxBase
 * fields (AttnFlags, ChipRevBits0, DisplayFlags, ...), the display database
 * entry of the default monitor and the Workbench screen.  The reference is
 * an A4000/040 with AGA (roadmap "Canonical Reference Configuration"), and
 * lxa must present the same machine, field by field.
 *
 * Only values that do not depend on timing or on allocation addresses are
 * printed; pointers are reduced to NULL/non-NULL.
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <graphics/gfxbase.h>
#include <graphics/displayinfo.h>
#include <graphics/monitor.h>
#include <graphics/modeid.h>
#include <graphics/view.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;

static void P_UHEX(const char *label, ULONG v, int digits)
{
    probe_s(label);
    probe_s(" = ");
    probe_hex(v, digits);
    probe_ch('\n');
}

static void P_POINT(const char *label, Point *p)
{
    probe_s(label);
    probe_s(" = ");
    probe_dec(p->x);
    probe_ch(',');
    probe_dec(p->y);
    probe_ch('\n');
}

static void P_RECT(const char *label, struct Rectangle *r)
{
    probe_s(label);
    probe_s(" = ");
    probe_dec(r->MinX); probe_ch(','); probe_dec(r->MinY); probe_s(" - ");
    probe_dec(r->MaxX); probe_ch(','); probe_dec(r->MaxY);
    probe_ch('\n');
}

static void P_MSPC(const char *label, struct MonitorSpec *m)
{
    P_STR(label, m ? m->ms_Node.xln_Name : NULL);
}

/* CACR, VBR, TC, ITT0/1, DTT0/1 of the 68040, read in supervisor mode */
ULONG cpu_regs[7];
void read_cpu_regs(void);
__asm__("\t.text\n\t.even\n_read_cpu_regs:\n"
        "\tlea _cpu_regs,a0\n"
        "\t.short 0x4e7a,0x0002\n\tmove.l d0,(a0)+\n"   /* movec cacr,d0 */
        "\t.short 0x4e7a,0x0801\n\tmove.l d0,(a0)+\n"   /* movec vbr,d0 */
        "\t.short 0x4e7a,0x0003\n\tmove.l d0,(a0)+\n"   /* movec tc,d0 */
        "\t.short 0x4e7a,0x0004\n\tmove.l d0,(a0)+\n"   /* movec itt0,d0 */
        "\t.short 0x4e7a,0x0005\n\tmove.l d0,(a0)+\n"   /* movec itt1,d0 */
        "\t.short 0x4e7a,0x0006\n\tmove.l d0,(a0)+\n"   /* movec dtt0,d0 */
        "\t.short 0x4e7a,0x0007\n\tmove.l d0,(a0)+\n"   /* movec dtt1,d0 */
        "\trte\n");

static void cpu_fields(void)
{
    P_SECTION("CPU and ROM");
    P_UHEX("CacheControl(0,0)", CacheControl(0, 0), 8);
    if (SysBase->AttnFlags & AFF_68040) {
        Supervisor((ULONG (*)())read_cpu_regs);
        P_UHEX("CACR", cpu_regs[0], 8);
        P_BOOL("VBR != 0", cpu_regs[1] != 0);
        /* TC (0x8000 on the reference: 68040.library enables the MMU)
         * is not printed: lxa has no MMU (Phase 275) */
        P_UHEX("ITT0", cpu_regs[3], 8);
        P_UHEX("ITT1", cpu_regs[4], 8);
        P_UHEX("DTT0", cpu_regs[5], 8);
        P_UHEX("DTT1", cpu_regs[6], 8);
    }
    P_UHEX("ROM $F80000", *(volatile UWORD *)0xf80000, 4);
    P_LONG("ROM version", *(volatile UWORD *)0xf8000c);
    P_LONG("ROM revision", *(volatile UWORD *)0xf8000e);
    P_UHEX("ROM size at $FFFFEC", *(volatile ULONG *)0xffffec, 8);
}

static void exec_fields(void)
{
    struct ExecBase *e = SysBase;

    P_SECTION("ExecBase");
    P_LONG("lib_Version", e->LibNode.lib_Version);
    P_LONG("lib_Revision", e->LibNode.lib_Revision);
    P_LONG("SoftVer", e->SoftVer);
    P_BOOL("ChkBase == ~SysBase", e->ChkBase == ~(ULONG)e);
    P_NULL("ColdCapture", e->ColdCapture);
    P_NULL("CoolCapture", e->CoolCapture);
    P_NULL("WarmCapture", e->WarmCapture);
    P_UHEX("MaxLocMem", e->MaxLocMem, 8);
    P_NULL("MaxExtMem", e->MaxExtMem);
    P_LONG("Quantum", e->Quantum);
    P_UHEX("AttnFlags", e->AttnFlags, 4);
    P_BOOL("AFF_68010", e->AttnFlags & AFF_68010);
    P_BOOL("AFF_68020", e->AttnFlags & AFF_68020);
    P_BOOL("AFF_68030", e->AttnFlags & AFF_68030);
    P_BOOL("AFF_68040", e->AttnFlags & AFF_68040);
    P_BOOL("AFF_68881", e->AttnFlags & AFF_68881);
    P_BOOL("AFF_68882", e->AttnFlags & AFF_68882);
    P_BOOL("AFF_FPU40", e->AttnFlags & AFF_FPU40);
    P_LONG("VBlankFrequency", e->VBlankFrequency);
    P_LONG("PowerSupplyFrequency", e->PowerSupplyFrequency);
    P_LONG("ex_EClockFrequency", e->ex_EClockFrequency);
    P_NULL("KickMemPtr", e->KickMemPtr);
    P_NULL("KickTagPtr", e->KickTagPtr);
    P_NULL("KickCheckSum", e->KickCheckSum);
    P_UHEX("TaskSigAlloc", e->TaskSigAlloc, 8);
    P_UHEX("TaskTrapAlloc", e->TaskTrapAlloc, 4);
}

static void gfx_fields(void)
{
    struct GfxBase *g = GfxBase;
    struct Node *n;
    LONG cnt;

    P_SECTION("GfxBase");
    P_LONG("lib_Version", g->LibNode.lib_Version);
    P_LONG("lib_Revision", g->LibNode.lib_Revision);
    P_UHEX("ChipRevBits0", g->ChipRevBits0, 2);
    P_UHEX("SetChipRev(0)", SetChipRev(0), 8);
    P_UHEX("DisplayFlags", g->DisplayFlags, 4);
    P_LONG("VBlank", g->VBlank);
    P_UHEX("system_bplcon0", (UWORD)g->system_bplcon0, 4);
    P_UHEX("Flags", g->Flags, 4);
    P_LONG("MaxDisplayRow", g->MaxDisplayRow);
    P_LONG("MaxDisplayColumn", g->MaxDisplayColumn);
    P_LONG("NormalDisplayRows", g->NormalDisplayRows);
    P_LONG("NormalDisplayColumns", g->NormalDisplayColumns);
    P_LONG("NormalDPMX", g->NormalDPMX);
    P_LONG("NormalDPMY", g->NormalDPMY);
    P_LONG("MicrosPerLine", g->MicrosPerLine);
    P_LONG("MinDisplayColumn", g->MinDisplayColumn);
    P_UHEX("MemType", g->MemType, 2);
    P_UHEX("monitor_id", g->monitor_id, 4);
    P_LONG("current_tot_rows", g->current_tot_rows);
    P_LONG("current_tot_cclks", g->current_tot_cclks);
    P_LONG("control_delta_pal", g->control_delta_pal);
    P_LONG("control_delta_ntsc", g->control_delta_ntsc);
    P_MSPC("current_monitor", g->current_monitor);
    P_MSPC("default_monitor", g->default_monitor);
    P_MSPC("natural_monitor", g->natural_monitor);
    cnt = 0;
    for (n = g->MonitorList.lh_Head; n->ln_Succ; n = n->ln_Succ) {
        probe_s("MonitorList[");
        probe_dec(cnt++);
        probe_s("] = ");
        probe_s(n->ln_Name ? n->ln_Name : "(null)");
        probe_ch('\n');
    }
    P_LONG("TopLine", g->TopLine);
    P_LONG("SpriteWidth", g->SpriteWidth);
    P_UHEX("SpriteFMode", g->SpriteFMode, 4);
    P_LONG("DefaultSpriteWidth", g->DefaultSpriteWidth);
    P_UHEX("WantChips", g->WantChips, 2);
    P_UHEX("BoardMemType", g->BoardMemType, 2);
    P_UHEX("Bugs", g->Bugs, 2);
    P_UHEX("ColorMask", g->ColorMask, 8);
    P_UHEX("MonitorFlags", g->MonitorFlags, 4);
    P_LONG("ScanDoubledSprites", g->ScanDoubledSprites);
    P_UHEX("BP3Bits", g->BP3Bits, 2);
    P_LONG("MonitorVBlank.Start", g->MonitorVBlank.asi_Start);
    P_LONG("MonitorVBlank.Stop", g->MonitorVBlank.asi_Stop);
    P_UHEX("ExtSprites", g->ExtSprites, 2);
    P_UHEX("GfxFlags", g->GfxFlags, 4);
    P_STR("DefaultFont", g->DefaultFont ? g->DefaultFont->tf_Message.mn_Node.ln_Name : NULL);
    P_LONG("DefaultFont YSize", g->DefaultFont ? g->DefaultFont->tf_YSize : -1);
}

static void monitor_info(const char *label, ULONG id)
{
    struct MonitorInfo mi;
    struct NameInfo ni;
    ULONG got;

    probe_s("-- monitor of ");
    probe_s(label);
    probe_s(" ");
    probe_hex(id, 8);
    probe_ch('\n');
    if (GetDisplayInfoData(NULL, (UBYTE *)&ni, sizeof(ni), DTAG_NAME, id) >= sizeof(struct QueryHeader))
        P_STR("Name", (char *)ni.Name);
    else
        P_STR("Name", NULL);
    got = GetDisplayInfoData(NULL, (UBYTE *)&mi, sizeof(mi), DTAG_MNTR, id);
    P_BOOL("DTAG_MNTR", got >= sizeof(struct QueryHeader));
    if (got < sizeof(struct QueryHeader))
        return;
    P_MSPC("Mspc", mi.Mspc);
    P_POINT("ViewPosition", &mi.ViewPosition);
    P_POINT("ViewResolution", &mi.ViewResolution);
    P_RECT("ViewPositionRange", &mi.ViewPositionRange);
    P_LONG("TotalRows", mi.TotalRows);
    P_LONG("TotalColorClocks", mi.TotalColorClocks);
    P_LONG("MinRow", mi.MinRow);
    P_LONG("Compatibility", mi.Compatibility);
    P_POINT("MouseTicks", &mi.MouseTicks);
    P_POINT("DefaultViewPosition", &mi.DefaultViewPosition);
    P_UHEX("PreferredModeID", mi.PreferredModeID, 8);
}

static void wb_screen(void)
{
    struct Screen *s = LockPubScreen(NULL);
    struct ViewPort *vp;
    struct ColorMap *cm;
    ULONG rgb[3 * 4];
    LONG i;

    P_SECTION("Workbench screen");
    if (!s) {
        P_STR("LockPubScreen", NULL);
        return;
    }
    vp = &s->ViewPort;
    P_UHEX("ModeID", GetVPModeID(vp), 8);
    P_LONG("Width", s->Width);
    P_LONG("Height", s->Height);
    P_LONG("Depth", s->BitMap.Depth);
    P_LONG("BitMap.BytesPerRow", s->BitMap.BytesPerRow);
    P_UHEX("BitMap.Flags", s->BitMap.Flags, 2);
    P_UHEX("BMA_FLAGS", GetBitMapAttr(s->RastPort.BitMap, BMA_FLAGS), 8);
    P_UHEX("vp Modes", vp->Modes, 4);
    cm = vp->ColorMap;
    P_NULL("ColorMap", cm);
    if (cm) {
        P_LONG("cm Type", cm->Type);
        P_LONG("cm Count", cm->Count);
        P_UHEX("cm Flags", cm->Flags, 2);
        P_LONG("cm SpriteResolution", cm->SpriteResolution);
        P_UHEX("cm AuxFlags", cm->AuxFlags, 2);
        P_LONG("cm SpriteBase_Even", cm->SpriteBase_Even);
        P_LONG("cm SpriteBase_Odd", cm->SpriteBase_Odd);
        P_UHEX("cm Bp_0_base", cm->Bp_0_base, 4);
        P_UHEX("cm Bp_1_base", cm->Bp_1_base, 4);
        GetRGB32(cm, 0, 4, rgb);
        for (i = 0; i < 4; i++) {
            probe_s("GetRGB32 ");
            probe_dec(i);
            probe_s(" = ");
            probe_hex(rgb[i * 3], 8);
            probe_ch(' ');
            probe_hex(rgb[i * 3 + 1], 8);
            probe_ch(' ');
            probe_hex(rgb[i * 3 + 2], 8);
            probe_ch('\n');
        }
        P_UHEX("GetRGB4 1", GetRGB4(cm, 1), 4);
    }
    P_NULL("ActiView", GfxBase->ActiView);
    if (GfxBase->ActiView) {
        P_UHEX("ActiView Modes", GfxBase->ActiView->Modes, 4);
        P_LONG("ActiView DxOffset", GfxBase->ActiView->DxOffset);
        P_LONG("ActiView DyOffset", GfxBase->ActiView->DyOffset);
    }
    UnlockPubScreen(NULL, s);
}

int main(void)
{
    struct Library *l;

    exec_fields();
    cpu_fields();
    gfx_fields();
    monitor_info("default", LORES_KEY);
    monitor_info("PAL hires", PAL_MONITOR_ID | HIRES_KEY);
    monitor_info("NTSC hires", NTSC_MONITOR_ID | HIRES_KEY);
    wb_screen();

    P_SECTION("libraries");
    P_LONG("intuition lib_Version", IntuitionBase->LibNode.lib_Version);
    l = OpenLibrary((STRPTR)"68040.library", 0);
    P_NULL("OpenLibrary(68040.library)", l);
    if (l) {
        P_LONG("68040.library lib_Version", l->lib_Version);
        P_LONG("68040.library lib_Revision", l->lib_Revision);
        P_LONG("68040.library lib_NegSize", l->lib_NegSize);
        P_STR("68040.library lib_IdString", (char *)l->lib_IdString);
        CloseLibrary(l);
    }
    P_NULL("FindName(LibList, 68040.library)", FindName(&SysBase->LibList, (STRPTR)"68040.library"));
    return 0;
}
