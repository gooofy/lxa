/*
 * Test: graphics/blt_bitmap_rastport
 * Tests BltBitMapRastPort() with and without layer clipping.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/layers.h>
#include <graphics/clip.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/layers_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/layers.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct Library *LayersBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

int main(void)
{
    struct BitMap *src_bm;
    struct BitMap *dst_bm;
    struct RastPort src_rp;
    struct RastPort dst_rp;
    struct Layer_Info *li = NULL;
    struct Layer *layer = NULL;
    int errors = 0;

    print("Testing BltBitMapRastPort...\n\n");

    src_bm = AllocBitMap(40, 40, 1, BMF_CLEAR, NULL);
    dst_bm = AllocBitMap(100, 100, 1, BMF_CLEAR, NULL);
    if (!src_bm || !dst_bm) {
        print("FAIL: Could not allocate bitmaps\n");
        if (src_bm) FreeBitMap(src_bm);
        if (dst_bm) FreeBitMap(dst_bm);
        return 20;
    }

    InitRastPort(&src_rp);
    InitRastPort(&dst_rp);
    src_rp.BitMap = src_bm;
    dst_rp.BitMap = dst_bm;

    SetAPen(&src_rp, 1);
    RectFill(&src_rp, 4, 4, 19, 19);

    print("Test 1: Basic blit without layers...\n");
    SetRast(&dst_rp, 0);
    BltBitMapRastPort(src_bm, 4, 4, &dst_rp, 10, 10, 16, 16, 0xC0);
    if (ReadPixel(&dst_rp, 10, 10) == 1 && ReadPixel(&dst_rp, 26, 26) == 0) {
        print("  OK: BltBitMapRastPort copied requested rectangle\n");
    } else {
        print("  FAIL: Basic BltBitMapRastPort copy incorrect\n");
        errors++;
    }
    print("\n");

    print("Test 2: Minterm 0x50 inverts destination...\n");
    SetAPen(&dst_rp, 1);
    SetRast(&dst_rp, 1);
    BltBitMapRastPort(src_bm, 4, 4, &dst_rp, 10, 10, 16, 16, 0x50);
    if (ReadPixel(&dst_rp, 12, 12) == 0) {
        print("  OK: Minterm 0x50 inverted destination bits\n");
    } else {
        print("  FAIL: Minterm 0x50 did not invert destination bits\n");
        errors++;
    }
    print("\n");

    print("Test 3: Layer clipping limits destination writes...\n");
    li = NewLayerInfo();
    if (!li) {
        print("  FAIL: Could not allocate Layer_Info\n");
        errors++;
    } else {
        layer = CreateUpfrontLayer(li, dst_bm, 20, 20, 59, 59, LAYERSIMPLE, NULL);
        if (!layer) {
            print("  FAIL: Could not create layer\n");
            errors++;
        } else {
            dst_rp.Layer = layer;
            SetRast(&dst_rp, 0);
            BltBitMapRastPort(src_bm, 0, 0, &dst_rp, 0, 0, 40, 40, 0xC0);

            if (ReadPixel(&dst_rp, 4, 4) == 1 && ReadPixel(&dst_rp, 39, 4) == 0) {
                print("  OK: Layer clipping constrained destination blit\n");
            } else {
                print("  FAIL: Layer clipping did not constrain destination blit\n");
                errors++;
            }
        }
    }
    print("\n");

    if (layer)
        DeleteLayer(0, layer);
    if (li)
        DisposeLayerInfo(li);
    FreeBitMap(src_bm);
    FreeBitMap(dst_bm);

    if (errors != 0) {
        print("FAIL: blt_bitmap_rastport had errors\n");
        return 20;
    }

    print("PASS: blt_bitmap_rastport all tests passed\n");
    return 0;
}
