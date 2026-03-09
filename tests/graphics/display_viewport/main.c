/*
 * Test: graphics/display_viewport
 * Tests Display/ViewPort APIs used by Phase 78-C.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/view.h>
#include <graphics/copper.h>
#include <graphics/displayinfo.h>
#include <graphics/videocontrol.h>
#include <graphics/modeid.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static void fill_bytes(APTR ptr, UBYTE value, ULONG size)
{
    UBYTE *p = (UBYTE *)ptr;
    ULONG i;

    for (i = 0; i < size; i++)
        p[i] = value;
}

static int strings_equal(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }

    return (*a == '\0' && *b == '\0');
}

static int string_contains(const char *haystack, const char *needle)
{
    const char *start;

    if (!*needle)
        return 1;

    for (start = haystack; *start; start++)
    {
        const char *h = start;
        const char *n = needle;

        while (*h && *n && *h == *n)
        {
            h++;
            n++;
        }

        if (*n == '\0')
            return 1;
    }

    return 0;
}

int main(void)
{
    struct View view;
    struct View *old_view;
    struct ViewPort vp;
    struct RasInfo ras_info;
    struct BitMap *bm = NULL;
    struct ColorMap *cm = NULL;
    struct TagItem batch_items[] = {
        { TAG_DONE, 0 }
    };
    struct TagItem set_tags[22];
    struct TagItem get_tags[18];
    struct TagItem invalid_tags[] = {
        { 0xDEADBEEF, 0 },
        { TAG_DONE, 0 }
    };
    struct TagItem intermediate_query_tags[] = {
        { VC_IntermediateCLUpdate_Query, 0 },
        { TAG_DONE, 0 }
    };
    struct TagItem no_color_query_tags[] = {
        { VC_NoColorPaletteLoad_Query, 0 },
        { TAG_DONE, 0 }
    };
    struct TagItem dualpf_query_tags[] = {
        { VC_DUALPF_Disable_Query, 0 },
        { TAG_DONE, 0 }
    };
    struct DisplayInfo disp;
    struct DimensionInfo dims;
    struct MonitorInfo mon;
    struct NameInfo name;
    ULONG result;
    ULONG ids[8];
    ULONG display_id;
    ULONG query_value;
    LONG immediate = -1;
    APTR normal_info = (APTR)0x00123456;
    APTR coerce_info = (APTR)0x00654321;
    int errors = 0;
    int i;

    print("Testing Display/ViewPort APIs...\n");

    fill_bytes(&view, 0xA5, sizeof(view));
    InitView(&view);
    if (view.ViewPort != NULL || view.LOFCprList != NULL || view.SHFCprList != NULL ||
        view.DxOffset != 0 || view.DyOffset != 0 || view.Modes != 0)
    {
        print("FAIL: InitView() did not reset fields\n");
        errors++;
    }
    else
    {
        print("OK: InitView() reset public fields\n");
    }

    fill_bytes(&vp, 0x5A, sizeof(vp));
    InitVPort(&vp);
    if (vp.Next != NULL || vp.ColorMap != NULL || vp.DspIns != NULL || vp.SprIns != NULL ||
        vp.ClrIns != NULL || vp.UCopIns != NULL || vp.DWidth != 0 || vp.DHeight != 0 ||
        vp.DxOffset != 0 || vp.DyOffset != 0 || vp.Modes != 0 ||
        vp.SpritePriorities != 0x24 || vp.ExtendedModes != 0 || vp.RasInfo != NULL)
    {
        print("FAIL: InitVPort() did not reset fields\n");
        errors++;
    }
    else
    {
        print("OK: InitVPort() reset public fields\n");
    }

    bm = AllocBitMap(320, 256, 2, BMF_CLEAR, NULL);
    if (!bm)
    {
        print("FAIL: AllocBitMap() for display test returned NULL\n");
        return 20;
    }

    cm = GetColorMap(4);
    if (!cm)
    {
        print("FAIL: GetColorMap() returned NULL\n");
        FreeBitMap(bm);
        return 20;
    }

    ras_info.Next = NULL;
    ras_info.BitMap = bm;
    ras_info.RxOffset = 0;
    ras_info.RyOffset = 0;

    view.ViewPort = &vp;
    view.Modes = LACE;
    vp.RasInfo = &ras_info;
    vp.ColorMap = cm;
    vp.DWidth = 320;
    vp.DHeight = 256;
    vp.Modes = HIRES | LACE;

    i = 0;
    set_tags[i].ti_Tag = VTAG_ATTACH_CM_SET; set_tags[i++].ti_Data = (ULONG)&vp;
    set_tags[i].ti_Tag = VTAG_NORMAL_DISP_SET; set_tags[i++].ti_Data = (ULONG)normal_info;
    set_tags[i].ti_Tag = VTAG_COERCE_DISP_SET; set_tags[i++].ti_Data = (ULONG)coerce_info;
    set_tags[i].ti_Tag = VTAG_PF1_BASE_SET; set_tags[i++].ti_Data = 0x1111;
    set_tags[i].ti_Tag = VTAG_PF2_BASE_SET; set_tags[i++].ti_Data = 0x2222;
    set_tags[i].ti_Tag = VTAG_SPEVEN_BASE_SET; set_tags[i++].ti_Data = 0x3333;
    set_tags[i].ti_Tag = VTAG_SPODD_BASE_SET; set_tags[i++].ti_Data = 0x4444;
    set_tags[i].ti_Tag = VTAG_BORDERSPRITE_SET; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_BORDERBLANK_SET; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_BORDERNOTRANS_SET; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_SPRITERESN_SET; set_tags[i++].ti_Data = SPRITERESN_70NS;
    set_tags[i].ti_Tag = VTAG_DEFSPRITERESN_SET; set_tags[i++].ti_Data = SPRITERESN_140NS;
    set_tags[i].ti_Tag = VTAG_FULLPALETTE_SET; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_USERCLIP_SET; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_BATCH_CM_SET; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_BATCH_ITEMS_SET; set_tags[i++].ti_Data = (ULONG)batch_items;
    set_tags[i].ti_Tag = VTAG_VPMODEID_SET; set_tags[i++].ti_Data = PAL_MONITOR_ID | HIRES_KEY;
    set_tags[i].ti_Tag = VC_IntermediateCLUpdate; set_tags[i++].ti_Data = FALSE;
    set_tags[i].ti_Tag = VC_NoColorPaletteLoad; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VC_DUALPF_Disable; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_IMMEDIATE; set_tags[i++].ti_Data = (ULONG)&immediate;
    set_tags[i].ti_Tag = TAG_DONE; set_tags[i].ti_Data = 0;

    result = VideoControl(cm, set_tags);
    if (result != 0)
    {
        print("FAIL: VideoControl() set tags returned error\n");
        errors++;
    }
    else if (immediate != 0)
    {
        print("FAIL: VideoControl() did not clear immediate result\n");
        errors++;
    }
    else
    {
        print("OK: VideoControl() handled set tags\n");
    }

    i = 0;
    get_tags[i].ti_Tag = VTAG_ATTACH_CM_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_NORMAL_DISP_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_COERCE_DISP_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_PF1_BASE_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_PF2_BASE_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_SPEVEN_BASE_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_SPODD_BASE_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_BORDERSPRITE_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_BORDERBLANK_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_BORDERNOTRANS_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_SPRITERESN_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_DEFSPRITERESN_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_FULLPALETTE_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_USERCLIP_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_BATCH_CM_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_BATCH_ITEMS_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_VPMODEID_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = TAG_DONE; get_tags[i].ti_Data = 0;

    result = VideoControl(cm, get_tags);
    if (result != 0 ||
        get_tags[0].ti_Tag != VTAG_ATTACH_CM_SET || get_tags[0].ti_Data != (ULONG)&vp ||
        get_tags[1].ti_Tag != VTAG_NORMAL_DISP_SET || get_tags[1].ti_Data != (ULONG)normal_info ||
        get_tags[2].ti_Tag != VTAG_COERCE_DISP_SET || get_tags[2].ti_Data != (ULONG)coerce_info ||
        get_tags[3].ti_Tag != VTAG_PF1_BASE_SET || get_tags[3].ti_Data != 0x1111 ||
        get_tags[4].ti_Tag != VTAG_PF2_BASE_SET || get_tags[4].ti_Data != 0x2222 ||
        get_tags[5].ti_Tag != VTAG_SPEVEN_BASE_SET || get_tags[5].ti_Data != 0x3333 ||
        get_tags[6].ti_Tag != VTAG_SPODD_BASE_SET || get_tags[6].ti_Data != 0x4444 ||
        get_tags[7].ti_Tag != VTAG_BORDERSPRITE_SET || get_tags[7].ti_Data != TRUE ||
        get_tags[8].ti_Tag != VTAG_BORDERBLANK_SET || get_tags[8].ti_Data != TRUE ||
        get_tags[9].ti_Tag != VTAG_BORDERNOTRANS_SET || get_tags[9].ti_Data != TRUE ||
        get_tags[10].ti_Tag != VTAG_SPRITERESN_SET || get_tags[10].ti_Data != SPRITERESN_70NS ||
        get_tags[11].ti_Tag != VTAG_DEFSPRITERESN_SET || get_tags[11].ti_Data != SPRITERESN_140NS ||
        get_tags[12].ti_Tag != VTAG_FULLPALETTE_SET || get_tags[12].ti_Data != TRUE ||
        get_tags[13].ti_Tag != VTAG_USERCLIP_SET || get_tags[13].ti_Data != TRUE ||
        get_tags[14].ti_Tag != VTAG_BATCH_CM_SET || get_tags[14].ti_Data != TRUE ||
        get_tags[15].ti_Tag != VTAG_BATCH_ITEMS_SET || get_tags[15].ti_Data != (ULONG)batch_items ||
        get_tags[16].ti_Tag != VTAG_VPMODEID_SET || get_tags[16].ti_Data != (PAL_MONITOR_ID | HIRES_KEY))
    {
        print("FAIL: VideoControl() get tags returned wrong values\n");
        errors++;
    }
    else
    {
        print("OK: VideoControl() returned stored values\n");
    }

    query_value = 0xFFFFFFFF;
    intermediate_query_tags[0].ti_Data = (ULONG)&query_value;
    result = VideoControl(cm, intermediate_query_tags);
    if (result != 0 || query_value != FALSE)
    {
        print("FAIL: VC_IntermediateCLUpdate_Query wrong value\n");
        errors++;
    }
    else
    {
        print("OK: VC_IntermediateCLUpdate_Query returned FALSE\n");
    }

    query_value = 0;
    no_color_query_tags[0].ti_Data = (ULONG)&query_value;
    result = VideoControl(cm, no_color_query_tags);
    if (result != 0 || query_value != TRUE)
    {
        print("FAIL: VC_NoColorPaletteLoad_Query wrong value\n");
        errors++;
    }
    else
    {
        print("OK: VC_NoColorPaletteLoad_Query returned TRUE\n");
    }

    query_value = 0;
    dualpf_query_tags[0].ti_Data = (ULONG)&query_value;
    result = VideoControl(cm, dualpf_query_tags);
    if (result != 0 || query_value != TRUE)
    {
        print("FAIL: VC_DUALPF_Disable_Query wrong value\n");
        errors++;
    }
    else
    {
        print("OK: VC_DUALPF_Disable_Query returned TRUE\n");
    }

    result = VideoControl(cm, invalid_tags);
    if (result == 0)
    {
        print("FAIL: VideoControl() accepted invalid tag\n");
        errors++;
    }
    else
    {
        print("OK: VideoControl() rejects invalid tags\n");
    }

    result = MakeVPort(&view, &vp);
    if (result != MVP_OK || vp.DspIns == NULL)
    {
        print("FAIL: MakeVPort() did not build placeholder copper list\n");
        errors++;
    }
    else
    {
        print("OK: MakeVPort() built placeholder copper list\n");
    }

    result = MrgCop(&view);
    if (result != MVP_OK || view.LOFCprList == NULL || view.SHFCprList == NULL)
    {
        print("FAIL: MrgCop() did not build compiled copper lists\n");
        errors++;
    }
    else
    {
        print("OK: MrgCop() built compiled copper lists\n");
    }

    FreeVPortCopLists(&vp);
    if (vp.DspIns != NULL || vp.SprIns != NULL || vp.ClrIns != NULL)
    {
        print("FAIL: FreeVPortCopLists() did not clear pointers\n");
        errors++;
    }
    else
    {
        print("OK: FreeVPortCopLists() released placeholder copper lists\n");
    }

    FreeCprList(view.LOFCprList);
    FreeCprList(view.SHFCprList);
    view.LOFCprList = NULL;
    view.SHFCprList = NULL;
    FreeCopList(NULL);
    print("OK: FreeCopList()/FreeCprList() are safe\n");

    old_view = GfxBase->ActiView;
    LoadView(&view);
    if (GfxBase->ActiView != &view)
    {
        print("FAIL: LoadView() did not update ActiView\n");
        errors++;
    }
    else
    {
        print("OK: LoadView() updated ActiView\n");
    }
    LoadView(old_view);

    if (FindDisplayInfo(INVALID_ID) != NULL || FindDisplayInfo(0x00F00000) != NULL)
    {
        print("FAIL: FindDisplayInfo() accepted invalid display ID\n");
        errors++;
    }
    else if (FindDisplayInfo(LORES_KEY) == NULL ||
             FindDisplayInfo(HIRES_KEY) == NULL ||
             FindDisplayInfo(PAL_MONITOR_ID | HIRES_KEY) == NULL)
    {
        print("FAIL: FindDisplayInfo() rejected known display ID\n");
        errors++;
    }
    else
    {
        print("OK: FindDisplayInfo() handles known IDs\n");
    }

    display_id = INVALID_ID;
    for (i = 0; i < 8; i++)
    {
        display_id = NextDisplayInfo(display_id);
        ids[i] = display_id;
        if (display_id == INVALID_ID)
            break;
    }

    if (ids[0] != LORES_KEY || ids[1] != HIRES_KEY || ids[2] != (NTSC_MONITOR_ID | LORES_KEY) ||
        ids[3] != (NTSC_MONITOR_ID | HIRES_KEY) || ids[4] != (PAL_MONITOR_ID | LORES_KEY) ||
        ids[5] != (PAL_MONITOR_ID | HIRES_KEY) || ids[6] != INVALID_ID)
    {
        print("FAIL: NextDisplayInfo() iteration order unexpected\n");
        errors++;
    }
    else
    {
        print("OK: NextDisplayInfo() iterates known modes\n");
    }

    result = GetDisplayInfoData(FindDisplayInfo(PAL_MONITOR_ID | HIRES_KEY), &disp,
                                sizeof(disp), DTAG_DISP, INVALID_ID);
    if (result != sizeof(disp) || disp.Header.StructID != DTAG_DISP ||
        disp.Header.DisplayID != (PAL_MONITOR_ID | HIRES_KEY) ||
        disp.Header.SkipID != TAG_SKIP || disp.NotAvailable != FALSE ||
        (disp.PropertyFlags & DIPF_IS_SPRITES) == 0 ||
        (disp.PropertyFlags & DIPF_IS_WB) == 0 ||
        (disp.PropertyFlags & DIPF_IS_DRAGGABLE) == 0 ||
        (disp.PropertyFlags & DIPF_IS_PAL) == 0)
    {
        print("FAIL: GetDisplayInfoData(DTAG_DISP) returned wrong data\n");
        errors++;
    }
    else
    {
        print("OK: GetDisplayInfoData(DTAG_DISP) returned expected header/flags\n");
    }

    result = GetDisplayInfoData(NULL, &dims, sizeof(dims), DTAG_DIMS, PAL_MONITOR_ID | HIRES_KEY);
    if (result != sizeof(dims) || dims.Header.DisplayID != (PAL_MONITOR_ID | HIRES_KEY) ||
        dims.MaxDepth != 8 || dims.MinRasterWidth != 640 || dims.MaxRasterWidth < 640 ||
        dims.Nominal.MaxX != 639 || dims.Nominal.MaxY != 255)
    {
        print("FAIL: GetDisplayInfoData(DTAG_DIMS) returned wrong geometry\n");
        errors++;
    }
    else
    {
        print("OK: GetDisplayInfoData(DTAG_DIMS) returned expected geometry\n");
    }

    result = GetDisplayInfoData(NULL, &mon, sizeof(mon), DTAG_MNTR, PAL_MONITOR_ID | LORES_KEY);
    if (result != sizeof(mon) || mon.Header.DisplayID != (PAL_MONITOR_ID | LORES_KEY) ||
        mon.TotalRows != 312 || mon.Compatibility != MCOMPAT_MIXED ||
        mon.PreferredModeID != (PAL_MONITOR_ID | LORES_KEY))
    {
        print("FAIL: GetDisplayInfoData(DTAG_MNTR) returned wrong monitor data\n");
        errors++;
    }
    else
    {
        print("OK: GetDisplayInfoData(DTAG_MNTR) returned expected monitor data\n");
    }

    result = GetDisplayInfoData(NULL, &name, sizeof(name), DTAG_NAME, HIRES_KEY);
    if (result != sizeof(name) || name.Header.DisplayID != HIRES_KEY ||
        !string_contains((const char *)name.Name, "HIRES"))
    {
        print("FAIL: GetDisplayInfoData(DTAG_NAME) returned wrong name\n");
        errors++;
    }
    else
    {
        print("OK: GetDisplayInfoData(DTAG_NAME) returned expected name\n");
    }

    fill_bytes(&name, 0xCC, sizeof(name));
    result = GetDisplayInfoData(NULL, &name, 12, DTAG_NAME, LORES_KEY);
    if (result != 12)
    {
        print("FAIL: GetDisplayInfoData() did not honor truncated size\n");
        errors++;
    }
    else
    {
        print("OK: GetDisplayInfoData() honors truncated size\n");
    }

    result = GetDisplayInfoData(NULL, &name, sizeof(name), 0x81234567, LORES_KEY);
    if (result != 0)
    {
        print("FAIL: GetDisplayInfoData() accepted unknown tag\n");
        errors++;
    }
    else
    {
        print("OK: GetDisplayInfoData() rejects unknown tag\n");
    }

    result = GetDisplayInfoData(NULL, &name, sizeof(name), DTAG_NAME, INVALID_ID);
    if (result != 0)
    {
        print("FAIL: GetDisplayInfoData() accepted INVALID_ID without handle\n");
        errors++;
    }
    else
    {
        print("OK: GetDisplayInfoData() rejects INVALID_ID without handle\n");
    }

    result = GetDisplayInfoData(FindDisplayInfo(LORES_KEY), &name, sizeof(name), DTAG_NAME, INVALID_ID);
    if (result != sizeof(name) || !strings_equal((const char *)name.Name, "LORES"))
    {
        print("FAIL: GetDisplayInfoData() did not resolve handle-based lookup\n");
        errors++;
    }
    else
    {
        print("OK: GetDisplayInfoData() resolves handle-based lookup\n");
    }

    FreeColorMap(cm);
    FreeBitMap(bm);

    if (errors == 0)
    {
        print("PASS: Display/ViewPort tests passed\n");
        return 0;
    }

    print("FAIL: Display/ViewPort tests had errors\n");
    return 20;
}
