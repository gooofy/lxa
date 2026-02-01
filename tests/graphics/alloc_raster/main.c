/*
 * Test: graphics/alloc_raster
 * Tests AllocRaster()/FreeRaster() memory handling
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
    PLANEPTR plane;
    ULONG size;
    int errors = 0;
    ULONG i;

    print("Testing AllocRaster()/FreeRaster()...\n");

    /* Test standard 320x200 allocation */
    plane = AllocRaster(320, 200);
    if (!plane) {
        print("FAIL: AllocRaster(320, 200) returned NULL\n");
        errors++;
    } else {
        print("OK: AllocRaster(320, 200) succeeded\n");

        /* Check RASSIZE macro */
        size = RASSIZE(320, 200);
        if (size != 8000) {
            print("FAIL: RASSIZE(320, 200) != 8000\n");
            errors++;
        } else {
            print("OK: RASSIZE(320, 200) = 8000\n");
        }

        /* Verify memory is accessible (write and read back) */
        /* Note: AllocRaster may use MEMF_CHIP which might not be cleared */
        ((UBYTE *)plane)[0] = 0xAA;
        ((UBYTE *)plane)[size-1] = 0x55;
        if (((UBYTE *)plane)[0] != 0xAA || ((UBYTE *)plane)[size-1] != 0x55) {
            print("FAIL: Memory not accessible\n");
            errors++;
        } else {
            print("OK: Raster memory is accessible\n");
        }

        FreeRaster(plane, 320, 200);
        print("OK: FreeRaster() completed\n");
    }

    /* Test smaller allocation */
    plane = AllocRaster(64, 64);
    if (!plane) {
        print("FAIL: AllocRaster(64, 64) returned NULL\n");
        errors++;
    } else {
        print("OK: AllocRaster(64, 64) succeeded\n");

        size = RASSIZE(64, 64);
        if (size != 512) {
            print("FAIL: RASSIZE(64, 64) != 512\n");
            errors++;
        } else {
            print("OK: RASSIZE(64, 64) = 512\n");
        }

        FreeRaster(plane, 64, 64);
        print("OK: FreeRaster(64, 64) completed\n");
    }

    /* Test non-word-aligned width (17 pixels -> 4 bytes per row) */
    plane = AllocRaster(17, 10);
    if (!plane) {
        print("FAIL: AllocRaster(17, 10) returned NULL\n");
        errors++;
    } else {
        print("OK: AllocRaster(17, 10) succeeded\n");

        size = RASSIZE(17, 10);
        if (size != 40) {
            print("FAIL: RASSIZE(17, 10) != 40\n");
            errors++;
        } else {
            print("OK: RASSIZE(17, 10) = 40\n");
        }

        FreeRaster(plane, 17, 10);
        print("OK: FreeRaster(17, 10) completed\n");
    }

    /* Test multiple alloc/free cycles */
    print("Testing multiple alloc/free cycles...\n");
    for (i = 0; i < 10; i++) {
        plane = AllocRaster(100, 100);
        if (!plane) {
            print("FAIL: AllocRaster failed on iteration\n");
            errors++;
            break;
        }
        FreeRaster(plane, 100, 100);
    }
    if (i == 10) {
        print("OK: 10 alloc/free cycles completed\n");
    }

    /* Final result */
    if (errors == 0) {
        print("PASS: AllocRaster/FreeRaster all tests passed\n");
        return 0;
    } else {
        print("FAIL: AllocRaster/FreeRaster had errors\n");
        return 20;
    }
}
