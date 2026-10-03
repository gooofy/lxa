/*
 * Test: graphics/display_viewport
 * Tests the View/ViewPort, copper list and display database APIs against
 * the behaviour of AmigaOS 3.1 (reference machine, PAL).  Only facts that
 * do not depend on the machine configuration are asserted (no chipset
 * specific palette ranges, depths or mode names).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
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

static int errors;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static void check(int ok, const char *okmsg, const char *failmsg)
{
    print(ok ? okmsg : failmsg);
    if (!ok)
        errors++;
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

static ULONG get_flags(ULONG id)
{
    struct DisplayInfo di;

    fill_bytes(&di, 0, sizeof(di));
    if (GetDisplayInfoData(NULL, &di, sizeof(di), DTAG_DISP, id) == 0)
        return 0;
    return di.PropertyFlags;
}

int main(void)
{
    struct View view;
    struct ViewPort vp;
    struct RasInfo ras_info;
    struct BitMap *bm = NULL;
    struct ColorMap *cm = NULL;
    struct TagItem batch_items[] = {
        { TAG_DONE, 0 }
    };
    struct TagItem set_tags[20];
    struct TagItem get_tags[16];
    struct TagItem query_tags[2];
    struct TagItem invalid_tags[] = {
        { 0xDEADBEEF, 0 },
        { TAG_DONE, 0 }
    };
    struct DisplayInfo disp;
    struct DimensionInfo dims;
    struct MonitorInfo mon;
    struct UCopList ucl;
    struct CopList *ucl_list;
    struct DBufInfo *dbi = NULL;
    ULONG result;
    ULONG ids[8];
    ULONG display_id;
    ULONG query_value;
    int i;

    print("Testing Display/ViewPort APIs...\n");

    /* InitView() clears the View and sets the standard display start */
    fill_bytes(&view, 0xA5, sizeof(view));
    InitView(&view);
    check(view.ViewPort == NULL && view.LOFCprList == NULL && view.SHFCprList == NULL &&
          view.DxOffset == 0x81 && view.DyOffset == 0x2C && view.Modes == 0,
          "OK: InitView() reset public fields\n", "FAIL: InitView() did not reset fields\n");

    fill_bytes(&vp, 0x5A, sizeof(vp));
    InitVPort(&vp);
    check(vp.Next == NULL && vp.ColorMap == NULL && vp.DspIns == NULL && vp.SprIns == NULL &&
          vp.ClrIns == NULL && vp.UCopIns == NULL && vp.DWidth == 0 && vp.DHeight == 0 &&
          vp.DxOffset == 0 && vp.DyOffset == 0 && vp.Modes == 0 &&
          vp.SpritePriorities == 0x24 && vp.ExtendedModes == 0 && vp.RasInfo == NULL,
          "OK: InitVPort() reset public fields\n", "FAIL: InitVPort() did not reset fields\n");

    bm = AllocBitMap(320, 256, 2, BMF_CLEAR, NULL);
    cm = GetColorMap(4);
    if (!bm || !cm)
    {
        print("FAIL: AllocBitMap()/GetColorMap() returned NULL\n");
        return 20;
    }

    ras_info.Next = NULL;
    ras_info.BitMap = bm;
    ras_info.RxOffset = 0;
    ras_info.RyOffset = 0;

    view.ViewPort = &vp;
    view.Modes = LACE;
    vp.RasInfo = &ras_info;
    vp.DWidth = 320;
    vp.DHeight = 256;
    vp.Modes = HIRES | LACE;

    /* VideoControl(): VTAG_ATTACH_CM_SET links ColorMap and ViewPort */
    i = 0;
    set_tags[i].ti_Tag = VTAG_ATTACH_CM_SET; set_tags[i++].ti_Data = (ULONG)&vp;
    set_tags[i].ti_Tag = VTAG_PF1_BASE_SET; set_tags[i++].ti_Data = 0x1111;
    set_tags[i].ti_Tag = VTAG_PF2_BASE_SET; set_tags[i++].ti_Data = 0x2222;
    set_tags[i].ti_Tag = VTAG_SPEVEN_BASE_SET; set_tags[i++].ti_Data = 0x3333;
    set_tags[i].ti_Tag = VTAG_SPODD_BASE_SET; set_tags[i++].ti_Data = 0x4444;
    set_tags[i].ti_Tag = VTAG_BORDERSPRITE_SET; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_BORDERBLANK_SET; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_BORDERNOTRANS_SET; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_SPRITERESN_SET; set_tags[i++].ti_Data = SPRITERESN_70NS;
    set_tags[i].ti_Tag = VTAG_DEFSPRITERESN_SET; set_tags[i++].ti_Data = SPRITERESN_140NS;
    set_tags[i].ti_Tag = VTAG_USERCLIP_SET; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_BATCH_CM_SET; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VTAG_BATCH_ITEMS_SET; set_tags[i++].ti_Data = (ULONG)batch_items;
    set_tags[i].ti_Tag = VTAG_VPMODEID_SET; set_tags[i++].ti_Data = PAL_MONITOR_ID | HIRES_KEY;
    set_tags[i].ti_Tag = VC_IntermediateCLUpdate; set_tags[i++].ti_Data = FALSE;
    set_tags[i].ti_Tag = VC_NoColorPaletteLoad; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = VC_DUALPF_Disable; set_tags[i++].ti_Data = TRUE;
    set_tags[i].ti_Tag = TAG_DONE; set_tags[i].ti_Data = 0;

    result = VideoControl(cm, set_tags);
    check(result == 0 && cm->cm_vp == &vp && vp.ColorMap == cm,
          "OK: VideoControl() handled set tags and attached the ColorMap\n",
          "FAIL: VideoControl() set tags failed\n");

    /* GET tags are converted to the SET (or CLR) tag; value tags return
     * the value, boolean tags report the state in the tag only */
    i = 0;
    get_tags[i].ti_Tag = VTAG_ATTACH_CM_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_PF1_BASE_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_PF2_BASE_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_SPEVEN_BASE_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_SPODD_BASE_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_BORDERSPRITE_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_BORDERBLANK_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_BORDERNOTRANS_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_SPRITERESN_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_DEFSPRITERESN_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_USERCLIP_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_BATCH_CM_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = VTAG_BATCH_ITEMS_GET; get_tags[i++].ti_Data = 0;
    get_tags[i].ti_Tag = TAG_DONE; get_tags[i].ti_Data = 0;

    result = VideoControl(cm, get_tags);
    check(result == 0 &&
          get_tags[0].ti_Tag == VTAG_ATTACH_CM_SET && get_tags[0].ti_Data == (ULONG)&vp &&
          get_tags[1].ti_Tag == VTAG_PF1_BASE_SET && get_tags[1].ti_Data == 0x1111 &&
          get_tags[2].ti_Tag == VTAG_PF2_BASE_SET && get_tags[2].ti_Data == 0x2222 &&
          get_tags[3].ti_Tag == VTAG_SPEVEN_BASE_SET && get_tags[3].ti_Data == 0x3333 &&
          get_tags[4].ti_Tag == VTAG_SPODD_BASE_SET && get_tags[4].ti_Data == 0x4444 &&
          get_tags[5].ti_Tag == VTAG_BORDERSPRITE_SET && get_tags[5].ti_Data == 0 &&
          get_tags[6].ti_Tag == VTAG_BORDERBLANK_SET && get_tags[6].ti_Data == 0 &&
          get_tags[7].ti_Tag == VTAG_BORDERNOTRANS_SET && get_tags[7].ti_Data == 0 &&
          get_tags[8].ti_Tag == VTAG_SPRITERESN_SET && get_tags[8].ti_Data == SPRITERESN_70NS &&
          get_tags[9].ti_Tag == VTAG_DEFSPRITERESN_SET && get_tags[9].ti_Data == SPRITERESN_140NS &&
          get_tags[10].ti_Tag == VTAG_USERCLIP_SET && get_tags[10].ti_Data == 0 &&
          get_tags[11].ti_Tag == VTAG_BATCH_CM_SET && get_tags[11].ti_Data == 0 &&
          get_tags[12].ti_Tag == VTAG_BATCH_ITEMS_SET && get_tags[12].ti_Data == (ULONG)batch_items,
          "OK: VideoControl() returned stored values\n",
          "FAIL: VideoControl() get tags returned wrong values\n");

    /* V40 queries report TRUE as -1 */
    query_tags[1].ti_Tag = TAG_DONE;
    query_value = 0x55;
    query_tags[0].ti_Tag = VC_IntermediateCLUpdate_Query;
    query_tags[0].ti_Data = (ULONG)&query_value;
    result = VideoControl(cm, query_tags);
    check(result == 0 && query_value == 0, "OK: VC_IntermediateCLUpdate_Query returned 0\n",
          "FAIL: VC_IntermediateCLUpdate_Query wrong value\n");

    query_value = 0x55;
    query_tags[0].ti_Tag = VC_NoColorPaletteLoad_Query;
    result = VideoControl(cm, query_tags);
    check(result == 0 && query_value == (ULONG)-1, "OK: VC_NoColorPaletteLoad_Query returned -1\n",
          "FAIL: VC_NoColorPaletteLoad_Query wrong value\n");

    query_value = 0x55;
    query_tags[0].ti_Tag = VC_DUALPF_Disable_Query;
    result = VideoControl(cm, query_tags);
    check(result == 0 && query_value == (ULONG)-1, "OK: VC_DUALPF_Disable_Query returned -1\n",
          "FAIL: VC_DUALPF_Disable_Query wrong value\n");

    result = VideoControl(cm, invalid_tags);
    check(result == 0, "OK: VideoControl() ignores unknown tags\n",
          "FAIL: VideoControl() rejected an unknown tag\n");

    result = MakeVPort(&view, &vp);
    check(result == MVP_OK && vp.DspIns != NULL && cm->cm_vp == &vp,
          "OK: MakeVPort() built the copper list and kept the ColorMap attached\n",
          "FAIL: MakeVPort() failed\n");

    /* CalcIVG(): 0 without instructions, else 1 line, 2 when interlaced */
    {
        struct CopIns calc_ins[3];
        struct CopList calc_list;
        struct BitMap calc_bm;
        struct RasInfo calc_ras;
        struct ViewPort calc_vp;
        struct View calc_view;
        UWORD lace_lines, plain_lines, no_lines;

        InitView(&calc_view);
        InitVPort(&calc_vp);
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

        calc_vp.DspIns = &calc_list;
        calc_vp.RasInfo = &calc_ras;
        calc_vp.DWidth = 320;
        calc_vp.Modes = HIRES | LACE;
        calc_view.Modes = LACE;
        lace_lines = CalcIVG(&calc_view, &calc_vp);
        calc_vp.Modes = HIRES;
        calc_view.Modes = 0;
        plain_lines = CalcIVG(&calc_view, &calc_vp);
        calc_vp.DspIns = NULL;
        no_lines = CalcIVG(&calc_view, &calc_vp);
        check(lace_lines == 2 && plain_lines == 1 && no_lines == 0,
              "OK: CalcIVG() reports the copper lines in front of a ViewPort\n",
              "FAIL: CalcIVG() returned unexpected values\n");
    }

    fill_bytes(&ucl, 0, sizeof(ucl));
    ucl_list = UCopperListInit(&ucl, 4);
    check(ucl_list && ucl.FirstCopList == ucl_list && ucl.CopList == ucl_list &&
          ucl_list->MaxCount == 4 && ucl_list->CopPtr == ucl_list->CopIns,
          "OK: UCopperListInit() initialized UCopList state\n",
          "FAIL: UCopperListInit() did not initialize UCopList state\n");
    free_ucoplist_chain(&ucl);

    /* CMove() (no meaningful return value on AmigaOS 3.1) */
    fill_bytes(&ucl, 0, sizeof(ucl));
    ucl_list = UCopperListInit(&ucl, 1);
    if (!ucl_list)
    {
        print("FAIL: UCopperListInit() returned NULL for CMove() test\n");
        errors++;
    }
    else
    {
        CMove(&ucl, (APTR)0x0180, 0x1357);
        check(ucl.CopList == ucl_list && ucl_list->Count == 0 &&
              ucl_list->CopPtr == ucl_list->CopIns &&
              ucl_list->CopIns[0].OpCode == COPPER_MOVE &&
              ucl_list->CopIns[0].u3.u4.u1.DestAddr == 0x0180 &&
              ucl_list->CopIns[0].u3.u4.u2.DestData == 0x1357,
              "OK: CMove() encodes the copper move instruction\n",
              "FAIL: CMove() did not encode the copper move instruction\n");
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

            check(ucl_list->Next && ucl.CopList == ucl_list->Next &&
                  ucl_list->CopIns[1].OpCode == CPRNXTBUF &&
                  ucl_list->CopIns[1].u3.nxtlist == ucl_list->Next &&
                  ucl.CopList->Count == 1 &&
                  ucl.CopList->CopPtr == (ucl.CopList->CopIns + 1) &&
                  ucl.CopList->CopIns[0].OpCode == COPPER_WAIT &&
                  ucl.CopList->CopIns[0].u3.u4.u1.VWaitPos == 0x0020 &&
                  ucl.CopList->CopIns[0].u3.u4.u2.HWaitPos == 0x0040,
                  "OK: CBump() advances and chains copper instruction blocks\n",
                  "FAIL: CBump() did not spill the last instruction into a chained block\n");
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
            check(ucl_list->CopIns[0].OpCode == COPPER_MOVE,
                  "OK: CWait() encodes instructions and leaves full blocks untouched\n",
                  "FAIL: CWait() overwrote a full copper block\n");
        }
    }
    free_ucoplist_chain(&ucl);

    result = MrgCop(&view);
    check(result == MVP_OK && view.LOFCprList != NULL && view.SHFCprList != NULL,
          "OK: MrgCop() built compiled copper lists\n",
          "FAIL: MrgCop() did not build compiled copper lists\n");

    FreeVPortCopLists(&vp);
    check(vp.DspIns == NULL && vp.SprIns == NULL && vp.ClrIns == NULL,
          "OK: FreeVPortCopLists() released the copper lists\n",
          "FAIL: FreeVPortCopLists() did not clear pointers\n");

    FreeCprList(view.LOFCprList);
    FreeCprList(view.SHFCprList);
    view.LOFCprList = NULL;
    view.SHFCprList = NULL;
    FreeCopList(NULL);
    print("OK: FreeCopList()/FreeCprList() are safe\n");

    /* Reloading the active View keeps it active */
    {
        struct View *active = GfxBase->ActiView;

        LoadView(active);
        check(GfxBase->ActiView == active, "OK: LoadView() updated ActiView\n",
              "FAIL: LoadView() did not update ActiView\n");
    }

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
        check(vp.RasInfo->BitMap == bm, "OK: ChangeVPBitMap() updated RasInfo bitmap\n",
              "FAIL: ChangeVPBitMap() did not update RasInfo bitmap\n");
        FreeDBufInfo(dbi);
        dbi = NULL;
    }

    result = CoerceMode(&vp, PAL_MONITOR_ID, 0);
    check(result == (PAL_MONITOR_ID | HIRES_KEY), "OK: CoerceMode() returned expected mode\n",
          "FAIL: CoerceMode() returned unexpected mode\n");

    WaitBOVP(&vp);
    print("OK: WaitBOVP() returned\n");

    /* Display database */
    check(FindDisplayInfo(INVALID_ID) == NULL &&
          FindDisplayInfo(LORES_KEY) != NULL &&
          FindDisplayInfo(HIRES_KEY) != NULL &&
          FindDisplayInfo(PAL_MONITOR_ID | HIRES_KEY) != NULL,
          "OK: FindDisplayInfo() handles INVALID_ID and known IDs\n",
          "FAIL: FindDisplayInfo() returned unexpected handles\n");

    display_id = INVALID_ID;
    for (i = 0; i < 8; i++)
    {
        display_id = NextDisplayInfo(display_id);
        ids[i] = display_id;
        if (display_id == INVALID_ID)
            break;
    }

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
                ok = FALSE;
            for (k = 0; k < j; k++)
                if (ids[k] == ids[j])
                    ok = FALSE;
        }
        check(ok, "OK: NextDisplayInfo() iterates known modes\n",
              "FAIL: NextDisplayInfo() iteration contains invalid or duplicate IDs\n");
    }

    fill_bytes(&disp, 0, sizeof(disp));
    result = GetDisplayInfoData(FindDisplayInfo(PAL_MONITOR_ID | HIRES_KEY), &disp,
                                sizeof(disp), DTAG_DISP, INVALID_ID);
    check(result == 48 && disp.Header.StructID == DTAG_DISP &&
          disp.Header.DisplayID == (PAL_MONITOR_ID | HIRES_KEY) &&
          disp.Header.SkipID == TAG_SKIP && disp.NotAvailable == FALSE &&
          (disp.PropertyFlags & DIPF_IS_SPRITES) && (disp.PropertyFlags & DIPF_IS_WB) &&
          (disp.PropertyFlags & DIPF_IS_DRAGGABLE) && (disp.PropertyFlags & DIPF_IS_PAL) &&
          disp.Resolution.x == 22 && disp.Resolution.y == 44,
          "OK: GetDisplayInfoData(DTAG_DISP) returned expected header/flags\n",
          "FAIL: GetDisplayInfoData(DTAG_DISP) returned wrong data\n");

    /* mode IDs of the default monitor describe the native (PAL) monitor */
    fill_bytes(&disp, 0, sizeof(disp));
    result = GetDisplayInfoData(NULL, &disp, sizeof(disp), DTAG_DISP, LORES_KEY);
    check(result == 48 && disp.Header.DisplayID == (PAL_MONITOR_ID | LORES_KEY) &&
          disp.Resolution.x == 44 && disp.Resolution.y == 44,
          "OK: default monitor IDs resolve to the native PAL monitor\n",
          "FAIL: default monitor IDs did not resolve to the native monitor\n");

    fill_bytes(&dims, 0, sizeof(dims));
    result = GetDisplayInfoData(NULL, &dims, sizeof(dims), DTAG_DIMS, PAL_MONITOR_ID | HIRES_KEY);
    check(result == 66 && dims.Header.DisplayID == (PAL_MONITOR_ID | HIRES_KEY) &&
          dims.MinRasterWidth == 32 && dims.MinRasterHeight == 1 &&
          dims.MaxRasterWidth == 16368 && dims.MaxRasterHeight == 16384 &&
          dims.Nominal.MinX == 0 && dims.Nominal.MinY == 0 &&
          dims.Nominal.MaxX == 639 && dims.Nominal.MaxY == 255 &&
          dims.StdOScan.MaxX == 639 && dims.TxtOScan.MaxY == 255 &&
          dims.MaxOScan.MinX == -72 && dims.MaxOScan.MinY == -15 &&
          dims.MaxOScan.MaxX == 651 && dims.MaxOScan.MaxY == 267 &&
          dims.VideoOScan.MaxX == 663,
          "OK: GetDisplayInfoData(DTAG_DIMS) returned expected geometry\n",
          "FAIL: GetDisplayInfoData(DTAG_DIMS) returned wrong geometry\n");

    fill_bytes(&dims, 0, sizeof(dims));
    result = GetDisplayInfoData(NULL, &dims, sizeof(dims), DTAG_DIMS, PAL_MONITOR_ID | LORESLACE_KEY);
    check(result == 66 && dims.MinRasterWidth == 16 &&
          dims.Nominal.MaxX == 319 && dims.Nominal.MaxY == 511 &&
          dims.MaxOScan.MinX == -36 && dims.MaxOScan.MinY == -30 &&
          dims.MaxOScan.MaxX == 325 && dims.MaxOScan.MaxY == 535,
          "OK: GetDisplayInfoData(DTAG_DIMS) LORES interlace geometry\n",
          "FAIL: GetDisplayInfoData(DTAG_DIMS) LORES interlace geometry wrong\n");

    fill_bytes(&mon, 0, sizeof(mon));
    result = GetDisplayInfoData(NULL, &mon, sizeof(mon), DTAG_MNTR, PAL_MONITOR_ID | LORES_KEY);
    check(result == 88 && mon.Header.DisplayID == (PAL_MONITOR_ID | LORES_KEY) &&
          mon.Mspc == (struct MonitorSpec *)GfxBase->natural_monitor &&
          mon.TotalRows == 312 && mon.TotalColorClocks == 226 &&
          mon.Compatibility == MCOMPAT_MIXED &&
          mon.PreferredModeID == (PAL_MONITOR_ID | HIRES_KEY),
          "OK: GetDisplayInfoData(DTAG_MNTR) returned expected monitor data\n",
          "FAIL: GetDisplayInfoData(DTAG_MNTR) returned wrong monitor data\n");

    fill_bytes(&disp, 0xCC, sizeof(disp));
    result = GetDisplayInfoData(NULL, &disp, 12, DTAG_DISP, LORES_KEY);
    check(result == 12 && ((UBYTE *)&disp)[12] == 0xCC,
          "OK: GetDisplayInfoData() honors truncated size\n",
          "FAIL: GetDisplayInfoData() did not honor truncated size\n");

    result = GetDisplayInfoData(NULL, &disp, sizeof(disp), 0x81234567, LORES_KEY);
    check(result == 0, "OK: GetDisplayInfoData() rejects unknown tag\n",
          "FAIL: GetDisplayInfoData() accepted unknown tag\n");

    result = GetDisplayInfoData(NULL, &disp, sizeof(disp), DTAG_DISP, INVALID_ID);
    check(result == 0, "OK: GetDisplayInfoData() rejects INVALID_ID without handle\n",
          "FAIL: GetDisplayInfoData() accepted INVALID_ID without handle\n");

    fill_bytes(&disp, 0, sizeof(disp));
    result = GetDisplayInfoData(FindDisplayInfo(HIRES_KEY), &disp, sizeof(disp), DTAG_DISP, INVALID_ID);
    check(result == 48 && disp.Header.DisplayID == (PAL_MONITOR_ID | HIRES_KEY),
          "OK: GetDisplayInfoData() resolves handle-based lookup\n",
          "FAIL: GetDisplayInfoData() did not resolve handle-based lookup\n");

    /* Mode property flags */
    {
        ULONG ham = get_flags(PAL_MONITOR_ID | HAM_KEY);
        ULONG ehb = get_flags(PAL_MONITOR_ID | EXTRAHALFBRITE_KEY);
        ULONG lace = get_flags(PAL_MONITOR_ID | HIRESLACE_KEY);

        check((ham & DIPF_IS_HAM) && !(ham & DIPF_IS_WB),
              "OK: HAM mode advertises DIPF_IS_HAM and is no Workbench mode\n",
              "FAIL: HAM mode flags wrong\n");
        check((ehb & DIPF_IS_EXTRAHALFBRITE) && !(ehb & DIPF_IS_WB),
              "OK: EHB mode advertises DIPF_IS_EXTRAHALFBRITE and is no Workbench mode\n",
              "FAIL: EHB mode flags wrong\n");
        check((lace & DIPF_IS_LACE) && (lace & DIPF_IS_WB),
              "OK: HIRESLACE advertises DIPF_IS_LACE\n",
              "FAIL: HIRESLACE flags wrong\n");
    }

    /* BestModeIDA() */
    {
        struct TagItem tags[4];
        ULONG mode;

        tags[0].ti_Tag = BIDTAG_DesiredWidth;
        tags[0].ti_Data = 640;
        tags[1].ti_Tag = BIDTAG_DesiredHeight;
        tags[1].ti_Data = 256;
        tags[2].ti_Tag = TAG_DONE;
        tags[2].ti_Data = 0;
        mode = BestModeIDA(tags);
        check(mode == (PAL_MONITOR_ID | HIRES_KEY), "OK: BestModeIDA() returns PAL HIRES for 640x256\n",
              "FAIL: BestModeIDA() did not return PAL HIRES for 640x256\n");

        tags[2].ti_Tag = BIDTAG_DIPFMustNotHave;
        tags[2].ti_Data = DIPF_IS_LACE;
        tags[3].ti_Tag = TAG_DONE;
        tags[3].ti_Data = 0;
        mode = BestModeIDA(tags);
        check(mode != INVALID_ID && !(mode & LACE), "OK: BestModeIDA() honors DIPF_IS_LACE in MustNotHave\n",
              "FAIL: BestModeIDA() returned LACE mode despite MustNotHave\n");
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
