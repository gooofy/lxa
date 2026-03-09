#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxnodes.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;

#define GRAPHICS_TFE_MATCHWORD 0xDFE7

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    BOOL neg = FALSE;

    *p = '\0';
    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }

    do
    {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    if (neg)
        *--p = '-';

    print(p);
}

static void fill_bytes(APTR ptr, UBYTE value, ULONG size)
{
    UBYTE *p = (UBYTE *)ptr;
    ULONG i;

    for (i = 0; i < size; i++)
        p[i] = value;
}

static UBYTE get_pixel(CONST struct BitMap *bm, UWORD x, UWORD y)
{
    UWORD byte_offset;
    UBYTE bit_mask;

    if (!bm || !bm->Planes[0])
        return 0;

    byte_offset = y * bm->BytesPerRow + (x >> 3);
    bit_mask = 0x80 >> (x & 7);

    return (bm->Planes[0][byte_offset] & bit_mask) ? 1 : 0;
}

int main(void)
{
    struct BitMap *src = NULL;
    struct BitMap *dest = NULL;
    struct BitScaleArgs bsa;
    struct View view;
    struct ViewPort vp;
    struct ExtendedNode *view_extra = NULL;
    struct ExtendedNode *vp_extra = NULL;
    struct TextFont font;
    struct TextAttr ta;
    struct TextFont *opened;
    struct TextFontExtension *tfe;
    struct MsgPort *orig_extension;
    int errors = 0;

    print("Testing graphics bitmap utilities...\n");

    if (ScalerDiv(0, 1, 1) != 0 ||
        ScalerDiv(3, 1, 2) != 2 ||
        ScalerDiv(7, 3, 4) != 5)
    {
        print("FAIL: ScalerDiv() returned unexpected values\n");
        errors++;
    }
    else
    {
        print("OK: ScalerDiv() handles zero and rounded ratios\n");
    }

    src = AllocBitMap(4, 4, 1, BMF_CLEAR, NULL);
    dest = AllocBitMap(8, 8, 1, BMF_CLEAR, NULL);
    if (!src || !dest)
    {
        print("FAIL: AllocBitMap() for scale test failed\n");
        if (src)
            FreeBitMap(src);
        if (dest)
            FreeBitMap(dest);
        return 20;
    }

    src->Planes[0][0 * src->BytesPerRow] = 0x90;
    src->Planes[0][1 * src->BytesPerRow] = 0x60;
    src->Planes[0][2 * src->BytesPerRow] = 0x60;
    src->Planes[0][3 * src->BytesPerRow] = 0x90;

    bsa.bsa_SrcX = 0;
    bsa.bsa_SrcY = 0;
    bsa.bsa_SrcWidth = 4;
    bsa.bsa_SrcHeight = 4;
    bsa.bsa_XSrcFactor = 1;
    bsa.bsa_YSrcFactor = 1;
    bsa.bsa_DestX = 0;
    bsa.bsa_DestY = 0;
    bsa.bsa_DestWidth = 0;
    bsa.bsa_DestHeight = 0;
    bsa.bsa_XDestFactor = 2;
    bsa.bsa_YDestFactor = 2;
    bsa.bsa_SrcBitMap = src;
    bsa.bsa_DestBitMap = dest;
    bsa.bsa_Flags = 0;
    bsa.bsa_XDDA = 0;
    bsa.bsa_YDDA = 0;
    bsa.bsa_Reserved1 = 0;
    bsa.bsa_Reserved2 = 0;

    BitMapScale(&bsa);
    if (bsa.bsa_DestWidth != 8 || bsa.bsa_DestHeight != 8 ||
        !get_pixel(dest, 0, 0) || !get_pixel(dest, 1, 1) ||
        get_pixel(dest, 2, 0) || !get_pixel(dest, 6, 0) ||
        !get_pixel(dest, 2, 2) || get_pixel(dest, 0, 2) ||
        !get_pixel(dest, 7, 7))
    {
        print("FAIL: BitMapScale() produced wrong nearest-neighbor result\n");
        errors++;
    }
    else
    {
        print("OK: BitMapScale() scales planar bitmaps and reports dest size\n");
    }

    InitView(&view);
    InitVPort(&vp);
    view_extra = (struct ExtendedNode *)GfxNew(VIEW_EXTRA_TYPE);
    vp_extra = (struct ExtendedNode *)GfxNew(VIEWPORT_EXTRA_TYPE);
    if (!view_extra || !vp_extra)
    {
        print("FAIL: GfxNew() returned NULL\n");
        errors++;
    }
    else
    {
        GfxAssociate(&view, view_extra);
        GfxAssociate(&vp, vp_extra);

        if (GfxLookUp(&view) != view_extra || GfxLookUp(&vp) != vp_extra)
        {
            print("FAIL: GfxAssociate()/GfxLookUp() mismatch\n");
            errors++;
        }
        else
        {
            print("OK: GfxNew()/GfxAssociate()/GfxLookUp() round-trip works\n");
        }

        GfxFree(vp_extra);
        if (GfxLookUp(&vp) != NULL)
        {
            print("FAIL: GfxFree() did not remove viewport association\n");
            errors++;
        }
        else
        {
            print("OK: GfxFree() removes associated nodes\n");
        }

        vp_extra = NULL;
        GfxFree(view_extra);
        view_extra = NULL;
    }

    fill_bytes(&font, 0, sizeof(font));
    font.tf_Message.mn_Node.ln_Name = "bitmaputil.font";
    font.tf_YSize = 9;
    font.tf_XSize = 7;
    font.tf_Baseline = 7;
    font.tf_BoldSmear = 1;
    font.tf_Flags = FPF_DESIGNED;
    orig_extension = font.tf_Extension;
    AddFont(&font);
    ta.ta_Name = (STRPTR)"bitmaputil.font";
    ta.ta_YSize = 9;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    opened = OpenFont(&ta);
    if (opened != &font)
    {
        print("FAIL: AddFont()/OpenFont() did not expose public font\n");
        errors++;
    }
    else
    {
        print("OK: AddFont() exposes font to OpenFont()\n");
    }

    if (!ExtendFont(&font, NULL))
    {
        print("FAIL: ExtendFont() returned FALSE\n");
        errors++;
    }
    else
    {
        tfe = (struct TextFontExtension *)font.tf_Extension;
        if (!(font.tf_Style & FSF_TAGGED) || !tfe ||
            tfe->tfe_MatchWord != GRAPHICS_TFE_MATCHWORD ||
            tfe->tfe_BackPtr != &font ||
            tfe->tfe_OrigReplyPort != orig_extension)
        {
            print("FAIL: ExtendFont() did not initialize tf_Extension correctly\n");
            errors++;
        }
        else
        {
            print("OK: ExtendFont() creates and attaches a font extension\n");
        }

        tfe->tfe_Flags0 = TE0F_NOREMFONT;
        RemFont(&font);
        if (font.tf_Flags & FPF_REMOVED)
        {
            print("FAIL: RemFont() ignored TE0F_NOREMFONT\n");
            errors++;
        }
        else
        {
            print("OK: RemFont() honors TE0F_NOREMFONT\n");
        }

        tfe->tfe_Flags0 = 0;
    }

    StripFont(&font);
    if ((font.tf_Style & FSF_TAGGED) || font.tf_Extension != orig_extension)
    {
        print("FAIL: StripFont() did not restore original font state\n");
        errors++;
    }
    else
    {
        print("OK: StripFont() restores the original tf_Extension\n");
    }

    if (opened)
        CloseFont(opened);

    if (OpenFont(&ta) != NULL)
    {
        print("FAIL: CloseFont()/RemFont() left non-ROM font public\n");
        errors++;
    }
    else
    {
        print("OK: CloseFont() removes unaccessed non-ROM fonts from the public list\n");
    }

    if (dest)
        FreeBitMap(dest);
    if (src)
        FreeBitMap(src);

    if (errors == 0)
    {
        print("PASS: graphics/bitmap_utils all tests passed\n");
        return 0;
    }

    print("FAIL: graphics/bitmap_utils had errors\n");
    print("Errors: ");
    print_num(errors);
    print("\n");
    return 20;
}
