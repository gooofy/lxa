/*
 * Test: graphics/blt_mask
 * Tests BltMaskBitMapRastPort() function
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
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
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

int main(void)
{
    struct RastPort *rp;
    struct BitMap *bm_src, *bm_dst, *bm_mask;
    PLANEPTR mask_plane;
    
    print("Testing BltMaskBitMapRastPort...\n\n");
    
    /* Create bitmaps */
    bm_src = AllocBitMap(32, 32, 1, BMF_CLEAR, NULL);
    bm_dst = AllocBitMap(64, 64, 1, BMF_CLEAR, NULL);
    bm_mask = AllocBitMap(32, 32, 1, BMF_CLEAR, NULL);
    
    if (!bm_src || !bm_dst || !bm_mask) {
        print("ERROR: Could not allocate bitmaps\n");
        if (bm_src) FreeBitMap(bm_src);
        if (bm_dst) FreeBitMap(bm_dst);
        if (bm_mask) FreeBitMap(bm_mask);
        return 1;
    }
    
    /* Create rastport for destination */
    rp = (struct RastPort *)AllocMem(sizeof(struct RastPort), MEMF_CLEAR);
    if (!rp) {
        print("ERROR: Could not allocate rastport\n");
        FreeBitMap(bm_src);
        FreeBitMap(bm_dst);
        FreeBitMap(bm_mask);
        return 1;
    }
    
    InitRastPort(rp);
    rp->BitMap = bm_dst;
    
    /* Fill source bitmap with pattern */
    {
        struct RastPort temp_rp;
        InitRastPort(&temp_rp);
        temp_rp.BitMap = bm_src;
        SetAPen(&temp_rp, 1);
        RectFill(&temp_rp, 0, 0, 31, 31);
    }
    
    /* Create a simple mask (checkerboard pattern) */
    mask_plane = bm_mask->Planes[0];
    if (mask_plane) {
        UBYTE *mask_data = (UBYTE *)mask_plane;
        LONG i;
        for (i = 0; i < 128; i++) {  /* 32*32 bits / 8 = 128 bytes */
            mask_data[i] = (i & 1) ? 0xAA : 0x55;  /* Alternating pattern */
        }
    }
    
    print("OK: Resources allocated\n\n");
    
    /* Test 1: Basic masked blit */
    print("Test 1: Basic masked blit...\n");
    {
        BltMaskBitMapRastPort(bm_src, 0, 0, rp, 16, 16, 32, 32, 0xC0, mask_plane);
        print("  OK: BltMaskBitMapRastPort executed\n");
    }
    print("\n");
    
    /* Test 2: Masked blit with NULL mask (should copy all) */
    print("Test 2: Masked blit with NULL mask...\n");
    {
        SetRast(rp, 0);
        BltMaskBitMapRastPort(bm_src, 0, 0, rp, 0, 0, 16, 16, 0xC0, NULL);
        print("  OK: BltMaskBitMapRastPort with NULL mask executed\n");
    }
    print("\n");
    
    /* Test 3: Masked blit with different minterm */
    print("Test 3: Masked blit with minterm 0x50...\n");
    {
        SetRast(rp, 0);
        BltMaskBitMapRastPort(bm_src, 8, 8, rp, 24, 24, 16, 16, 0x50, mask_plane);
        print("  OK: BltMaskBitMapRastPort with minterm 0x50 executed\n");
    }
    print("\n");
    
    /* Test 4: Partial blit at edge */
    print("Test 4: Partial blit at destination edge...\n");
    {
        BltMaskBitMapRastPort(bm_src, 0, 0, rp, 56, 56, 16, 16, 0xC0, mask_plane);
        print("  OK: Edge blit executed\n");
    }
    print("\n");
    
    /* Cleanup */
    FreeMem(rp, sizeof(struct RastPort));
    FreeBitMap(bm_src);
    FreeBitMap(bm_dst);
    FreeBitMap(bm_mask);
    
    print("PASS: blt_mask all tests passed\n");
    return 0;
}
