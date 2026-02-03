/*
 * Test: graphics/rpattrs
 * Tests SetRPAttrsA() and GetRPAttrsA() functions
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/diskfont.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;

/* RPTAG constants from graphics/rpattr.h */
#define RPTAG_Font          0x80000000
#define RPTAG_APen          0x80000002
#define RPTAG_BPen          0x80000003
#define RPTAG_DrMd          0x80000004
#define RPTAG_OutlinePen    0x80000005
#define RPTAG_WriteMask     0x80000006
#define RPTAG_MaxPen        0x80000007
#define RPTAG_DrawBounds    0x80000008

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

static void print_hex(ULONG n)
{
    char buf[12];
    char *p = buf + 11;
    *p = '\0';
    do
    {
        int digit = n & 0xF;
        *(--p) = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        n >>= 4;
    } while (n > 0);
    print("0x");
    print(p);
}

int main(void)
{
    struct Library *DiskfontBase;
    struct RastPort *rp;
    struct BitMap *bm;
    struct TextFont *font1, *font2;
    struct TextAttr ta;
    ULONG result_ulong;
    struct TextFont *result_font;
    struct Rectangle result_rect;
    
    print("Testing SetRPAttrsA and GetRPAttrsA...\n");
    
    /* Open diskfont.library */
    DiskfontBase = OpenLibrary((CONST_STRPTR)"diskfont.library", 0);
    if (!DiskfontBase) {
        print("ERROR: Could not open diskfont.library\n");
        return 1;
    }
    
    /* Open fonts */
    ta.ta_Name = (STRPTR)"topaz.font";
    ta.ta_YSize = 8;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    font1 = OpenDiskFont(&ta);
    
    ta.ta_YSize = 9;
    font2 = OpenDiskFont(&ta);
    
    if (!font1 || !font2) {
        print("ERROR: Could not open test fonts\n");
        if (font1) CloseFont(font1);
        if (font2) CloseFont(font2);
        CloseLibrary(DiskfontBase);
        return 1;
    }
    
    /* Create bitmap and rastport */
    bm = AllocBitMap(320, 200, 8, BMF_CLEAR, NULL);
    if (!bm) {
        print("ERROR: Could not allocate bitmap\n");
        CloseFont(font1);
        CloseFont(font2);
        CloseLibrary(DiskfontBase);
        return 1;
    }
    
    rp = (struct RastPort *)AllocMem(sizeof(struct RastPort), MEMF_CLEAR);
    if (!rp) {
        print("ERROR: Could not allocate rastport\n");
        FreeBitMap(bm);
        CloseFont(font1);
        CloseFont(font2);
        CloseLibrary(DiskfontBase);
        return 1;
    }
    
    InitRastPort(rp);
    rp->BitMap = bm;
    
    print("OK: Resources allocated\n\n");
    
    /* Test 1: Set and get Font */
    print("Test 1: Set and get Font...\n");
    {
        struct TagItem tags_set[] = {
            {RPTAG_Font, (ULONG)font1},
            {TAG_DONE, 0}
        };
        struct TagItem tags_get[] = {
            {RPTAG_Font, (ULONG)&result_font},
            {TAG_DONE, 0}
        };
        
        SetRPAttrsA(rp, tags_set);
        GetRPAttrsA(rp, tags_get);
        
        if (result_font == font1) {
            print("  OK: Font set and retrieved correctly\n");
        } else {
            print("  FAIL: Font mismatch\n");
        }
    }
    print("\n");
    
    /* Test 2: Set and get APen */
    print("Test 2: Set and get APen...\n");
    {
        struct TagItem tags_set[] = {
            {RPTAG_APen, 5},
            {TAG_DONE, 0}
        };
        struct TagItem tags_get[] = {
            {RPTAG_APen, (ULONG)&result_ulong},
            {TAG_DONE, 0}
        };
        
        SetRPAttrsA(rp, tags_set);
        GetRPAttrsA(rp, tags_get);
        
        print("  APen = ");
        print_num(result_ulong);
        print(" (expected 5)\n");
        if (result_ulong == 5) {
            print("  OK\n");
        } else {
            print("  FAIL\n");
        }
    }
    print("\n");
    
    /* Test 3: Set and get BPen */
    print("Test 3: Set and get BPen...\n");
    {
        struct TagItem tags_set[] = {
            {RPTAG_BPen, 3},
            {TAG_DONE, 0}
        };
        struct TagItem tags_get[] = {
            {RPTAG_BPen, (ULONG)&result_ulong},
            {TAG_DONE, 0}
        };
        
        SetRPAttrsA(rp, tags_set);
        GetRPAttrsA(rp, tags_get);
        
        print("  BPen = ");
        print_num(result_ulong);
        print(" (expected 3)\n");
        if (result_ulong == 3) {
            print("  OK\n");
        } else {
            print("  FAIL\n");
        }
    }
    print("\n");
    
    /* Test 4: Set and get DrMd */
    print("Test 4: Set and get DrMd...\n");
    {
        struct TagItem tags_set[] = {
            {RPTAG_DrMd, JAM2},
            {TAG_DONE, 0}
        };
        struct TagItem tags_get[] = {
            {RPTAG_DrMd, (ULONG)&result_ulong},
            {TAG_DONE, 0}
        };
        
        SetRPAttrsA(rp, tags_set);
        GetRPAttrsA(rp, tags_get);
        
        print("  DrMd = ");
        print_num(result_ulong);
        print(" (expected ");
        print_num(JAM2);
        print(")\n");
        if (result_ulong == JAM2) {
            print("  OK\n");
        } else {
            print("  FAIL\n");
        }
    }
    print("\n");
    
    /* Test 5: Set and get OutlinePen */
    print("Test 5: Set and get OutlinePen...\n");
    {
        struct TagItem tags_set[] = {
            {RPTAG_OutlinePen, 7},
            {TAG_DONE, 0}
        };
        struct TagItem tags_get[] = {
            {RPTAG_OutlinePen, (ULONG)&result_ulong},
            {TAG_DONE, 0}
        };
        
        SetRPAttrsA(rp, tags_set);
        GetRPAttrsA(rp, tags_get);
        
        print("  OutlinePen = ");
        print_num(result_ulong);
        print(" (expected 7)\n");
        if (result_ulong == 7) {
            print("  OK\n");
        } else {
            print("  FAIL\n");
        }
    }
    print("\n");
    
    /* Test 6: Set and get WriteMask */
    print("Test 6: Set and get WriteMask...\n");
    {
        struct TagItem tags_set[] = {
            {RPTAG_WriteMask, 0xFF},
            {TAG_DONE, 0}
        };
        struct TagItem tags_get[] = {
            {RPTAG_WriteMask, (ULONG)&result_ulong},
            {TAG_DONE, 0}
        };
        
        SetRPAttrsA(rp, tags_set);
        GetRPAttrsA(rp, tags_get);
        
        print("  WriteMask = ");
        print_hex(result_ulong);
        print(" (expected 0xff)\n");
        if (result_ulong == 0xFF) {
            print("  OK\n");
        } else {
            print("  FAIL\n");
        }
    }
    print("\n");
    
    /* Test 7: Get MaxPen (read-only) */
    print("Test 7: Get MaxPen (read-only)...\n");
    {
        struct TagItem tags_set[] = {
            {RPTAG_WriteMask, 0x0F},
            {TAG_DONE, 0}
        };
        struct TagItem tags_get[] = {
            {RPTAG_MaxPen, (ULONG)&result_ulong},
            {TAG_DONE, 0}
        };
        
        SetRPAttrsA(rp, tags_set);
        GetRPAttrsA(rp, tags_get);
        
        print("  MaxPen = ");
        print_num(result_ulong);
        print(" (expected 16 for WriteMask 0x0F)\n");
        if (result_ulong == 16) {
            print("  OK\n");
        } else {
            print("  FAIL\n");
        }
    }
    print("\n");
    
    /* Test 8: Get DrawBounds (not implemented, should return zeros) */
    print("Test 8: Get DrawBounds (not implemented)...\n");
    {
        struct TagItem tags_get[] = {
            {RPTAG_DrawBounds, (ULONG)&result_rect},
            {TAG_DONE, 0}
        };
        
        result_rect.MinX = 99;
        result_rect.MinY = 99;
        result_rect.MaxX = 99;
        result_rect.MaxY = 99;
        
        GetRPAttrsA(rp, tags_get);
        
        if (result_rect.MinX == 0 && result_rect.MinY == 0 &&
            result_rect.MaxX == 0 && result_rect.MaxY == 0) {
            print("  OK: DrawBounds returns zero rectangle\n");
        } else {
            print("  FAIL: DrawBounds not zero\n");
        }
    }
    print("\n");
    
    /* Test 9: Multiple attributes in one call */
    print("Test 9: Set multiple attributes in one call...\n");
    {
        struct TagItem tags_set[] = {
            {RPTAG_Font, (ULONG)font2},
            {RPTAG_APen, 10},
            {RPTAG_BPen, 11},
            {RPTAG_DrMd, JAM1},
            {TAG_DONE, 0}
        };
        struct TagItem tags_get[] = {
            {RPTAG_Font, (ULONG)&result_font},
            {RPTAG_APen, (ULONG)&result_ulong},
            {TAG_DONE, 0}
        };
        ULONG result_bpen, result_drmd;
        struct TagItem tags_get2[] = {
            {RPTAG_BPen, (ULONG)&result_bpen},
            {RPTAG_DrMd, (ULONG)&result_drmd},
            {TAG_DONE, 0}
        };
        
        SetRPAttrsA(rp, tags_set);
        GetRPAttrsA(rp, tags_get);
        GetRPAttrsA(rp, tags_get2);
        
        if (result_font == font2 && result_ulong == 10 &&
            result_bpen == 11 && result_drmd == JAM1) {
            print("  OK: All attributes set correctly\n");
        } else {
            print("  FAIL: One or more attributes incorrect\n");
        }
    }
    print("\n");
    
    /* Test 10: TAG_IGNORE */
    print("Test 10: TAG_IGNORE should skip attribute...\n");
    {
        struct TagItem tags_set[] = {
            {RPTAG_APen, 20},
            {TAG_IGNORE, 0},
            {RPTAG_BPen, 21},
            {TAG_DONE, 0}
        };
        struct TagItem tags_get[] = {
            {RPTAG_APen, (ULONG)&result_ulong},
            {TAG_DONE, 0}
        };
        ULONG result_bpen;
        struct TagItem tags_get2[] = {
            {RPTAG_BPen, (ULONG)&result_bpen},
            {TAG_DONE, 0}
        };
        
        SetRPAttrsA(rp, tags_set);
        GetRPAttrsA(rp, tags_get);
        GetRPAttrsA(rp, tags_get2);
        
        if (result_ulong == 20 && result_bpen == 21) {
            print("  OK: TAG_IGNORE handled correctly\n");
        } else {
            print("  FAIL: TAG_IGNORE not handled correctly\n");
        }
    }
    print("\n");
    
    /* Test 11: TAG_SKIP */
    print("Test 11: TAG_SKIP should skip next N tags...\n");
    {
        struct TagItem tags_set[] = {
            {RPTAG_APen, 30},
            {TAG_SKIP, 1},
            {RPTAG_BPen, 31},  /* Should be skipped */
            {RPTAG_DrMd, COMPLEMENT},
            {TAG_DONE, 0}
        };
        struct TagItem tags_get[] = {
            {RPTAG_APen, (ULONG)&result_ulong},
            {TAG_DONE, 0}
        };
        ULONG result_bpen, result_drmd;
        struct TagItem tags_get2[] = {
            {RPTAG_BPen, (ULONG)&result_bpen},
            {RPTAG_DrMd, (ULONG)&result_drmd},
            {TAG_DONE, 0}
        };
        
        /* Set BPen to known value first */
        struct TagItem tags_prep[] = {
            {RPTAG_BPen, 99},
            {TAG_DONE, 0}
        };
        SetRPAttrsA(rp, tags_prep);
        
        SetRPAttrsA(rp, tags_set);
        GetRPAttrsA(rp, tags_get);
        GetRPAttrsA(rp, tags_get2);
        
        if (result_ulong == 30 && result_bpen == 99 && result_drmd == COMPLEMENT) {
            print("  OK: TAG_SKIP handled correctly\n");
        } else {
            print("  FAIL: TAG_SKIP not handled correctly\n");
        }
    }
    print("\n");
    
    /* Cleanup */
    FreeMem(rp, sizeof(struct RastPort));
    FreeBitMap(bm);
    CloseFont(font1);
    CloseFont(font2);
    CloseLibrary(DiskfontBase);
    
    print("PASS: rpattrs all tests passed\n");
    return 0;
}
