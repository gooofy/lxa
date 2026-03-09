#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/view.h>
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

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static int expect_rgb32(const ULONG *table, ULONG r, ULONG g, ULONG b)
{
    return (table[0] == r) && (table[1] == g) && (table[2] == b);
}

int main(void)
{
    struct BitMap *bm = NULL;
    struct ColorMap *cm = NULL;
    struct ViewPort vp;
    struct RasInfo ri;
    ULONG table[6];
    UWORD load4[2];
    ULONG load32[] = {
        (2UL << 16) | 4UL,
        0x11223344UL, 0x55667788UL, 0x99AABBCCUL,
        0xFFEEDDCCUL, 0xCCBBAA99UL, 0x88776655UL,
        0UL
    };
    ULONG pen;
    LONG found;
    int errors = 0;

    print("Testing pens/colors APIs...\n");

    bm = AllocBitMap(32, 8, 3, BMF_CLEAR, NULL);
    cm = GetColorMap(8);
    if (!bm || !cm)
    {
        print("FAIL: setup allocation failed\n");
        if (cm)
            FreeColorMap(cm);
        if (bm)
            FreeBitMap(bm);
        return 20;
    }

    InitVPort(&vp);
    ri.Next = NULL;
    ri.BitMap = bm;
    ri.RxOffset = 0;
    ri.RyOffset = 0;
    vp.RasInfo = &ri;
    vp.ColorMap = cm;

    SetRGB32CM(cm, 2, 0x12345678UL, 0x9ABCDEF0UL, 0xFEDCBA98UL);
    GetRGB32(cm, 2, 1, table);
    if (!expect_rgb32(table, 0x12121212UL, 0x9A9A9A9AUL, 0xFEFEFEFEUL))
    {
        print("FAIL: SetRGB32CM()/GetRGB32() mismatch\n");
        errors++;
    }
    else
    {
        print("OK: SetRGB32CM()/GetRGB32() preserve 8-bit palette precision\n");
    }

    SetRGB4(&vp, 3, 0xF, 0x1, 0x8);
    GetRGB32(cm, 3, 1, table);
    if (!expect_rgb32(table, 0xF0F0F0F0UL, 0x10101010UL, 0x80808080UL))
    {
        print("FAIL: SetRGB4() did not update ViewPort colormap\n");
        errors++;
    }
    else
    {
        print("OK: SetRGB4() updates ViewPort colormap\n");
    }

    SetRGB32(&vp, 3, 0xA1B2C3D4UL, 0x10203040UL, 0x55667788UL);
    GetRGB32(cm, 3, 1, table);
    if (!expect_rgb32(table, 0xA1A1A1A1UL, 0x10101010UL, 0x55555555UL))
    {
        print("FAIL: SetRGB32() did not keep top 8 bits\n");
        errors++;
    }
    else
    {
        print("OK: SetRGB32() updates ViewPort colormap with 32-bit input\n");
    }

    load4[0] = 0x0123;
    load4[1] = 0x0ABC;
    LoadRGB4(&vp, load4, 2);
    GetRGB32(cm, 0, 2, table);
    if (!expect_rgb32(&table[0], 0x10101010UL, 0x20202020UL, 0x30303030UL) ||
        !expect_rgb32(&table[3], 0xA0A0A0A0UL, 0xB0B0B0B0UL, 0xC0C0C0C0UL))
    {
        print("FAIL: LoadRGB4() mismatch\n");
        errors++;
    }
    else
    {
        print("OK: LoadRGB4() loads sequential color registers\n");
    }

    LoadRGB32(&vp, load32);
    GetRGB32(cm, 4, 2, table);
    if (!expect_rgb32(&table[0], 0x11111111UL, 0x55555555UL, 0x99999999UL) ||
        !expect_rgb32(&table[3], 0xFFFFFFFFUL, 0xCCCCCCCCUL, 0x88888888UL))
    {
        print("FAIL: LoadRGB32() mismatch\n");
        errors++;
    }
    else
    {
        print("OK: LoadRGB32() loads packed RGB32 records\n");
    }

    if (AttachPalExtra(cm, &vp) != 0 || cm->PalExtra == NULL || AttachPalExtra(cm, &vp) != 0)
    {
        print("FAIL: AttachPalExtra() did not attach palette state\n");
        errors++;
    }
    else
    {
        print("OK: AttachPalExtra() initializes reusable palette state\n");
    }

    pen = ObtainPen(cm, 4, 0x11111111UL, 0x55555555UL, 0x99999999UL, PENF_NO_SETCOLOR);
    if (pen != 4)
    {
        print("FAIL: ObtainPen() shared specific pen failed\n");
        errors++;
    }
    else
    {
        GetRGB32(cm, 4, 1, table);
        if (!expect_rgb32(table, 0x11111111UL, 0x55555555UL, 0x99999999UL))
        {
            print("FAIL: PENF_NO_SETCOLOR changed shared pen color\n");
            errors++;
        }
        else
        {
            print("OK: ObtainPen() honors PENF_NO_SETCOLOR\n");
        }
    }

    pen = ObtainPen(cm, 4, 0x11111111UL, 0x55555555UL, 0x99999999UL, 0);
    if (pen != 4)
    {
        print("FAIL: ObtainPen() did not reuse shared pen\n");
        errors++;
    }
    else
    {
        print("OK: ObtainPen() reuses matching shared pen\n");
    }

    ReleasePen(cm, 4);
    ReleasePen(cm, 4);

    pen = ObtainPen(cm, 4, 0x00000000UL, 0xFFFFFFFFUL, 0x00000000UL,
                    PENF_EXCLUSIVE | PENF_NO_SETCOLOR);
    if (pen != 4)
    {
        print("FAIL: ObtainPen() exclusive specific pen failed\n");
        errors++;
    }
    else if (ObtainPen(cm, 4, 0x00000000UL, 0xFFFFFFFFUL, 0x00000000UL, 0) != (ULONG)-1)
    {
        print("FAIL: shared obtain succeeded while pen was exclusive\n");
        errors++;
    }
    else
    {
        print("OK: exclusive pens block later shared obtains\n");
    }

    ReleasePen(cm, 4);

    pen = ObtainPen(cm, 4, 0xFF000000UL, 0x00000000UL, 0x00000000UL, 0);
    GetRGB32(cm, 4, 1, table);
    if (pen != 4 || !expect_rgb32(table, 0xFFFFFFFFUL, 0x00000000UL, 0x00000000UL))
    {
        print("FAIL: ObtainPen() specific shared update produced wrong color\n");
        errors++;
    }
    else
    {
        print("OK: ObtainPen() can recolor a newly shared pen\n");
    }

    found = FindColor(cm, 0xFFFFFFFFUL, 0xCCCCCCCCUL, 0x88888888UL, -1);
    if (found != 5)
    {
        print("FAIL: FindColor() did not find exact match\n");
        errors++;
    }
    else
    {
        print("OK: FindColor() returns closest matching pen\n");
    }

    found = ObtainBestPenA(cm, 0xFFFFFFFFUL, 0xCCCCCCCCUL, 0x88888888UL, NULL);
    if (found != 5)
    {
        print("FAIL: ObtainBestPenA() did not reuse best shared pen\n");
        errors++;
    }
    else
    {
        print("OK: ObtainBestPenA() reuses matching shared pen\n");
        ReleasePen(cm, (ULONG)found);
    }

    ReleasePen(cm, 4);
    FreeColorMap(cm);
    FreeBitMap(bm);

    if (errors == 0)
    {
        print("PASS: Pens/colors tests passed\n");
        return 0;
    }

    print("FAIL: Pens/colors tests had errors\n");
    return 20;
}
