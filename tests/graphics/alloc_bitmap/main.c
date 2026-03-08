/*
 * Test: graphics/alloc_bitmap
 * Tests AllocBitMap()/FreeBitMap() allocation and cleanup semantics.
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

static int verify_zeroed_plane(PLANEPTR plane, ULONG width, ULONG height)
{
    ULONG size = RASSIZE(width, height);
    ULONG i;

    for (i = 0; i < size; i++)
    {
        if (((UBYTE *)plane)[i] != 0)
            return 0;
    }

    return 1;
}

int main(void)
{
    struct BitMap *bm;
    struct BitMap *manual;
    ULONG plane;
    int errors = 0;

    print("Testing AllocBitMap()/FreeBitMap()...\n");

    bm = AllocBitMap(17, 10, 3, BMF_CLEAR, NULL);
    if (!bm)
    {
        print("FAIL: AllocBitMap(17, 10, 3, BMF_CLEAR, NULL) returned NULL\n");
        errors++;
    }
    else
    {
        if (bm->BytesPerRow != 4)
        {
            print("FAIL: BytesPerRow != 4\n");
            errors++;
        }
        else
        {
            print("OK: BytesPerRow rounded to 4 bytes\n");
        }

        if (bm->Rows != 10)
        {
            print("FAIL: Rows != 10\n");
            errors++;
        }
        else
        {
            print("OK: Rows=10\n");
        }

        if (bm->Depth != 3)
        {
            print("FAIL: Depth != 3\n");
            errors++;
        }
        else
        {
            print("OK: Depth=3\n");
        }

        if ((bm->Flags & (BMF_STANDARD | BMF_CLEAR)) != (BMF_STANDARD | BMF_CLEAR))
        {
            print("FAIL: Flags missing BMF_STANDARD or BMF_CLEAR\n");
            errors++;
        }
        else
        {
            print("OK: Flags include BMF_STANDARD and BMF_CLEAR\n");
        }

        for (plane = 0; plane < bm->Depth; plane++)
        {
            if (!bm->Planes[plane])
            {
                print("FAIL: Plane allocation returned NULL\n");
                errors++;
                break;
            }

            if (!verify_zeroed_plane(bm->Planes[plane], 17, 10))
            {
                print("FAIL: Cleared plane contains non-zero data\n");
                errors++;
                break;
            }

            ((UBYTE *)bm->Planes[plane])[0] = (UBYTE)(0x10 + plane);
            ((UBYTE *)bm->Planes[plane])[RASSIZE(17, 10) - 1] = (UBYTE)(0x20 + plane);
        }

        if (plane == bm->Depth)
        {
            print("OK: Plane memory allocated, cleared, and writable\n");
        }

        FreeBitMap(bm);
        print("OK: FreeBitMap() released allocated bitmap\n");
    }

    bm = AllocBitMap(32, 4, 2, 0, NULL);
    if (!bm)
    {
        print("FAIL: AllocBitMap(32, 4, 2, 0, NULL) returned NULL\n");
        errors++;
    }
    else
    {
        if ((bm->Flags & BMF_STANDARD) == 0 || (bm->Flags & BMF_CLEAR) != 0)
        {
            print("FAIL: Flags incorrect for non-cleared allocation\n");
            errors++;
        }
        else
        {
            print("OK: Non-cleared allocation preserves public flags\n");
        }

        ((UBYTE *)bm->Planes[0])[0] = 0x5A;
        ((UBYTE *)bm->Planes[1])[RASSIZE(32, 4) - 1] = 0xA5;
        if (((UBYTE *)bm->Planes[0])[0] != 0x5A ||
            ((UBYTE *)bm->Planes[1])[RASSIZE(32, 4) - 1] != 0xA5)
        {
            print("FAIL: Plane memory not writable\n");
            errors++;
        }
        else
        {
            print("OK: Non-cleared plane memory is writable\n");
        }

        FreeBitMap(bm);
        print("OK: FreeBitMap() released second bitmap\n");
    }

    manual = (struct BitMap *)AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
    if (!manual)
    {
        print("FAIL: AllocMem for manual bitmap returned NULL\n");
        errors++;
    }
    else
    {
        manual->BytesPerRow = 4;
        manual->Rows = 1;
        manual->Depth = 2;
        manual->Flags = BMF_STANDARD;
        manual->Planes[0] = AllocRaster(32, 1);
        manual->Planes[1] = (PLANEPTR)-1;

        if (!manual->Planes[0])
        {
            print("FAIL: AllocRaster for manual bitmap returned NULL\n");
            errors++;
            FreeMem(manual, sizeof(struct BitMap));
        }
        else
        {
            FreeBitMap(manual);
            print("OK: FreeBitMap() ignored special plane pointers safely\n");
        }
    }

    FreeBitMap(NULL);
    print("OK: FreeBitMap(NULL) is safe\n");

    if (errors == 0)
    {
        print("PASS: AllocBitMap/FreeBitMap all tests passed\n");
        return 0;
    }

    print("FAIL: AllocBitMap/FreeBitMap had errors\n");
    return 20;
}
