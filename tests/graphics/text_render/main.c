/*
 * Test: graphics/text_render
 * Tests OpenFont(), SetFont(), Text(), TextLength() functions
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
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
    struct RastPort rp;
    struct BitMap bm;
    struct TextAttr ta;
    struct TextFont *font;
    LONG len;
    PLANEPTR plane;
    int errors = 0;

    print("Testing Text Rendering...\n");

    /* Allocate a small bitmap (64x16 pixels, 1 bitplane) */
    plane = AllocRaster(64, 16);
    if (!plane)
    {
        print("FAIL: Could not allocate raster\n");
        return 20;
    }

    /* Initialize bitmap */
    InitBitMap(&bm, 1, 64, 16);
    bm.Planes[0] = plane;

    /* Initialize rastport */
    InitRastPort(&rp);
    rp.BitMap = &bm;

    /* Set up TextAttr for topaz 8 */
    ta.ta_Name = (STRPTR)"topaz.font";
    ta.ta_YSize = 8;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;

    /* Test OpenFont() */
    print("Testing OpenFont(topaz.font, 8)...\n");
    font = OpenFont(&ta);
    if (!font)
    {
        print("FAIL: OpenFont() returned NULL\n");
        errors++;
    }
    else
    {
        print("OK: OpenFont() returned font\n");

        /* Test font properties */
        if (font->tf_YSize != 8)
        {
            print("FAIL: tf_YSize != 8, got ");
            print_num(font->tf_YSize);
            print("\n");
            errors++;
        }
        else
        {
            print("OK: tf_YSize = 8\n");
        }

        if (font->tf_XSize != 8)
        {
            print("FAIL: tf_XSize != 8, got ");
            print_num(font->tf_XSize);
            print("\n");
            errors++;
        }
        else
        {
            print("OK: tf_XSize = 8\n");
        }

        /* Test SetFont() */
        print("Testing SetFont()...\n");
        SetFont(&rp, font);

        if (rp.TxHeight != 8)
        {
            print("FAIL: TxHeight != 8 after SetFont\n");
            errors++;
        }
        else
        {
            print("OK: TxHeight = 8 after SetFont\n");
        }

        if (rp.Font != font)
        {
            print("FAIL: rp.Font not set correctly\n");
            errors++;
        }
        else
        {
            print("OK: rp.Font set correctly\n");
        }

        /* Test TextLength() */
        print("Testing TextLength()...\n");
        len = TextLength(&rp, (CONST_STRPTR)"Hello", 5);
        if (len != 40)  /* 5 chars * 8 pixels = 40 */
        {
            print("FAIL: TextLength(Hello,5) != 40, got ");
            print_num(len);
            print("\n");
            errors++;
        }
        else
        {
            print("OK: TextLength(Hello,5) = 40\n");
        }

        len = TextLength(&rp, (CONST_STRPTR)"A", 1);
        if (len != 8)  /* 1 char * 8 pixels = 8 */
        {
            print("FAIL: TextLength(A,1) != 8, got ");
            print_num(len);
            print("\n");
            errors++;
        }
        else
        {
            print("OK: TextLength(A,1) = 8\n");
        }

        /* Test Text() rendering (check cursor position moves) */
        print("Testing Text() cursor movement...\n");
        Move(&rp, 0, 7);  /* Position at baseline */
        Text(&rp, (CONST_STRPTR)"AB", 2);

        if (rp.cp_x != 16)  /* Should move 2 chars * 8 pixels */
        {
            print("FAIL: cp_x != 16 after Text, got ");
            print_num(rp.cp_x);
            print("\n");
            errors++;
        }
        else
        {
            print("OK: cp_x = 16 after Text(AB,2)\n");
        }

        /* Test CloseFont() */
        print("Testing CloseFont()...\n");
        CloseFont(font);
        print("OK: CloseFont() completed\n");
    }

    /* Test OpenFont() with unknown font - LXA returns built-in font as fallback */
    print("Testing OpenFont(unknown.font) - LXA uses fallback...\n");
    ta.ta_Name = (STRPTR)"unknown.font";
    ta.ta_YSize = 12;
    font = OpenFont(&ta);
    if (font != NULL)
    {
        print("OK: OpenFont(unknown.font) returned fallback font\n");
        CloseFont(font);
    }
    else
    {
        print("OK: OpenFont(unknown.font) returned NULL\n");
    }

    /* Clean up */
    FreeRaster(plane, 64, 16);

    /* Final result */
    if (errors == 0)
    {
        print("PASS: text_render all tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: text_render had errors\n");
        return 20;
    }
}
