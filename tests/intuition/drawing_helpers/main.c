#include <exec/types.h>

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

    print("Testing Intuition drawing helpers...\n\n");

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
    image.ImageData = image_data;
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
    if (ReadPixel(window->RPort, 60, 20) == 1 &&
        ReadPixel(window->RPort, 72, 22) == 2) {
        print("  OK: DrawImage renders linked image chains\n\n");
    } else {
        print("  FAIL: DrawImage did not render the full image chain\n\n");
        errors++;
    }

    print("Test 4: DrawImageState() selected-state semantics...\n");
    selected_image.LeftEdge = 0;
    selected_image.TopEdge = 0;
    selected_image.Width = 4;
    selected_image.Height = 4;
    selected_image.Depth = 1;
    selected_image.ImageData = selected_image_data;
    selected_image.PlanePick = 1;
    selected_image.PlaneOnOff = 0;
    selected_image.NextImage = NULL;

    DrawImageState(window->RPort, &selected_image, 90, 20, IDS_SELECTED, draw_info);
    if (ReadPixel(window->RPort, 90, 20) == 2 &&
        ReadPixel(window->RPort, 91, 21) == 3) {
        print("  OK: DrawImageState applies IDS_SELECTED to standard images\n");
    } else {
        print("  FAIL: DrawImageState did not apply selected-state rendering\n");
        errors++;
    }

    print("Test 5: EraseImage()...\n");

    print("READY: before erase\n");
    Delay(50);

    EraseImage(window->RPort, &image, 60, 20);
    if (ReadPixel(window->RPort, 60, 20) == 0 &&
        ReadPixel(window->RPort, 72, 22) == 2) {
        print("  OK: EraseImage clears only the erased image bounds\n");
    } else {
        print("  FAIL: EraseImage did not clear the image bounds correctly\n");
        errors++;
    }

    print("READY: after erase\n\n");
    Delay(50);

    FreeScreenDrawInfo(screen, draw_info);
    CloseWindow(window);
    CloseScreen(screen);

    if (errors == 0) {
        print("PASS: drawing_helpers all tests completed\n");
        return 0;
    }

    print("FAIL: drawing_helpers had errors\n");
    return 20;
}
