/*
 * Test: graphics/clipblit
 * Tests ClipBlit() function for layer-aware blitting
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
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

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + 15;
    BOOL neg = FALSE;

    *p = '\0';
    if (n < 0) {
        neg = TRUE;
        n = -n;
    }
    do {
        *(--p) = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    if (neg)
        *(--p) = '-';
    print(p);
}

int main(void)
{
    struct Library *LayersBase;
    struct RastPort *rp_src, *rp_dst;
    struct BitMap *bm_src, *bm_dst;
    struct Layer_Info *li;
    struct Layer *layer;
    int errors = 0;
    
    print("Testing ClipBlit...\n\n");
    
    /* Open layers.library */
    LayersBase = OpenLibrary((CONST_STRPTR)"layers.library", 0);
    if (!LayersBase) {
        print("ERROR: Could not open layers.library\n");
        return 1;
    }
    
    /* Create source bitmap and rastport */
    bm_src = AllocBitMap(50, 50, 1, BMF_CLEAR, NULL);
    bm_dst = AllocBitMap(100, 100, 1, BMF_CLEAR, NULL);
    
    if (!bm_src || !bm_dst) {
        print("ERROR: Could not allocate bitmaps\n");
        if (bm_src) FreeBitMap(bm_src);
        if (bm_dst) FreeBitMap(bm_dst);
        CloseLibrary(LayersBase);
        return 1;
    }
    
    rp_src = (struct RastPort *)AllocMem(sizeof(struct RastPort), MEMF_CLEAR);
    rp_dst = (struct RastPort *)AllocMem(sizeof(struct RastPort), MEMF_CLEAR);
    
    if (!rp_src || !rp_dst) {
        print("ERROR: Could not allocate rastports\n");
        if (rp_src) FreeMem(rp_src, sizeof(struct RastPort));
        if (rp_dst) FreeMem(rp_dst, sizeof(struct RastPort));
        FreeBitMap(bm_src);
        FreeBitMap(bm_dst);
        CloseLibrary(LayersBase);
        return 1;
    }
    
    InitRastPort(rp_src);
    InitRastPort(rp_dst);
    rp_src->BitMap = bm_src;
    rp_dst->BitMap = bm_dst;
    
    /* Draw something in source */
    SetAPen(rp_src, 1);
    RectFill(rp_src, 10, 10, 40, 40);
    
    print("OK: Resources allocated\n\n");
    
    /* Test 1: ClipBlit without layers */
    print("Test 1: ClipBlit without layers...\n");
    {
        SetRast(rp_dst, 0);
        ClipBlit(rp_src, 10, 10, rp_dst, 20, 20, 30, 30, 0xC0);
        
        /* Check if pixels were copied */
        ULONG pixel = ReadPixel(rp_dst, 25, 25);
        if (pixel == 1) {
            print("  OK: Pixels copied without layers\n");
        } else {
            print("  FAIL: Expected copied pixel color 1, got ");
            print_num(pixel);
            print("\n");
            errors++;
        }
    }
    print("\n");
    
    /* Test 2: ClipBlit with layer */
    print("Test 2: ClipBlit with layer...\n");
    {
        /* Create Layer_Info */
        li = NewLayerInfo();
        if (!li) {
            print("  ERROR: Could not create Layer_Info\n");
        } else {
            /* Create layer */
            layer = CreateUpfrontLayer(li, bm_dst, 10, 10, 90, 90, 0, NULL);
            if (!layer) {
                print("  ERROR: Could not create layer\n");
            } else {
                rp_dst->Layer = layer;
                
                /* Clear destination */
                SetRast(rp_dst, 0);
                
                /* Blit with layer */
                ClipBlit(rp_src, 10, 10, rp_dst, 0, 0, 30, 30, 0xC0);

                if (ReadPixel(rp_dst, 15, 15) == 1 && ReadPixel(rp_dst, 95, 15) == 0) {
                    print("  OK: ClipBlit with layer clipped to visible region\n");
                } else {
                    print("  FAIL: ClipBlit with layer ignored layer bounds\n");
                    errors++;
                }
                
                DeleteLayer(0, layer);
            }
            DisposeLayerInfo(li);
        }
    }
    print("\n");
    
    /* Test 3: ClipBlit with minterm */
    print("Test 3: ClipBlit with different minterm...\n");
    {
        SetAPen(rp_dst, 1);
        SetRast(rp_dst, 1);
        rp_dst->Layer = NULL;
        
        /* Minterm 0x50 inverts destination bits */
        ClipBlit(rp_src, 15, 15, rp_dst, 5, 5, 20, 20, 0x50);

        if (ReadPixel(rp_dst, 10, 10) == 0) {
            print("  OK: ClipBlit minterm 0x50 inverted destination bits\n");
        } else {
            print("  FAIL: ClipBlit minterm 0x50 did not invert destination bits\n");
            errors++;
        }
    }
    print("\n");

    /* Test 4: ClipBlit from obscured source area */
    print("Test 4: ClipBlit honors source layer visibility...\n");
    {
        struct Layer *src_back;
        struct Layer *src_front;
        struct Layer_Info *src_li;
        struct RastPort rp_back;
        struct RastPort rp_front;

        src_li = NewLayerInfo();
        if (!src_li) {
            print("  FAIL: Could not create source Layer_Info\n");
            errors++;
        } else {
            src_back = CreateUpfrontLayer(src_li, bm_src, 0, 0, 49, 49, LAYERSIMPLE, NULL);
            src_front = CreateUpfrontLayer(src_li, bm_src, 20, 0, 49, 49, LAYERSIMPLE, NULL);

            if (!src_back || !src_front) {
                print("  FAIL: Could not create source layers\n");
                errors++;
            } else {
                InitRastPort(&rp_back);
                InitRastPort(&rp_front);
                rp_back.BitMap = bm_src;
                rp_back.Layer = src_back;
                rp_front.BitMap = bm_src;
                rp_front.Layer = src_front;

                SetAPen(&rp_back, 1);
                RectFill(&rp_back, 0, 0, 49, 49);
                SetAPen(&rp_front, 0);
                RectFill(&rp_front, 0, 0, 29, 49);

                SetRast(rp_dst, 0);
                ClipBlit(&rp_back, 0, 0, rp_dst, 0, 0, 50, 50, 0xC0);

                if (ReadPixel(rp_dst, 10, 10) == 1 && ReadPixel(rp_dst, 30, 10) == 0) {
                    print("  OK: ClipBlit skipped obscured source pixels\n");
                } else {
                    print("  FAIL: ClipBlit copied obscured source pixels\n");
                    errors++;
                }
            }

            if (src_front)
                DeleteLayer(0, src_front);
            if (src_back)
                DeleteLayer(0, src_back);
            DisposeLayerInfo(src_li);
        }
    }
    print("\n");
    
    /* Cleanup */
    FreeMem(rp_src, sizeof(struct RastPort));
    FreeMem(rp_dst, sizeof(struct RastPort));
    FreeBitMap(bm_src);
    FreeBitMap(bm_dst);
    CloseLibrary(LayersBase);
    
    if (errors != 0) {
        print("FAIL: clipblit had errors\n");
        return 20;
    }

    print("PASS: clipblit all tests passed\n");
    return 0;
}
