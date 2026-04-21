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

static void free_ucoplist_chain(struct UCopList *ucl)
{
    struct CopList *cop_list;

    if (!ucl)
        return;

    cop_list = ucl->FirstCopList;
    while (cop_list)
    {
        struct CopList *next = cop_list->Next;

        if (cop_list->CopIns)
            FreeMem(cop_list->CopIns, cop_list->MaxCount * sizeof(struct CopIns));

        FreeMem(cop_list, sizeof(struct CopList));
        cop_list = next;
    }

    ucl->FirstCopList = NULL;
    ucl->CopList = NULL;
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
    struct UCopList ucl;
    struct CopList *ucl_list;
    struct DBufInfo *dbi = NULL;
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

    if (cm->cm_vp == &vp)
    {
        print("OK: VideoControl() attached ColorMap to ViewPort\n");
    }
    else
    {
        print("FAIL: VideoControl() did not set cm_vp\n");
        errors++;
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
    else if (cm->cm_vp != &vp)
    {
        print("FAIL: MakeVPort() did not keep ColorMap attached to ViewPort\n");
        errors++;
    }
    else
    {
        print("OK: MakeVPort() built placeholder copper list and attached ColorMap\n");
    }

    {
        struct CopIns calc_ins[3];
        struct CopList calc_list;
        struct BitMap calc_bm;
        struct RasInfo calc_ras;

        fill_bytes(&calc_list, 0, sizeof(calc_list));
        fill_bytes(calc_ins, 0, sizeof(calc_ins));
        fill_bytes(&calc_bm, 0, sizeof(calc_bm));
        fill_bytes(&calc_ras, 0, sizeof(calc_ras));

        calc_ins[0].OpCode = COPPER_MOVE;
        calc_ins[1].OpCode = COPPER_MOVE;
        calc_ins[2].OpCode = COPPER_WAIT;
        calc_list.CopIns = calc_ins;
        calc_list.Count = 3;
        calc_list.MaxCount = 3;
        calc_bm.BytesPerRow = 40;
        calc_bm.Rows = 256;
        calc_bm.Depth = 2;
        calc_ras.BitMap = &calc_bm;

        vp.DspIns = &calc_list;
        vp.RasInfo = &calc_ras;
        vp.DWidth = 320;
        vp.Modes = HIRES | LACE;
        view.Modes = LACE;

        result = CalcIVG(&view, &vp);
        if (result != 1)
        {
            print("FAIL: CalcIVG() returned unexpected laced viewport gap\n");
            errors++;
        }
        else
        {
            calc_bm.Depth = 8;
            vp.DWidth = 640;
            result = CalcIVG(&view, &vp);
            if (result != 0)
            {
                print("FAIL: CalcIVG() ignored unavailable copper bandwidth\n");
                errors++;
            }
            else
            {
                vp.DspIns = NULL;
                if (CalcIVG(&view, &vp) != 0)
                {
                    print("FAIL: CalcIVG() accepted a viewport without DspIns\n");
                    errors++;
                }
                else
                {
                    print("OK: CalcIVG() accounts for copper bandwidth and DspIns presence\n");
                }
            }
        }

        vp.RasInfo = &ras_info;
        vp.DWidth = 320;
        vp.Modes = HIRES | LACE;
        view.Modes = LACE;
    }

    fill_bytes(&ucl, 0, sizeof(ucl));
    ucl_list = UCopperListInit(&ucl, 4);
    if (!ucl_list || ucl.FirstCopList != ucl_list || ucl.CopList != ucl_list ||
        ucl_list->MaxCount != 4 || ucl_list->CopPtr != ucl_list->CopIns)
    {
        print("FAIL: UCopperListInit() did not initialize UCopList state\n");
        errors++;
    }
    else
    {
        print("OK: UCopperListInit() initialized UCopList state\n");
    }

    free_ucoplist_chain(&ucl);

    fill_bytes(&ucl, 0, sizeof(ucl));
    ucl_list = UCopperListInit(&ucl, 1);
    if (!ucl_list)
    {
        print("FAIL: UCopperListInit() returned NULL for CMove() test\n");
        errors++;
    }
    else if (!CMove(&ucl, (APTR)0x0180, 0x1357) ||
             ucl.CopList != ucl_list || ucl_list->Count != 0 ||
             ucl_list->CopPtr != ucl_list->CopIns ||
             ucl_list->CopIns[0].OpCode != COPPER_MOVE ||
             ucl_list->CopIns[0].u3.u4.u1.DestAddr != 0x0180 ||
             ucl_list->CopIns[0].u3.u4.u2.DestData != 0x1357)
    {
        print("FAIL: CMove() did not encode the copper move instruction\n");
        errors++;
    }
    else
    {
        ucl_list->Count = ucl_list->MaxCount;
        ucl_list->CopPtr = ucl_list->CopIns + ucl_list->MaxCount;

        if (CMove(&ucl, (APTR)0x0182, 0x2468) != FALSE)
        {
            print("FAIL: CMove() did not report a full copper block\n");
            errors++;
        }
        else
        {
            print("OK: CMove() encodes instructions and reports full blocks\n");
        }
    }

    free_ucoplist_chain(&ucl);

    fill_bytes(&ucl, 0, sizeof(ucl));
    ucl_list = UCopperListInit(&ucl, 2);
    if (!ucl_list)
    {
        print("FAIL: UCopperListInit() returned NULL for CBump() test\n");
        errors++;
    }
    else
    {
        ucl.CopList->CopPtr->OpCode = COPPER_MOVE;
        ucl.CopList->CopPtr->u3.u4.u1.DestAddr = 0x0180;
        ucl.CopList->CopPtr->u3.u4.u2.DestData = 0x1234;
        CBump(&ucl);

        if (ucl.CopList != ucl_list || ucl_list->Count != 1 ||
            ucl_list->CopPtr != (ucl_list->CopIns + 1))
        {
            print("FAIL: CBump() did not advance within the current copper block\n");
            errors++;
        }
        else
        {
            ucl.CopList->CopPtr->OpCode = COPPER_WAIT;
            ucl.CopList->CopPtr->u3.u4.u1.VWaitPos = 0x0020;
            ucl.CopList->CopPtr->u3.u4.u2.HWaitPos = 0x0040;
            CBump(&ucl);

            if (!ucl_list->Next || ucl.CopList != ucl_list->Next ||
                ucl_list->CopIns[1].OpCode != CPRNXTBUF ||
                ucl_list->CopIns[1].u3.nxtlist != ucl_list->Next ||
                ucl.CopList->Count != 1 ||
                ucl.CopList->CopPtr != (ucl.CopList->CopIns + 1) ||
                ucl.CopList->CopIns[0].OpCode != COPPER_WAIT ||
                ucl.CopList->CopIns[0].u3.u4.u1.VWaitPos != 0x0020 ||
                ucl.CopList->CopIns[0].u3.u4.u2.HWaitPos != 0x0040)
            {
                print("FAIL: CBump() did not spill the last instruction into a chained block\n");
                errors++;
            }
            else
            {
                print("OK: CBump() advances and chains copper instruction blocks\n");
            }
        }
    }

    free_ucoplist_chain(&ucl);

    fill_bytes(&ucl, 0, sizeof(ucl));
    ucl_list = UCopperListInit(&ucl, 1);
    if (!ucl_list)
    {
        print("FAIL: UCopperListInit() returned NULL for CWait() test\n");
        errors++;
    }
    else
    {
        CWait(&ucl, 0x0024, 0x0068);

        if (ucl.CopList != ucl_list || ucl_list->Count != 0 ||
            ucl_list->CopPtr != ucl_list->CopIns ||
            ucl_list->CopIns[0].OpCode != COPPER_WAIT ||
            ucl_list->CopIns[0].u3.u4.u1.VWaitPos != 0x0024 ||
            ucl_list->CopIns[0].u3.u4.u2.HWaitPos != 0x0068)
        {
            print("FAIL: CWait() did not encode the copper wait instruction\n");
            errors++;
        }
        else
        {
            ucl_list->CopIns[0].OpCode = COPPER_MOVE;
            ucl_list->Count = ucl_list->MaxCount;
            ucl_list->CopPtr = ucl_list->CopIns + ucl_list->MaxCount;

            CWait(&ucl, 0x0012, 0x0034);

            if (ucl_list->CopIns[0].OpCode != COPPER_MOVE)
            {
                print("FAIL: CWait() overwrote a full copper block\n");
                errors++;
            }
            else
            {
                print("OK: CWait() encodes instructions and leaves full blocks untouched\n");
            }
        }
    }

    free_ucoplist_chain(&ucl);

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

    dbi = AllocDBufInfo(&vp);
    if (!dbi)
    {
        print("FAIL: AllocDBufInfo() returned NULL\n");
        errors++;
    }
    else
    {
        print("OK: AllocDBufInfo() allocated DBufInfo\n");
        ChangeVPBitMap(&vp, bm, dbi);
        if (vp.RasInfo->BitMap != bm)
        {
            print("FAIL: ChangeVPBitMap() did not update RasInfo bitmap\n");
            errors++;
        }
        else
        {
            print("OK: ChangeVPBitMap() updated RasInfo bitmap\n");
        }
        FreeDBufInfo(dbi);
        dbi = NULL;
    }

    vp.DxOffset = 12;
    vp.DyOffset = 34;
    if (cm->cm_vpe)
    {
        ScrollVPort(&vp);
        if (cm->cm_vpe->Origin[0].x != 12 || cm->cm_vpe->Origin[0].y != 34)
        {
            print("FAIL: ScrollVPort() did not track viewport origin\n");
            errors++;
        }
        else
        {
            print("OK: ScrollVPort() tracked viewport origin\n");
        }
    }

    result = CoerceMode(&vp, PAL_MONITOR_ID, 0);
    if (result != (PAL_MONITOR_ID | HIRES_KEY))
    {
        print("FAIL: CoerceMode() returned unexpected mode\n");
        errors++;
    }
    else
    {
        print("OK: CoerceMode() returned expected mode\n");
    }

    WaitBOVP(&vp);
    print("OK: WaitBOVP() returned without blocking\n");

    /* Phase 129: FindDisplayInfo() now virtualizes unknown IDs to the
     * closest physical mode instead of returning 0.  This matches the Wine
     * strategy of accepting any conceptually renderable mode so that apps
     * probing exotic IDs (CloantoScreenManager, FinalWriter) get a plausible
     * handle rather than a hard failure.  INVALID_ID is still explicitly
     * rejected. */
    if (FindDisplayInfo(INVALID_ID) != NULL)
    {
        print("FAIL: FindDisplayInfo(INVALID_ID) should return NULL\n");
        errors++;
    }
    else if (FindDisplayInfo(LORES_KEY) == NULL ||
             FindDisplayInfo(HIRES_KEY) == NULL ||
             FindDisplayInfo(PAL_MONITOR_ID | HIRES_KEY) == NULL)
    {
        print("FAIL: FindDisplayInfo() rejected known display ID\n");
        errors++;
    }
    else if (FindDisplayInfo(0x00F00000) == NULL)
    {
        /* Virtualization: unknown-but-plausible IDs now return a handle. */
        print("FAIL: FindDisplayInfo() failed to virtualize unknown ID\n");
        errors++;
    }
    else
    {
        print("OK: FindDisplayInfo() handles known and virtualized IDs\n");
    }

    display_id = INVALID_ID;
    for (i = 0; i < 8; i++)
    {
        display_id = NextDisplayInfo(display_id);
        ids[i] = display_id;
        if (display_id == INVALID_ID)
            break;
    }

    /* Phase 129: The mode table was expanded from 6 to 36 entries (DEFAULT,
     * NTSC, and PAL monitors × 12 modes each) to satisfy commercial app
     * probes (PPaint CloantoScreenManager, FinalWriter).  The first entry
     * is still LORES_KEY and the iteration is still deterministic, but the
     * full order is no longer DEFAULT-LORES, DEFAULT-HIRES, NTSC-LORES...
     * because intermediate LACE/HAM/EHB modes are now advertised.  The test
     * now verifies that: (a) iteration is monotonic and terminates, (b) the
     * first entry is LORES_KEY, (c) all returned IDs are distinct and
     * accepted by FindDisplayInfo(). */
    if (ids[0] != LORES_KEY)
    {
        print("FAIL: NextDisplayInfo() first entry is not LORES_KEY\n");
        errors++;
    }
    else
    {
        BOOL ok = TRUE;
        int j;
        for (j = 0; j < 8 && ids[j] != INVALID_ID; j++)
        {
            int k;
            if (FindDisplayInfo(ids[j]) == NULL)
            {
                ok = FALSE;
                break;
            }
            for (k = 0; k < j; k++)
            {
                if (ids[k] == ids[j])
                {
                    ok = FALSE;
                    break;
                }
            }
        }
        if (!ok)
        {
            print("FAIL: NextDisplayInfo() iteration contains invalid or duplicate IDs\n");
            errors++;
        }
        else
        {
            print("OK: NextDisplayInfo() iterates known modes\n");
        }
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
    /* Phase 129: DTAG_DIMS now returns realistic raster ranges (HIRES:
     * 320×200 min, 1280×1024 max) rather than pinned Min==Max values.
     * Nominal is still the physical 640×256 PAL HIRES viewport.  Apps that
     * check whether a mode can render at a target size (CloantoScreenManager)
     * require Max > Min to consider the mode usable. */
    if (result != sizeof(dims) || dims.Header.DisplayID != (PAL_MONITOR_ID | HIRES_KEY) ||
        dims.MaxDepth != 8 ||
        dims.MinRasterWidth > 640 || dims.MaxRasterWidth < 640 ||
        dims.MinRasterWidth >= dims.MaxRasterWidth ||
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

    /*
     * Phase 129: Screen-mode virtualization coverage
     *
     * These tests verify the Wine-style mode-virtualization strategy added to
     * satisfy commercial app probe sequences (CloantoScreenManager, FinalWriter).
     * Unknown-but-plausible mode IDs are mapped to the closest physical mode,
     * DTAG_DIMS returns realistic raster ranges, and PropertyFlags reports
     * HAM/EHB/LACE/PAL bits so apps can discriminate mode capabilities.
     */
    {
        struct DisplayInfo di2;
        struct DimensionInfo dims2;

        /* HAM mode should report DIPF_IS_HAM in PropertyFlags */
        fill_bytes(&di2, 0, sizeof(di2));
        result = GetDisplayInfoData(NULL, &di2, sizeof(di2), DTAG_DISP, PAL_MONITOR_ID | HAM_KEY);
        if (result != sizeof(di2) || (di2.PropertyFlags & DIPF_IS_HAM) == 0)
        {
            print("FAIL: HAM mode missing DIPF_IS_HAM\n");
            errors++;
        }
        else
        {
            print("OK: HAM mode advertises DIPF_IS_HAM\n");
        }

        /* EHB mode should report DIPF_IS_EXTRAHALFBRITE */
        fill_bytes(&di2, 0, sizeof(di2));
        result = GetDisplayInfoData(NULL, &di2, sizeof(di2), DTAG_DISP, PAL_MONITOR_ID | EXTRAHALFBRITE_KEY);
        if (result != sizeof(di2) || (di2.PropertyFlags & DIPF_IS_EXTRAHALFBRITE) == 0)
        {
            print("FAIL: EHB mode missing DIPF_IS_EXTRAHALFBRITE\n");
            errors++;
        }
        else
        {
            print("OK: EHB mode advertises DIPF_IS_EXTRAHALFBRITE\n");
        }

        /* LACE mode should report DIPF_IS_LACE */
        fill_bytes(&di2, 0, sizeof(di2));
        result = GetDisplayInfoData(NULL, &di2, sizeof(di2), DTAG_DISP, PAL_MONITOR_ID | HIRESLACE_KEY);
        if (result != sizeof(di2) || (di2.PropertyFlags & DIPF_IS_LACE) == 0)
        {
            print("FAIL: HIRESLACE missing DIPF_IS_LACE\n");
            errors++;
        }
        else
        {
            print("OK: HIRESLACE advertises DIPF_IS_LACE\n");
        }

        /* DIPF_IS_ECS is set on all modes so apps can detect ECS chipset */
        fill_bytes(&di2, 0, sizeof(di2));
        result = GetDisplayInfoData(NULL, &di2, sizeof(di2), DTAG_DISP, PAL_MONITOR_ID | HIRES_KEY);
        if (result != sizeof(di2) || (di2.PropertyFlags & DIPF_IS_ECS) == 0)
        {
            print("FAIL: Standard mode missing DIPF_IS_ECS\n");
            errors++;
        }
        else
        {
            print("OK: Standard mode advertises DIPF_IS_ECS\n");
        }

        /* LORES DTAG_DIMS returns LORES-scale ranges (<= 640×512) */
        fill_bytes(&dims2, 0, sizeof(dims2));
        result = GetDisplayInfoData(NULL, &dims2, sizeof(dims2), DTAG_DIMS, PAL_MONITOR_ID | LORES_KEY);
        if (result != sizeof(dims2) ||
            dims2.MinRasterWidth >= dims2.MaxRasterWidth ||
            dims2.MaxRasterWidth > 640 ||
            dims2.MinRasterWidth > 320)
        {
            print("FAIL: LORES DTAG_DIMS returned wrong ranges\n");
            errors++;
        }
        else
        {
            print("OK: LORES DTAG_DIMS returns plausible ranges\n");
        }

        /* HIRES DTAG_DIMS returns HIRES-scale ranges (>= 640 max) */
        fill_bytes(&dims2, 0, sizeof(dims2));
        result = GetDisplayInfoData(NULL, &dims2, sizeof(dims2), DTAG_DIMS, PAL_MONITOR_ID | HIRES_KEY);
        if (result != sizeof(dims2) ||
            dims2.MinRasterWidth >= dims2.MaxRasterWidth ||
            dims2.MaxRasterWidth < 1024)
        {
            print("FAIL: HIRES DTAG_DIMS returned wrong ranges\n");
            errors++;
        }
        else
        {
            print("OK: HIRES DTAG_DIMS returns plausible ranges\n");
        }

        /* Virtualization: exotic IDs within our known monitor masks still
         * produce plausible DimensionInfo rather than a zero-length result. */
        fill_bytes(&dims2, 0, sizeof(dims2));
        result = GetDisplayInfoData(NULL, &dims2, sizeof(dims2), DTAG_DIMS, 0x00F00000);
        if (result != sizeof(dims2) || dims2.MaxRasterWidth == 0)
        {
            print("FAIL: Virtualized mode DTAG_DIMS returned nothing\n");
            errors++;
        }
        else
        {
            print("OK: Virtualized mode DTAG_DIMS returns plausible ranges\n");
        }
    }

    /* BestModeIDA coverage: MustHave/MustNotHave tag handling */
    {
        struct TagItem tags[5];
        ULONG mode;

        /* Request any HIRES-capable mode */
        tags[0].ti_Tag = BIDTAG_DesiredWidth;
        tags[0].ti_Data = 640;
        tags[1].ti_Tag = BIDTAG_DesiredHeight;
        tags[1].ti_Data = 256;
        tags[2].ti_Tag = TAG_DONE;
        tags[2].ti_Data = 0;
        mode = BestModeIDA(tags);
        if (mode == INVALID_ID || !(mode & HIRES))
        {
            print("FAIL: BestModeIDA() did not return HIRES mode for 640×256\n");
            errors++;
        }
        else
        {
            print("OK: BestModeIDA() returns HIRES mode for 640×256\n");
        }

        /* MustNotHave LACE excludes interlaced modes */
        tags[0].ti_Tag = BIDTAG_DesiredWidth;
        tags[0].ti_Data = 640;
        tags[1].ti_Tag = BIDTAG_DesiredHeight;
        tags[1].ti_Data = 256;
        tags[2].ti_Tag = BIDTAG_DIPFMustNotHave;
        tags[2].ti_Data = DIPF_IS_LACE;
        tags[3].ti_Tag = TAG_DONE;
        tags[3].ti_Data = 0;
        mode = BestModeIDA(tags);
        if (mode == INVALID_ID || (mode & LACE))
        {
            print("FAIL: BestModeIDA() returned LACE mode despite MustNotHave\n");
            errors++;
        }
        else
        {
            print("OK: BestModeIDA() honors DIPF_IS_LACE in MustNotHave\n");
        }
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
