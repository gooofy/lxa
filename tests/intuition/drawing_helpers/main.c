#include <exec/types.h>
#include <exec/memory.h>

#include <graphics/gfx.h>
#include <graphics/rastport.h>

#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>

#include <clib/dos_protos.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>

#include <inline/dos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/intuition.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct IntuitionBase *IntuitionBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static void print_pens(struct RastPort *rp, const WORD *xy, int n)
{
    char buf[8];
    int i;
    print("  pens");
    for (i = 0; i < n; i++) {
        LONG v = ReadPixel(rp, xy[2 * i], xy[2 * i + 1]);
        buf[0] = ' ';
        buf[1] = v < 0 ? '-' : '0' + (v % 10);
        buf[2] = 0;
        print(buf);
    }
    print("\n");
}

static const WORD probe3[] = { 60, 20, 61, 21, 67, 20, 72, 22, 75, 25, 76, 26 };
static const WORD probe4[] = { 90, 20, 91, 21, 93, 23, 94, 24 };

static UWORD image_data[] = {
    0xFF00,
    0x8100,
    0x8100,
    0x8100,
    0x8100,
    0x8100,
    0x8100,
    0xFF00
};

static UWORD selected_image_data[] = {
    0xF000,
    0x9000,
    0x9000,
    0xF000
};

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct DrawInfo *draw_info;
    struct Border shine_border;
    struct Border shadow_border;
    struct TextAttr text_attr;
    struct IntuiText text;
    struct Image image;
    struct Image chain_fill;
    struct Image selected_image;
    WORD border_xy[] = {0, 0, 23, 0, 23, 11, 0, 11, 0, 0};
    UBYTE shine_pen = 2;
    UBYTE shadow_pen = 1;
    UBYTE text_pen = 1;
    UBYTE background_pen = 0;
    LONG text_length;
    int errors = 0;
    UWORD *chip_image;
    UWORD *chip_selected;

    print("Testing Intuition drawing helpers...\n\n");

    /* image data is read by the blitter: it must be in chip memory */
    chip_image = (UWORD *)AllocMem(sizeof(image_data), MEMF_CHIP);
    chip_selected = (UWORD *)AllocMem(sizeof(selected_image_data), MEMF_CHIP);
    if (!chip_image || !chip_selected) {
        print("FAIL: Could not allocate chip memory\n");
        return 20;
    }
    CopyMem(image_data, chip_image, sizeof(image_data));
    CopyMem(selected_image_data, chip_selected, sizeof(selected_image_data));

    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = 320;
    ns.Height = 200;
    ns.Depth = 2;
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.ViewModes = 0;
    ns.Type = CUSTOMSCREEN;
    ns.Font = NULL;
    ns.DefaultTitle = (UBYTE *)"Drawing Helpers";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (!screen) {
        print("FAIL: Could not open screen\n");
        return 20;
    }

    nw.LeftEdge = 12;
    nw.TopEdge = 14;
    nw.Width = 180;
    nw.Height = 90;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = 0;
    nw.Flags = WFLG_BORDERLESS | WFLG_ACTIVATE;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = NULL;
    nw.Screen = screen;
    nw.BitMap = NULL;
    nw.MinWidth = 0;
    nw.MinHeight = 0;
    nw.MaxWidth = 0;
    nw.MaxHeight = 0;
    nw.Type = CUSTOMSCREEN;

    window = OpenWindow(&nw);
    if (!window) {
        print("FAIL: Could not open window\n");
        CloseScreen(screen);
        return 20;
    }

    draw_info = GetScreenDrawInfo(screen);
    if (!draw_info) {
        print("FAIL: GetScreenDrawInfo() returned NULL\n");
        CloseWindow(window);
        CloseScreen(screen);
        return 20;
    }

    shine_pen = draw_info->dri_Pens[SHINEPEN];
    shadow_pen = draw_info->dri_Pens[SHADOWPEN];
    text_pen = draw_info->dri_Pens[TEXTPEN];
    background_pen = draw_info->dri_Pens[BACKGROUNDPEN];

    print("Test 1: DrawBorder()...\n");
    shadow_border.LeftEdge = 1;
    shadow_border.TopEdge = 1;
    shadow_border.FrontPen = shadow_pen;
    shadow_border.BackPen = background_pen;
    shadow_border.DrawMode = JAM1;
    shadow_border.Count = 5;
    shadow_border.XY = border_xy;
    shadow_border.NextBorder = &shine_border;

    shine_border.LeftEdge = 0;
    shine_border.TopEdge = 0;
    shine_border.FrontPen = shine_pen;
    shine_border.BackPen = background_pen;
    shine_border.DrawMode = JAM1;
    shine_border.Count = 5;
    shine_border.XY = border_xy;
    shine_border.NextBorder = NULL;

    DrawBorder(window->RPort, &shadow_border, 10, 10);
    if (ReadPixel(window->RPort, 10, 10) == shine_pen &&
        ReadPixel(window->RPort, 11, 11) == shadow_pen) {
        print("  OK: DrawBorder renders both shine and shadow chains\n\n");
    } else {
        print("  FAIL: DrawBorder did not render expected pixels\n\n");
        errors++;
    }

    print("Test 2: PrintIText() / IntuiTextLength()...\n");
    text_attr.ta_Name = NULL;
    text_attr.ta_YSize = 8;
    text_attr.ta_Style = 0;
    text_attr.ta_Flags = 0;

    if (draw_info->dri_Font && draw_info->dri_Font->tf_Message.mn_Node.ln_Name) {
        text_attr.ta_Name = draw_info->dri_Font->tf_Message.mn_Node.ln_Name;
        text_attr.ta_YSize = draw_info->dri_Font->tf_YSize;
        text_attr.ta_Style = draw_info->dri_Font->tf_Style;
        text_attr.ta_Flags = draw_info->dri_Font->tf_Flags;
    }

    text.FrontPen = text_pen;
    text.BackPen = background_pen;
    text.DrawMode = JAM2;
    text.LeftEdge = 0;
    text.TopEdge = 0;
    text.ITextFont = text_attr.ta_Name ? &text_attr : NULL;
    text.IText = (UBYTE *)"HELLO";
    text.NextText = NULL;

    text_length = IntuiTextLength(&text);
    if (text_length == 40) {
        print("  OK: IntuiTextLength matches Topaz 8 metrics\n");
    } else {
        print("  FAIL: IntuiTextLength returned an unexpected width\n");
        errors++;
    }

    PrintIText(window->RPort, &text, 10, 30);
    print("  OK: PrintIText completed\n\n");

    print("Test 3: DrawImage() chain rendering...\n");
    image.LeftEdge = 0;
    image.TopEdge = 0;
    image.Width = 8;
    image.Height = 8;
    image.Depth = 1;
    image.ImageData = chip_image;
    image.PlanePick = 1;
    image.PlaneOnOff = 0;
    image.NextImage = &chain_fill;

    chain_fill.LeftEdge = 12;
    chain_fill.TopEdge = 2;
    chain_fill.Width = 4;
    chain_fill.Height = 4;
    chain_fill.Depth = 1;
    chain_fill.ImageData = NULL;
    chain_fill.PlanePick = 0;
    chain_fill.PlaneOnOff = 2;
    chain_fill.NextImage = NULL;

    DrawImage(window->RPort, &image, 60, 20);
    print_pens(window->RPort, probe3, 6);
    if (ReadPixel(window->RPort, 60, 20) == 1 &&
        ReadPixel(window->RPort, 72, 22) == 2) {
        print("  OK: DrawImage renders linked image chains\n\n");
    } else {
        print("  FAIL: DrawImage did not render the full image chain\n\n");
        errors++;
    }

    print("Test 4: DrawImageState() with a standard image...\n");
    selected_image.LeftEdge = 0;
    selected_image.TopEdge = 0;
    selected_image.Width = 4;
    selected_image.Height = 4;
    selected_image.Depth = 1;
    selected_image.ImageData = chip_selected;
    selected_image.PlanePick = 1;
    selected_image.PlaneOnOff = 0;
    selected_image.NextImage = NULL;

    DrawImageState(window->RPort, &selected_image, 90, 20, IDS_SELECTED, draw_info);
    print_pens(window->RPort, probe4, 4);
    /* AmigaOS 3.1 reference: the state only matters for BOOPSI images; a
     * standard image is drawn as by DrawImage() */
    if (ReadPixel(window->RPort, 90, 20) == 1 &&
        ReadPixel(window->RPort, 91, 21) == 0) {
        print("  OK: DrawImageState draws standard images like DrawImage\n");
    } else {
        print("  FAIL: DrawImageState did not draw the standard image\n");
        errors++;
    }

    print("Test 5: EraseImage()...\n");

    print("READY: before erase\n");
    Delay(50);

    EraseImage(window->RPort, &image, 60, 20);
    print_pens(window->RPort, probe3, 6);
    /* AmigaOS 3.1 reference: the whole NextImage chain is erased */
    if (ReadPixel(window->RPort, 60, 20) == 0 &&
        ReadPixel(window->RPort, 72, 22) == 0) {
        print("  OK: EraseImage clears the image chain\n");
    } else {
        print("  FAIL: EraseImage did not clear the image bounds correctly\n");
        errors++;
    }

    print("READY: after erase\n\n");
    Delay(50);

    FreeScreenDrawInfo(screen, draw_info);
    CloseWindow(window);
    CloseScreen(screen);
    FreeMem(chip_image, sizeof(image_data));
    FreeMem(chip_selected, sizeof(selected_image_data));

    if (errors == 0) {
        print("PASS: drawing_helpers all tests completed\n");
        return 0;
    }

    print("FAIL: drawing_helpers had errors\n");
    return 20;
}
