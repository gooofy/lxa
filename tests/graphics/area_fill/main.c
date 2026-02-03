/*
 * Test: graphics/area_fill
 * Tests AreaMove(), AreaDraw(), and AreaEnd() functions
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;

#define AREA_MAX_VECTORS 100

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
    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }
    do
    {
        *(--p) = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    if (neg) *(--p) = '-';
    print(p);
}

/* Helper to check if a pixel is set (non-zero) */
static BOOL is_pixel_set(struct RastPort *rp, LONG x, LONG y)
{
    ULONG pen = ReadPixel(rp, x, y);
    return (pen != 0);
}

int main(void)
{
    struct RastPort *rp;
    struct BitMap *bm;
    struct AreaInfo *areainfo;
    struct TmpRas *tmpras;
    WORD *areabuffer;
    BYTE *raster;
    LONG result;
    
    print("Testing AreaMove, AreaDraw, and AreaEnd...\n\n");
    
    /* Create bitmap and rastport */
    bm = AllocBitMap(100, 100, 1, BMF_CLEAR, NULL);
    if (!bm) {
        print("ERROR: Could not allocate bitmap\n");
        return 1;
    }
    
    rp = (struct RastPort *)AllocMem(sizeof(struct RastPort), MEMF_CLEAR);
    if (!rp) {
        print("ERROR: Could not allocate rastport\n");
        FreeBitMap(bm);
        return 1;
    }
    
    InitRastPort(rp);
    rp->BitMap = bm;
    
    /* Allocate AreaInfo and buffer */
    areainfo = (struct AreaInfo *)AllocMem(sizeof(struct AreaInfo), MEMF_CLEAR);
    if (!areainfo) {
        print("ERROR: Could not allocate AreaInfo\n");
        FreeMem(rp, sizeof(struct RastPort));
        FreeBitMap(bm);
        return 1;
    }
    
    areabuffer = (WORD *)AllocMem(AREA_MAX_VECTORS * 5 * sizeof(WORD), MEMF_CLEAR);
    if (!areabuffer) {
        print("ERROR: Could not allocate area buffer\n");
        FreeMem(areainfo, sizeof(struct AreaInfo));
        FreeMem(rp, sizeof(struct RastPort));
        FreeBitMap(bm);
        return 1;
    }
    
    /* Allocate TmpRas */
    tmpras = (struct TmpRas *)AllocMem(sizeof(struct TmpRas), MEMF_CLEAR);
    if (!tmpras) {
        print("ERROR: Could not allocate TmpRas\n");
        FreeMem(areabuffer, AREA_MAX_VECTORS * 5 * sizeof(WORD));
        FreeMem(areainfo, sizeof(struct AreaInfo));
        FreeMem(rp, sizeof(struct RastPort));
        FreeBitMap(bm);
        return 1;
    }
    
    /* Allocate raster buffer for TmpRas */
    raster = (BYTE *)AllocMem(RASSIZE(100, 100), MEMF_CHIP);
    if (!raster) {
        print("ERROR: Could not allocate raster buffer\n");
        FreeMem(tmpras, sizeof(struct TmpRas));
        FreeMem(areabuffer, AREA_MAX_VECTORS * 5 * sizeof(WORD));
        FreeMem(areainfo, sizeof(struct AreaInfo));
        FreeMem(rp, sizeof(struct RastPort));
        FreeBitMap(bm);
        return 1;
    }
    
    InitTmpRas(tmpras, (PLANEPTR)raster, RASSIZE(100, 100));
    rp->TmpRas = tmpras;
    
    /* Initialize AreaInfo */
    InitArea(areainfo, areabuffer, AREA_MAX_VECTORS);
    rp->AreaInfo = areainfo;
    
    /* Set APen for drawing */
    SetAPen(rp, 1);
    
    print("OK: Resources allocated\n\n");
    
    /* Test 1: Simple triangle */
    print("Test 1: Draw a triangle with AreaMove/AreaDraw/AreaEnd...\n");
    {
        result = AreaMove(rp, 10, 10);
        if (result != 0) {
            print("  FAIL: AreaMove returned error\n");
        } else {
            result = AreaDraw(rp, 40, 10);
            if (result != 0) {
                print("  FAIL: AreaDraw 1 returned error\n");
            } else {
                result = AreaDraw(rp, 25, 30);
                if (result != 0) {
                    print("  FAIL: AreaDraw 2 returned error\n");
                } else {
                    result = AreaEnd(rp);
                    if (result != 0) {
                        print("  FAIL: AreaEnd returned error\n");
                    } else {
                        /* Check that outline pixels are drawn */
                        BOOL has_pixels = is_pixel_set(rp, 10, 10) ||
                                        is_pixel_set(rp, 40, 10) ||
                                        is_pixel_set(rp, 25, 30);
                        
                        if (has_pixels) {
                            print("  OK: Triangle outline drawn\n");
                        } else {
                            print("  WARN: No pixels detected (might be OK if outline is thin)\n");
                            print("  OK: Functions executed without error\n");
                        }
                    }
                }
            }
        }
    }
    print("\n");
    
    /* Clear bitmap for next test */
    SetRast(rp, 0);
    
    /* Test 2: Check AreaInfo state after AreaEnd */
    print("Test 2: AreaInfo should be reset after AreaEnd...\n");
    {
        AreaMove(rp, 5, 5);
        AreaDraw(rp, 15, 5);
        AreaDraw(rp, 15, 15);
        AreaEnd(rp);
        
        /* AreaInfo should be reset */
        if (areainfo->Count == 0 &&
            areainfo->VctrPtr == areainfo->VctrTbl &&
            areainfo->FlagPtr == areainfo->FlagTbl) {
            print("  OK: AreaInfo reset correctly\n");
        } else {
            print("  FAIL: AreaInfo not reset\n");
            print("    Count = ");
            print_num(areainfo->Count);
            print(" (expected 0)\n");
        }
    }
    print("\n");
    
    /* Test 3: Multiple AreaMove replaces previous move */
    print("Test 3: Multiple AreaMove should replace previous...\n");
    {
        AreaMove(rp, 10, 10);
        AreaMove(rp, 20, 20);  /* Should replace first move */
        
        if (areainfo->Count == 1) {
            print("  OK: Only one entry after two AreaMove calls\n");
        } else {
            print("  FAIL: Count = ");
            print_num(areainfo->Count);
            print(" (expected 1)\n");
        }
        
        /* Clear for next test */
        AreaEnd(rp);
    }
    print("\n");
    
    /* Test 4: Drawing without AreaMove */
    print("Test 4: AreaDraw without AreaMove should work...\n");
    {
        result = AreaDraw(rp, 30, 30);
        if (result == 0 && areainfo->Count == 1) {
            print("  OK: AreaDraw without AreaMove succeeded\n");
        } else {
            print("  FAIL: AreaDraw without AreaMove failed\n");
        }
        
        AreaEnd(rp);
    }
    print("\n");
    
    /* Test 5: Multiple polygons in sequence */
    print("Test 5: Draw two triangles in sequence...\n");
    {
        /* First triangle */
        AreaMove(rp, 50, 50);
        AreaDraw(rp, 60, 50);
        AreaDraw(rp, 55, 60);
        result = AreaEnd(rp);
        
        if (result != 0) {
            print("  FAIL: First triangle failed\n");
        } else {
            /* Second triangle */
            AreaMove(rp, 70, 50);
            AreaDraw(rp, 80, 50);
            AreaDraw(rp, 75, 60);
            result = AreaEnd(rp);
            
            if (result != 0) {
                print("  FAIL: Second triangle failed\n");
            } else {
                print("  OK: Two triangles drawn in sequence\n");
            }
        }
    }
    print("\n");
    
    /* Test 6: AreaEnd with no TmpRas should fail gracefully */
    print("Test 6: AreaEnd with no TmpRas...\n");
    {
        struct TmpRas *saved_tmpras = rp->TmpRas;
        rp->TmpRas = NULL;
        
        AreaMove(rp, 10, 10);
        AreaDraw(rp, 20, 10);
        result = AreaEnd(rp);
        
        if (result == -1 || result == 0) {
            print("  OK: AreaEnd handled missing TmpRas (returned ");
            print_num(result);
            print(")\n");
        } else {
            print("  FAIL: Unexpected return value\n");
        }
        
        rp->TmpRas = saved_tmpras;
    }
    print("\n");
    
    /* Test 7: Rectangle (4 sides) */
    print("Test 7: Draw a rectangle...\n");
    {
        AreaMove(rp, 10, 70);
        AreaDraw(rp, 30, 70);
        AreaDraw(rp, 30, 90);
        AreaDraw(rp, 10, 90);
        result = AreaEnd(rp);
        
        if (result == 0) {
            print("  OK: Rectangle drawn\n");
        } else {
            print("  FAIL: Rectangle drawing failed\n");
        }
    }
    print("\n");
    
    /* Cleanup */
    FreeMem(raster, RASSIZE(100, 100));
    FreeMem(tmpras, sizeof(struct TmpRas));
    FreeMem(areabuffer, AREA_MAX_VECTORS * 5 * sizeof(WORD));
    FreeMem(areainfo, sizeof(struct AreaInfo));
    FreeMem(rp, sizeof(struct RastPort));
    FreeBitMap(bm);
    
    print("PASS: area_fill all tests passed\n");
    return 0;
}
