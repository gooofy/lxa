/*
 * Test: graphics/pixel_array8
 * Tests ReadPixelLine8, WritePixelLine8, ReadPixelArray8, WritePixelArray8
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

int main(void)
{
    struct RastPort *rp;
    struct RastPort *temprp;
    struct BitMap *bm;
    UBYTE array[64];
    LONG result;
    LONG i;
    int errors = 0;

    print("Testing pixel array functions...\n\n");

    /* Create bitmap: 2 planes for 4 colors (pens 0-3) */
    bm = AllocBitMap(64, 64, 2, BMF_CLEAR, NULL);
    if (!bm)
    {
        print("ERROR: Could not allocate bitmap\n");
        return 1;
    }

    rp = (struct RastPort *)AllocMem(sizeof(struct RastPort), MEMF_CLEAR);
    if (!rp)
    {
        print("ERROR: Could not allocate rastport\n");
        FreeBitMap(bm);
        return 1;
    }

    InitRastPort(rp);
    rp->BitMap = bm;

    /* Allocate temp rastport (required by pixel array functions) */
    temprp = (struct RastPort *)AllocMem(sizeof(struct RastPort), MEMF_CLEAR);
    if (!temprp)
    {
        print("ERROR: Could not allocate temp rastport\n");
        FreeMem(rp, sizeof(struct RastPort));
        FreeBitMap(bm);
        return 1;
    }

    InitRastPort(temprp);
    temprp->BitMap = bm;

    print("OK: Resources allocated\n\n");

    /* Test 1: WritePixelLine8 then ReadPixelLine8 */
    print("Test 1: WritePixelLine8 / ReadPixelLine8 round-trip...\n");
    {
        UBYTE write_buf[16];
        UBYTE read_buf[16];

        /* Write pen values 0,1,2,3,0,1,2,3,... */
        for (i = 0; i < 16; i++)
        {
            write_buf[i] = (UBYTE)(i % 4);
        }

        result = WritePixelLine8(rp, 0, 0, 16, write_buf, temprp);
        if (result < 0)
        {
            print("  FAIL: WritePixelLine8 returned ");
            print_num(result);
            print("\n");
            errors++;
        }
        else
        {
            print("  OK: WritePixelLine8 succeeded\n");
        }

        /* Read them back */
        for (i = 0; i < 16; i++)
        {
            read_buf[i] = 0xFF;
        }
        result = ReadPixelLine8(rp, 0, 0, 16, read_buf, temprp);
        if (result < 0)
        {
            print("  FAIL: ReadPixelLine8 returned ");
            print_num(result);
            print("\n");
            errors++;
        }
        else
        {
            /* Verify values */
            BOOL match = TRUE;
            for (i = 0; i < 16; i++)
            {
                if (read_buf[i] != write_buf[i])
                {
                    match = FALSE;
                    print("  FAIL: Mismatch at pixel ");
                    print_num(i);
                    print(": expected ");
                    print_num(write_buf[i]);
                    print(", got ");
                    print_num(read_buf[i]);
                    print("\n");
                    errors++;
                    break;
                }
            }
            if (match)
            {
                print("  OK: ReadPixelLine8 round-trip matches\n");
            }
        }
    }
    print("\n");

    /* Test 2: WritePixelArray8 then ReadPixelArray8 */
    print("Test 2: WritePixelArray8 / ReadPixelArray8 round-trip...\n");
    {
        UBYTE write_arr[16];  /* 4x4 block */
        UBYTE read_arr[16];

        /* Write a 4x4 block at (10,10)-(13,13) */
        for (i = 0; i < 16; i++)
        {
            write_arr[i] = (UBYTE)((i / 4 + i % 4) % 4);
        }

        SetRast(rp, 0);
        result = WritePixelArray8(rp, 10, 10, 13, 13, write_arr, temprp);
        if (result < 0)
        {
            print("  FAIL: WritePixelArray8 returned ");
            print_num(result);
            print("\n");
            errors++;
        }
        else
        {
            print("  OK: WritePixelArray8 succeeded (");
            print_num(result);
            print(" pixels)\n");
        }

        /* Read back */
        for (i = 0; i < 16; i++)
        {
            read_arr[i] = 0xFF;
        }
        result = ReadPixelArray8(rp, 10, 10, 13, 13, read_arr, temprp);
        if (result < 0)
        {
            print("  FAIL: ReadPixelArray8 returned ");
            print_num(result);
            print("\n");
            errors++;
        }
        else
        {
            BOOL match = TRUE;
            for (i = 0; i < 16; i++)
            {
                if (read_arr[i] != write_arr[i])
                {
                    match = FALSE;
                    print("  FAIL: Mismatch at index ");
                    print_num(i);
                    print(": expected ");
                    print_num(write_arr[i]);
                    print(", got ");
                    print_num(read_arr[i]);
                    print("\n");
                    errors++;
                    break;
                }
            }
            if (match)
            {
                print("  OK: ReadPixelArray8 round-trip matches\n");
            }
        }
    }
    print("\n");

    /* Test 3: Verify WritePixelLine8 sets actual pixels readable by ReadPixel */
    print("Test 3: WritePixelLine8 pixels readable by ReadPixel...\n");
    {
        UBYTE buf[4];
        SetRast(rp, 0);

        buf[0] = 0;
        buf[1] = 1;
        buf[2] = 2;
        buf[3] = 3;

        WritePixelLine8(rp, 0, 5, 4, buf, temprp);

        BOOL ok = TRUE;
        for (i = 0; i < 4; i++)
        {
            ULONG pen = ReadPixel(rp, i, 5);
            if (pen != (ULONG)buf[i])
            {
                ok = FALSE;
                print("  FAIL: ReadPixel(");
                print_num(i);
                print(",5) = ");
                print_num((LONG)pen);
                print(", expected ");
                print_num(buf[i]);
                print("\n");
                errors++;
                break;
            }
        }
        if (ok)
        {
            print("  OK: ReadPixel matches WritePixelLine8 output\n");
        }
    }
    print("\n");

    /* Test 4: ReadPixelLine8 reads pixels set by WritePixel */
    print("Test 4: ReadPixelLine8 reads WritePixel output...\n");
    {
        UBYTE buf[4];
        SetRast(rp, 0);

        SetAPen(rp, 1);
        WritePixel(rp, 0, 8);
        SetAPen(rp, 2);
        WritePixel(rp, 1, 8);
        SetAPen(rp, 3);
        WritePixel(rp, 2, 8);
        SetAPen(rp, 0);
        WritePixel(rp, 3, 8);

        for (i = 0; i < 4; i++)
        {
            buf[i] = 0xFF;
        }
        ReadPixelLine8(rp, 0, 8, 4, buf, temprp);

        if (buf[0] == 1 && buf[1] == 2 && buf[2] == 3 && buf[3] == 0)
        {
            print("  OK: ReadPixelLine8 correctly reads WritePixel output\n");
        }
        else
        {
            print("  FAIL: Got ");
            for (i = 0; i < 4; i++)
            {
                print_num(buf[i]);
                print(" ");
            }
            print("expected 1 2 3 0\n");
            errors++;
        }
    }
    print("\n");

    /* Test 5: ReadPixelArray8 on cleared bitmap returns zeros */
    print("Test 5: ReadPixelArray8 on cleared bitmap...\n");
    {
        UBYTE buf[9];  /* 3x3 */
        SetRast(rp, 0);

        for (i = 0; i < 9; i++)
        {
            buf[i] = 0xFF;
        }
        result = ReadPixelArray8(rp, 20, 20, 22, 22, buf, temprp);

        BOOL all_zero = TRUE;
        for (i = 0; i < 9; i++)
        {
            if (buf[i] != 0)
            {
                all_zero = FALSE;
                break;
            }
        }
        if (all_zero)
        {
            print("  OK: Cleared bitmap reads as all zeros\n");
        }
        else
        {
            print("  FAIL: Expected all zeros, got non-zero values\n");
            errors++;
        }
    }
    print("\n");

    /* Test 6: Single pixel write/read via WritePixelArray8/ReadPixelArray8 */
    print("Test 6: Single pixel via WritePixelArray8...\n");
    {
        UBYTE val;
        SetRast(rp, 0);

        val = 3;
        WritePixelArray8(rp, 30, 30, 30, 30, &val, temprp);

        val = 0xFF;
        ReadPixelArray8(rp, 30, 30, 30, 30, &val, temprp);

        if (val == 3)
        {
            print("  OK: Single pixel round-trip works\n");
        }
        else
        {
            print("  FAIL: Expected 3, got ");
            print_num(val);
            print("\n");
            errors++;
        }
    }
    print("\n");

    /* Cleanup */
    FreeMem(temprp, sizeof(struct RastPort));
    FreeMem(rp, sizeof(struct RastPort));
    FreeBitMap(bm);

    if (errors == 0)
    {
        print("PASS: pixel_array8 all tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: pixel_array8 had ");
        print_num(errors);
        print(" errors\n");
        return 20;
    }
}
