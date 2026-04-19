/*
 * Test: graphics/hw_blitter
 * Tests hardware blitter emulation by writing directly to custom chip registers.
 *
 * This tests the software blitter emulation in lxa that handles direct
 * hardware programming of the Amiga blitter (custom chip registers at
 * 0xDFF040-0xDFF074).  Apps like Cluster2 use this instead of the OS
 * BltBitMap() API.
 *
 * Tests:
 *   1. Simple rectangle fill (D-only, minterm 0xFF = set all bits)
 *   2. Copy blit (A->D, minterm 0xF0 = A_TO_D)
 *   3. Cookie-cut blit (A mask + B source -> D, minterm 0xCA)
 *   4. Rectangle clear (minterm 0x00 = clear all bits)
 *   5. Channel C read-modify-write (A & C -> D, minterm 0xA0)
 *   6. Modulo handling (blit within larger bitmap)
 *   7. First/last word masks (BLTAFWM/BLTALWM)
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
#include <hardware/custom.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;

/* Direct hardware custom chip access */
#define CUSTOM_BASE ((volatile struct Custom *)0xDFF000)

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
    char *p = buf + 10;
    *p = '\0';
    do
    {
        int d = n & 0xf;
        *(--p) = (d < 10) ? ('0' + d) : ('a' + d - 10);
        n >>= 4;
    } while (n > 0);
    *(--p) = 'x';
    *(--p) = '0';
    print(p);
}

/*
 * wait_blit() - Wait for blitter to finish.
 * On real hardware, checks DMACONR bit 14 (BBUSY).
 * In lxa, blitter operations are synchronous so this is a no-op,
 * but we call it for correctness.
 */
static void wait_blit(void)
{
    /* Use the OS WaitBlit() for correctness */
    WaitBlit();
}

int main(void)
{
    volatile struct Custom *custom = CUSTOM_BASE;
    UBYTE *buf1, *buf2, *buf3;
    int errors = 0;
    int i;

    print("Testing hardware blitter emulation...\n");

    /* Allocate chip memory buffers for blitter operations */
    /* 32 pixels wide = 2 words = 4 bytes per row, 8 rows = 32 bytes */
    buf1 = (UBYTE *)AllocMem(64, MEMF_CHIP | MEMF_CLEAR);
    buf2 = (UBYTE *)AllocMem(64, MEMF_CHIP | MEMF_CLEAR);
    buf3 = (UBYTE *)AllocMem(64, MEMF_CHIP | MEMF_CLEAR);

    if (!buf1 || !buf2 || !buf3)
    {
        print("FAIL: Could not allocate chip memory\n");
        if (buf1) FreeMem(buf1, 64);
        if (buf2) FreeMem(buf2, 64);
        if (buf3) FreeMem(buf3, 64);
        return 20;
    }

    OwnBlitter();

    /* ================================================================ */
    /* Test 1: Rectangle fill - set all bits in destination              */
    /* ================================================================ */
    print("Test 1: Rectangle fill (set all bits)...\n");

    /* Clear destination */
    for (i = 0; i < 32; i++) buf1[i] = 0x00;

    wait_blit();

    /* Setup: no DMA sources (A not from DMA), use D as destination.
     * We use ADAT=0xFFFF (constant) with minterm 0xF0 (D=A) */
    custom->bltcon0 = 0x01F0;       /* DEST enabled, minterm=0xF0 (D=A) */
    custom->bltcon1 = 0x0000;       /* Ascending, no fill */
    custom->bltafwm = 0xFFFF;       /* Full first word mask */
    custom->bltalwm = 0xFFFF;       /* Full last word mask */
    custom->bltadat = 0xFFFF;       /* A data = all ones (constant) */
    custom->bltdpt  = (APTR)buf1;   /* Destination pointer */
    custom->bltdmod = 0;            /* No modulo (contiguous) */
    /* Blit: 8 rows, 2 words wide = (8 << 6) | 2 = 0x0202 */
    custom->bltsize = (8 << 6) | 2;

    wait_blit();

    /* Verify: all 32 bytes should be 0xFF (16 bytes = 8 rows * 2 words * 2 bytes) */
    {
        int ok = 1;
        for (i = 0; i < 16; i++)
        {
            if (buf1[i] != 0xFF)
            {
                print("FAIL: buf1[");
                print_num(i);
                print("] = ");
                print_hex(buf1[i]);
                print(" expected 0xFF\n");
                ok = 0;
                errors++;
                break;
            }
        }
        if (ok) print("OK: Rectangle fill works\n");
    }

    /* ================================================================ */
    /* Test 2: Copy blit - A->D (minterm 0xF0)                          */
    /* ================================================================ */
    print("Test 2: Copy blit (A->D)...\n");

    /* Setup source with a pattern */
    for (i = 0; i < 32; i++) buf1[i] = 0x00;
    for (i = 0; i < 32; i++) buf2[i] = 0x00;
    /* Set alternating pattern in source (buf1): 0xAA55 per word */
    for (i = 0; i < 16; i += 2)
    {
        buf1[i]     = 0xAA;
        buf1[i + 1] = 0x55;
    }

    wait_blit();

    custom->bltcon0 = 0x09F0;       /* SRCA + DEST, minterm=0xF0 (D=A) */
    custom->bltcon1 = 0x0000;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltapt  = (APTR)buf1;   /* Source A */
    custom->bltdpt  = (APTR)buf2;   /* Destination D */
    custom->bltamod = 0;
    custom->bltdmod = 0;
    /* 4 rows, 2 words wide */
    custom->bltsize = (4 << 6) | 2;

    wait_blit();

    /* Verify: buf2 should match buf1's pattern for first 4 rows (16 bytes) */
    {
        int ok = 1;
        for (i = 0; i < 16; i++)
        {
            if (buf2[i] != buf1[i])
            {
                print("FAIL: buf2[");
                print_num(i);
                print("] = ");
                print_hex(buf2[i]);
                print(" expected ");
                print_hex(buf1[i]);
                print("\n");
                ok = 0;
                errors++;
                break;
            }
        }
        if (ok) print("OK: Copy blit works\n");
    }

    /* ================================================================ */
    /* Test 3: Cookie-cut (A mask, B source, C dest -> D)               */
    /*   minterm 0xCA = (A & B) | (!A & C)                              */
    /*   Where A mask=1: take B; where A mask=0: keep C                 */
    /* ================================================================ */
    print("Test 3: Cookie-cut blit...\n");

    /* A (mask): 0xFF00 per word = left half masked */
    for (i = 0; i < 32; i++) buf1[i] = 0x00;
    for (i = 0; i < 8; i += 2)
    {
        buf1[i]     = 0xFF;
        buf1[i + 1] = 0x00;
    }
    /* B (source): 0xAAAA */
    for (i = 0; i < 32; i++) buf2[i] = 0x00;
    for (i = 0; i < 8; i += 2)
    {
        buf2[i]     = 0xAA;
        buf2[i + 1] = 0xAA;
    }
    /* C/D (dest): 0x5555 */
    for (i = 0; i < 32; i++) buf3[i] = 0x00;
    for (i = 0; i < 8; i += 2)
    {
        buf3[i]     = 0x55;
        buf3[i + 1] = 0x55;
    }

    wait_blit();

    custom->bltcon0 = 0x0FCA;       /* SRCA + SRCB + SRCC + DEST, minterm=0xCA */
    custom->bltcon1 = 0x0000;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltapt  = (APTR)buf1;   /* A = mask */
    custom->bltbpt  = (APTR)buf2;   /* B = source */
    custom->bltcpt  = (APTR)buf3;   /* C = dest (read) */
    custom->bltdpt  = (APTR)buf3;   /* D = dest (write) */
    custom->bltamod = 0;
    custom->bltbmod = 0;
    custom->bltcmod = 0;
    custom->bltdmod = 0;
    /* 2 rows, 2 words wide (just word 0 of each row) */
    custom->bltsize = (2 << 6) | 2;

    wait_blit();

    /* Expected result:
     * Word 0: A=0xFF00, B=0xAAAA, C=0x5555
     *   where A=1 (bits 15:8): take B = 0xAA
     *   where A=0 (bits 7:0): keep C = 0x55
     *   result = 0xAA55
     * Word 1: A=same pattern again (next word in row)
     *   A=0xFF00, B=0xAAAA, C=0x5555 -> 0xAA55
     */
    {
        int ok = 1;
        if (buf3[0] != 0xAA || buf3[1] != 0x55)
        {
            print("FAIL: Cookie-cut word 0: got ");
            print_hex(buf3[0]);
            print_hex(buf3[1]);
            print(" expected 0xAA55\n");
            ok = 0;
            errors++;
        }
        if (ok) print("OK: Cookie-cut blit works\n");
    }

    /* ================================================================ */
    /* Test 4: Rectangle clear (minterm 0x00 = clear all)               */
    /* ================================================================ */
    print("Test 4: Rectangle clear...\n");

    /* Fill buf1 with non-zero data first */
    for (i = 0; i < 32; i++) buf1[i] = 0xFF;

    wait_blit();

    custom->bltcon0 = 0x0100;       /* DEST only, minterm=0x00 (D=0) */
    custom->bltcon1 = 0x0000;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltadat = 0x0000;       /* Not used but set for safety */
    custom->bltdpt  = (APTR)buf1;
    custom->bltdmod = 0;
    custom->bltsize = (4 << 6) | 2;

    wait_blit();

    /* Verify: first 16 bytes should be 0x00 */
    {
        int ok = 1;
        for (i = 0; i < 16; i++)
        {
            if (buf1[i] != 0x00)
            {
                print("FAIL: Clear buf1[");
                print_num(i);
                print("] = ");
                print_hex(buf1[i]);
                print(" expected 0x00\n");
                ok = 0;
                errors++;
                break;
            }
        }
        if (ok) print("OK: Rectangle clear works\n");
    }

    /* ================================================================ */
    /* Test 5: Read-modify-write (D = A & C, minterm 0xA0)              */
    /* ================================================================ */
    print("Test 5: Read-modify-write (A & C -> D)...\n");

    /* Setup: A (constant via ADAT) = 0xF0F0, C/D (buf1) = 0xFF00 */
    for (i = 0; i < 16; i += 2)
    {
        buf1[i]     = 0xFF;
        buf1[i + 1] = 0x00;
    }

    wait_blit();

    custom->bltcon0 = 0x03A0;       /* SRCC + DEST, minterm=0xA0 (D = A & C) */
    custom->bltcon1 = 0x0000;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltadat = 0xF0F0;       /* A = constant 0xF0F0 */
    custom->bltcpt  = (APTR)buf1;   /* C = source (read) */
    custom->bltdpt  = (APTR)buf1;   /* D = dest (write, same as C) */
    custom->bltcmod = 0;
    custom->bltdmod = 0;
    custom->bltsize = (4 << 6) | 1; /* 4 rows, 1 word wide */

    wait_blit();

    /* Expected: A=0xF0F0 & C=0xFF00 = 0xF000 for each word */
    {
        int ok = 1;
        for (i = 0; i < 8; i += 2)
        {
            if (buf1[i] != 0xF0 || buf1[i + 1] != 0x00)
            {
                print("FAIL: RMW buf1[");
                print_num(i);
                print("] = ");
                print_hex(buf1[i]);
                print_hex(buf1[i + 1]);
                print(" expected 0xF000\n");
                ok = 0;
                errors++;
                break;
            }
        }
        if (ok) print("OK: Read-modify-write works\n");
    }

    /* ================================================================ */
    /* Test 6: Modulo handling                                          */
    /*   Blit 1 word within a 4-word-wide bitmap (modulo=6)             */
    /* ================================================================ */
    print("Test 6: Modulo handling...\n");

    /* Setup: 4 words wide bitmap = 8 bytes per row, 4 rows = 32 bytes */
    for (i = 0; i < 32; i++) buf1[i] = 0x00;
    for (i = 0; i < 32; i++) buf2[i] = 0x00;

    /* Fill source: write 0xBEEF at word offset 0 of each row */
    for (i = 0; i < 4; i++)
    {
        buf1[i * 8]     = 0xBE;
        buf1[i * 8 + 1] = 0xEF;
    }

    wait_blit();

    custom->bltcon0 = 0x09F0;       /* SRCA + DEST, minterm=0xF0 (D=A) */
    custom->bltcon1 = 0x0000;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltapt  = (APTR)buf1;   /* Source: word 0 of each row */
    custom->bltdpt  = (APTR)buf2;   /* Dest: word 0 of each row */
    custom->bltamod = 6;            /* Skip 3 words (6 bytes) to next row */
    custom->bltdmod = 6;
    /* 4 rows, 1 word wide */
    custom->bltsize = (4 << 6) | 1;

    wait_blit();

    /* Verify: buf2 should have 0xBEEF at word 0 of each row */
    {
        int ok = 1;
        for (i = 0; i < 4; i++)
        {
            if (buf2[i * 8] != 0xBE || buf2[i * 8 + 1] != 0xEF)
            {
                print("FAIL: Modulo buf2 row ");
                print_num(i);
                print(": ");
                print_hex(buf2[i * 8]);
                print_hex(buf2[i * 8 + 1]);
                print(" expected 0xBEEF\n");
                ok = 0;
                errors++;
                break;
            }
            /* Other words in row should remain zero */
            if (buf2[i * 8 + 2] != 0x00 || buf2[i * 8 + 3] != 0x00)
            {
                print("FAIL: Modulo leakage at row ");
                print_num(i);
                print("\n");
                ok = 0;
                errors++;
                break;
            }
        }
        if (ok) print("OK: Modulo handling works\n");
    }

    /* ================================================================ */
    /* Test 7: First/last word masks                                    */
    /* ================================================================ */
    print("Test 7: First/last word masks...\n");

    for (i = 0; i < 32; i++) buf1[i] = 0x00;

    wait_blit();

    custom->bltcon0 = 0x01F0;       /* DEST only, minterm=0xF0 (D=A) */
    custom->bltcon1 = 0x0000;
    custom->bltafwm = 0x0FFF;       /* First word: mask off top 4 bits */
    custom->bltalwm = 0xFFF0;       /* Last word: mask off bottom 4 bits */
    custom->bltadat = 0xFFFF;       /* A = all ones */
    custom->bltdpt  = (APTR)buf1;
    custom->bltdmod = 0;
    /* 1 row, 2 words wide */
    custom->bltsize = (1 << 6) | 2;

    wait_blit();

    /* Expected: word 0 = 0x0FFF (AFWM applied), word 1 = 0xFFF0 (ALWM applied) */
    {
        int ok = 1;
        if (buf1[0] != 0x0F || buf1[1] != 0xFF)
        {
            print("FAIL: First word mask: got ");
            print_hex(buf1[0]);
            print_hex(buf1[1]);
            print(" expected 0x0FFF\n");
            ok = 0;
            errors++;
        }
        if (buf1[2] != 0xFF || buf1[3] != 0xF0)
        {
            print("FAIL: Last word mask: got ");
            print_hex(buf1[2]);
            print_hex(buf1[3]);
            print(" expected 0xFFF0\n");
            ok = 0;
            errors++;
        }
        if (ok) print("OK: First/last word masks work\n");
    }

    DisownBlitter();

    /* Free memory */
    FreeMem(buf1, 64);
    FreeMem(buf2, 64);
    FreeMem(buf3, 64);

    /* ================================================================ */
    /* Tests 8-15: Hardware blitter LINE mode (Phase 113)               */
    /*                                                                  */
    /* Uses a 32x32 1-bit-plane bitmap (4 bytes/row × 32 = 128 bytes).  */
    /* Line setup per HRM Ch.6: BLTCON1 bit 0 = LINE, BLTSIZE.height =  */
    /* pixel count, BLTAPT = 4*dy - 2*dx, BLTAMOD = 4*(dy-dx),          */
    /* BLTBMOD = 4*dy. Octant via SUD/SUL/AUL bits.                     */
    /* ================================================================ */

    {
        UBYTE *plane = (UBYTE *)AllocMem(128, MEMF_CHIP | MEMF_CLEAR);
        if (!plane)
        {
            print("FAIL: line tests: could not allocate plane\n");
            errors++;
        }
        else
        {
            const LONG bpr = 4;       /* bytes per row */
            int row, col;

            OwnBlitter();

            /* ---------------------------------------------------------- */
            /* Test 8: Horizontal line (X 0..15, Y 4)                    */
            /* dom=X (SUD=1), AUL=0 (X++), dx=15, dy=0, sign always set */
            /* ---------------------------------------------------------- */
            print("Test 8: LINE horizontal (0,4)->(15,4)...\n");
            for (i = 0; i < 128; i++) plane[i] = 0;
            wait_blit();

            custom->bltadat = 0x8000;       /* HRM: A pixel mask, MSB-shifted */
            custom->bltbdat = 0xFFFF;       /* solid pattern */
            custom->bltafwm = 0xFFFF;
            custom->bltalwm = 0xFFFF;
            custom->bltapt  = (APTR)(LONG)(-30);    /* 4*0 - 2*15 = -30 */
            custom->bltamod = (WORD)(-60);          /* 4*(0-15) */
            custom->bltbmod = 0;                    /* 4*0 */
            custom->bltcmod = bpr;
            custom->bltdmod = bpr;
            custom->bltcpt  = (APTR)(plane + 4 * bpr); /* row 4 word 0 */
            custom->bltdpt  = (APTR)(plane + 4 * bpr);
            /* BLTCON0: ash=0, A+C+D enabled, minterm 0xCA */
            custom->bltcon0 = 0x0BCA;
            /* BLTCON1: SUD=1 (dom=X), SIGN=1 (acc<0 initially), LINE=1 */
            custom->bltcon1 = (1 << 4) | (1 << 6) | 1;
            /* BLTSIZE: width=2 words, height=16 pixels */
            custom->bltsize = (16 << 6) | 2;

            wait_blit();

            {
                int ok = 1;
                /* Row 4: byte 0 = 0xFF, byte 1 = 0xFF */
                if (plane[4 * bpr + 0] != 0xFF || plane[4 * bpr + 1] != 0xFF)
                {
                    print("FAIL: H line row 4: ");
                    print_hex(plane[4 * bpr + 0]);
                    print_hex(plane[4 * bpr + 1]);
                    print(" expected 0xFFFF\n");
                    ok = 0;
                    errors++;
                }
                /* Other rows untouched (sample row 3 and 5) */
                for (col = 0; col < bpr; col++)
                {
                    if (plane[3 * bpr + col] != 0 || plane[5 * bpr + col] != 0)
                    {
                        print("FAIL: H line leakage outside row 4\n");
                        ok = 0;
                        errors++;
                        break;
                    }
                }
                if (ok) print("OK: LINE horizontal works\n");
            }

            /* ---------------------------------------------------------- */
            /* Test 9: Vertical line (X 4, Y 0..7)                       */
            /* dom=Y (SUD=0), AUL=0 (Y++), dy=7, dx=0, sign always set  */
            /* ASH=4 → pixel mask = 0x0800 (bit 11 of word, = bit 3 of  */
            /* byte 0 = byte&0x08).                                      */
            /* ---------------------------------------------------------- */
            print("Test 9: LINE vertical (4,0)->(4,7)...\n");
            for (i = 0; i < 128; i++) plane[i] = 0;
            wait_blit();

            custom->bltadat = 0x8000;
            custom->bltbdat = 0xFFFF;
            custom->bltafwm = 0xFFFF;
            custom->bltalwm = 0xFFFF;
            custom->bltapt  = (APTR)(LONG)(-14);    /* 4*0 - 2*7 */
            custom->bltamod = (WORD)(-28);          /* 4*(0-7) */
            custom->bltbmod = 0;
            custom->bltcmod = bpr;
            custom->bltdmod = bpr;
            custom->bltcpt  = (APTR)plane;
            custom->bltdpt  = (APTR)plane;
            /* ASH = 4 (bits 15:12 of bltcon0) */
            custom->bltcon0 = 0x4BCA;
            /* SUD=0 (dom=Y), SIGN=1, LINE=1 */
            custom->bltcon1 = (1 << 6) | 1;
            custom->bltsize = (8 << 6) | 2;

            wait_blit();

            {
                int ok = 1;
                for (row = 0; row < 8; row++)
                {
                    if (plane[row * bpr + 0] != 0x08 || plane[row * bpr + 1] != 0x00)
                    {
                        print("FAIL: V line row ");
                        print_num(row);
                        print(": ");
                        print_hex(plane[row * bpr + 0]);
                        print_hex(plane[row * bpr + 1]);
                        print(" expected 0x0800\n");
                        ok = 0;
                        errors++;
                        break;
                    }
                }
                /* Row 8 should be untouched */
                if (ok && (plane[8 * bpr] != 0))
                {
                    print("FAIL: V line wrote past row 7\n");
                    ok = 0;
                    errors++;
                }
                if (ok) print("OK: LINE vertical works\n");
            }

            /* ---------------------------------------------------------- */
            /* Test 10: Diagonal +X+Y (octant 0), (0,0)->(7,7)           */
            /* dx=dy=7. acc=4*7-2*7=14 (sign clear). SUD=1 (dom=X),     */
            /* AUL=0 (X++), SUL=0 (Y++ when minor). Each step: !sign →  */
            /* take Y minor + X dominant; acc += amod=0; sign stays.    */
            /* Result: pure diagonal.                                    */
            /* ---------------------------------------------------------- */
            print("Test 10: LINE diagonal +X+Y (0,0)->(7,7)...\n");
            for (i = 0; i < 128; i++) plane[i] = 0;
            wait_blit();

            custom->bltadat = 0x8000;
            custom->bltbdat = 0xFFFF;
            custom->bltafwm = 0xFFFF;
            custom->bltalwm = 0xFFFF;
            custom->bltapt  = (APTR)(LONG)(14);     /* 4*7 - 2*7 */
            custom->bltamod = 0;                    /* 4*(7-7) */
            custom->bltbmod = (WORD)(28);           /* 4*7 */
            custom->bltcmod = bpr;
            custom->bltdmod = bpr;
            custom->bltcpt  = (APTR)plane;
            custom->bltdpt  = (APTR)plane;
            custom->bltcon0 = 0x0BCA;               /* ASH=0 */
            custom->bltcon1 = (1 << 4) | 1;         /* SUD=1, SIGN=0, LINE=1 */
            custom->bltsize = (8 << 6) | 2;

            wait_blit();

            {
                int ok = 1;
                /* Pixel (x,y) → byte (y*bpr + (x>>3)), bit (7 - (x&7)) */
                for (row = 0; row < 8; row++)
                {
                    UBYTE expected = 0x80 >> row;       /* bit (7-row) of byte 0 */
                    if (plane[row * bpr + 0] != expected)
                    {
                        print("FAIL: Diag row ");
                        print_num(row);
                        print(": got ");
                        print_hex(plane[row * bpr + 0]);
                        print(" expected ");
                        print_hex(expected);
                        print("\n");
                        ok = 0;
                        errors++;
                        break;
                    }
                }
                if (ok) print("OK: LINE diagonal +X+Y works\n");
            }

            /* ---------------------------------------------------------- */
            /* Test 11: Octant — shallow +X dominant, +Y minor           */
            /* (0,0)->(7,3). dx=7 dom, dy=3 min.                         */
            /* acc = 4*3 - 2*7 = -2 (sign set initially). SUD=1, AUL=0, */
            /* SUL=0. amod=4*(3-7)=-16, bmod=4*3=12.                    */
            /* Trace expected pixels:                                    */
            /*   pixel 0: (0,0) acc=-2 sign=1 → write, no minor; X++;   */
            /*            acc+=bmod=12 → acc=10, sign=0                  */
            /*   pixel 1: (1,0) sign=0 → no minor was taken yet (we    */
            /*   take minor BEFORE writing in our impl). Let me trace.  */
            /*                                                            */
            /* Per our implementation: at start of each iteration we    */
            /* write THIS pixel using current cpt+ash, then step.       */
            /* Iter 0: write (0,0). step: !sign? sign=1 → no minor;     */
            /*         dom X++; acc+=bmod=12 → acc=10, sign=0           */
            /* Iter 1: write (1,0). step: sign=0 → minor Y++; dom X++; */
            /*         acc+=amod=-16 → acc=-6, sign=1                   */
            /* Iter 2: write (2,1). step: sign=1; X++; acc+=12 → 6, s=0*/
            /* Iter 3: write (3,1). step: Y++; X++; acc+=-16 → -10, s=1*/
            /* Iter 4: write (4,2). step: X++; acc+=12 → 2, s=0         */
            /* Iter 5: write (5,2). step: Y++; X++; acc+=-16 → -14, s=1*/
            /* Iter 6: write (6,3). step: X++; acc+=12 → -2, s=1        */
            /* Iter 7: write (7,3).                                     */
            /* Total 8 pixels. Endpoint (7,3) reached. Good.            */
            /* ---------------------------------------------------------- */
            print("Test 11: LINE shallow +X dom +Y (0,0)->(7,3)...\n");
            for (i = 0; i < 128; i++) plane[i] = 0;
            wait_blit();

            custom->bltadat = 0x8000;
            custom->bltbdat = 0xFFFF;
            custom->bltafwm = 0xFFFF;
            custom->bltalwm = 0xFFFF;
            custom->bltapt  = (APTR)(LONG)(-2);     /* 4*3 - 2*7 */
            custom->bltamod = (WORD)(-16);          /* 4*(3-7) */
            custom->bltbmod = 12;                   /* 4*3 */
            custom->bltcmod = bpr;
            custom->bltdmod = bpr;
            custom->bltcpt  = (APTR)plane;
            custom->bltdpt  = (APTR)plane;
            custom->bltcon0 = 0x0BCA;
            custom->bltcon1 = (1 << 4) | (1 << 6) | 1;  /* SUD=1, SIGN=1, LINE=1 */
            custom->bltsize = (8 << 6) | 2;

            wait_blit();

            {
                /* Expected pixels: (0,0),(1,0),(2,1),(3,1),(4,2),(5,2),(6,3),(7,3) */
                struct { int x; int y; } pix[8] = {
                    {0,0},{1,0},{2,1},{3,1},{4,2},{5,2},{6,3},{7,3}
                };
                UBYTE expected[4] = {0,0,0,0};  /* 4 rows we care about */
                int ok = 1;
                for (i = 0; i < 8; i++)
                    expected[pix[i].y] |= (UBYTE)(0x80 >> pix[i].x);
                for (row = 0; row < 4; row++)
                {
                    if (plane[row * bpr + 0] != expected[row])
                    {
                        print("FAIL: Shallow row ");
                        print_num(row);
                        print(": got ");
                        print_hex(plane[row * bpr + 0]);
                        print(" expected ");
                        print_hex(expected[row]);
                        print("\n");
                        ok = 0;
                        errors++;
                        break;
                    }
                }
                if (ok) print("OK: LINE shallow +X +Y works\n");
            }

            /* ---------------------------------------------------------- */
            /* Test 12: Octant — +X dominant, -Y (going up-right)        */
            /* (0,7)->(7,4). Same shape as test 11 but Y decreases.     */
            /* SUD=1, AUL=0 (X++), SUL=1 (Y-- when minor).              */
            /* dx=7 dom, dy=3 min.                                       */
            /* ---------------------------------------------------------- */
            print("Test 12: LINE +X -Y (0,7)->(7,4)...\n");
            for (i = 0; i < 128; i++) plane[i] = 0;
            wait_blit();

            custom->bltadat = 0x8000;
            custom->bltbdat = 0xFFFF;
            custom->bltafwm = 0xFFFF;
            custom->bltalwm = 0xFFFF;
            custom->bltapt  = (APTR)(LONG)(-2);
            custom->bltamod = (WORD)(-16);
            custom->bltbmod = 12;
            custom->bltcmod = bpr;
            custom->bltdmod = bpr;
            custom->bltcpt  = (APTR)(plane + 7 * bpr);  /* start at row 7 */
            custom->bltdpt  = (APTR)(plane + 7 * bpr);
            custom->bltcon0 = 0x0BCA;
            /* SUD=1, SUL=1 (Y--), AUL=0 (X++), SIGN=1, LINE=1 */
            custom->bltcon1 = (1 << 4) | (1 << 3) | (1 << 6) | 1;
            custom->bltsize = (8 << 6) | 2;

            wait_blit();

            {
                /* Expected: (0,7),(1,7),(2,6),(3,6),(4,5),(5,5),(6,4),(7,4) */
                struct { int x; int y; } pix[8] = {
                    {0,7},{1,7},{2,6},{3,6},{4,5},{5,5},{6,4},{7,4}
                };
                UBYTE expected[8] = {0,0,0,0,0,0,0,0};
                int ok = 1;
                for (i = 0; i < 8; i++)
                    expected[pix[i].y] |= (UBYTE)(0x80 >> pix[i].x);
                for (row = 4; row <= 7; row++)
                {
                    if (plane[row * bpr + 0] != expected[row])
                    {
                        print("FAIL: +X-Y row ");
                        print_num(row);
                        print(": got ");
                        print_hex(plane[row * bpr + 0]);
                        print(" expected ");
                        print_hex(expected[row]);
                        print("\n");
                        ok = 0;
                        errors++;
                        break;
                    }
                }
                if (ok) print("OK: LINE +X -Y works\n");
            }

            /* ---------------------------------------------------------- */
            /* Test 13: Octant — -X dominant, +Y (going down-left)       */
            /* (7,0)->(0,3). dx=7 dom (decreasing), dy=3 min (incr).    */
            /* SUD=1, AUL=1 (X--), SUL=0 (Y++).                         */
            /* ---------------------------------------------------------- */
            print("Test 13: LINE -X +Y (7,0)->(0,3)...\n");
            for (i = 0; i < 128; i++) plane[i] = 0;
            wait_blit();

            custom->bltadat = 0x8000;
            custom->bltbdat = 0xFFFF;
            custom->bltafwm = 0xFFFF;
            custom->bltalwm = 0xFFFF;
            custom->bltapt  = (APTR)(LONG)(-2);
            custom->bltamod = (WORD)(-16);
            custom->bltbmod = 12;
            custom->bltcmod = bpr;
            custom->bltdmod = bpr;
            custom->bltcpt  = (APTR)plane;          /* row 0 */
            custom->bltdpt  = (APTR)plane;
            /* ASH=7 (start at column 7 within word 0) */
            custom->bltcon0 = 0x7BCA;
            /* SUD=1, AUL=1 (X--), SIGN=1, LINE=1 */
            custom->bltcon1 = (1 << 4) | (1 << 2) | (1 << 6) | 1;
            custom->bltsize = (8 << 6) | 2;

            wait_blit();

            {
                /* Expected: (7,0),(6,0),(5,1),(4,1),(3,2),(2,2),(1,3),(0,3) */
                struct { int x; int y; } pix[8] = {
                    {7,0},{6,0},{5,1},{4,1},{3,2},{2,2},{1,3},{0,3}
                };
                UBYTE expected[4] = {0,0,0,0};
                int ok = 1;
                for (i = 0; i < 8; i++)
                    expected[pix[i].y] |= (UBYTE)(0x80 >> pix[i].x);
                for (row = 0; row < 4; row++)
                {
                    if (plane[row * bpr + 0] != expected[row])
                    {
                        print("FAIL: -X+Y row ");
                        print_num(row);
                        print(": got ");
                        print_hex(plane[row * bpr + 0]);
                        print(" expected ");
                        print_hex(expected[row]);
                        print("\n");
                        ok = 0;
                        errors++;
                        break;
                    }
                }
                if (ok) print("OK: LINE -X +Y works\n");
            }

            /* ---------------------------------------------------------- */
            /* Test 14: Steep +Y dominant, +X minor (0,0)->(3,7)         */
            /* dy=7 dom, dx=3 min. SUD=0 (dom=Y), AUL=0 (Y++),          */
            /* SUL=0 (X++ when minor).                                   */
            /* acc = 4*3 - 2*7 = -2, sign=1 initially.                  */
            /* amod = 4*(3-7) = -16, bmod = 4*3 = 12.                   */
            /* Trace: same accumulator pattern as test 11, but axes      */
            /* swapped. Expected pixels:                                 */
            /*   (0,0),(0,1),(1,2),(1,3),(2,4),(2,5),(3,6),(3,7)        */
            /* ---------------------------------------------------------- */
            print("Test 14: LINE steep +Y dom (0,0)->(3,7)...\n");
            for (i = 0; i < 128; i++) plane[i] = 0;
            wait_blit();

            custom->bltadat = 0x8000;
            custom->bltbdat = 0xFFFF;
            custom->bltafwm = 0xFFFF;
            custom->bltalwm = 0xFFFF;
            custom->bltapt  = (APTR)(LONG)(-2);
            custom->bltamod = (WORD)(-16);
            custom->bltbmod = 12;
            custom->bltcmod = bpr;
            custom->bltdmod = bpr;
            custom->bltcpt  = (APTR)plane;
            custom->bltdpt  = (APTR)plane;
            custom->bltcon0 = 0x0BCA;
            /* SUD=0 (dom=Y), SIGN=1, LINE=1 */
            custom->bltcon1 = (1 << 6) | 1;
            custom->bltsize = (8 << 6) | 2;

            wait_blit();

            {
                struct { int x; int y; } pix[8] = {
                    {0,0},{0,1},{1,2},{1,3},{2,4},{2,5},{3,6},{3,7}
                };
                UBYTE expected[8] = {0,0,0,0,0,0,0,0};
                int ok = 1;
                for (i = 0; i < 8; i++)
                    expected[pix[i].y] |= (UBYTE)(0x80 >> pix[i].x);
                for (row = 0; row < 8; row++)
                {
                    if (plane[row * bpr + 0] != expected[row])
                    {
                        print("FAIL: Steep row ");
                        print_num(row);
                        print(": got ");
                        print_hex(plane[row * bpr + 0]);
                        print(" expected ");
                        print_hex(expected[row]);
                        print("\n");
                        ok = 0;
                        errors++;
                        break;
                    }
                }
                if (ok) print("OK: LINE steep +Y dom works\n");
            }

            /* ---------------------------------------------------------- */
            /* Test 15: Cross-word horizontal line (X 12..23, Y=10)      */
            /* Crosses word boundary at X=15->16 to verify ash wrap.    */
            /* dx=11, dy=0. acc=-22, amod=-44, bmod=0. ASH=12.          */
            /* ---------------------------------------------------------- */
            print("Test 15: LINE crosses word boundary (12,10)->(23,10)...\n");
            for (i = 0; i < 128; i++) plane[i] = 0;
            wait_blit();

            custom->bltadat = 0x8000;
            custom->bltbdat = 0xFFFF;
            custom->bltafwm = 0xFFFF;
            custom->bltalwm = 0xFFFF;
            custom->bltapt  = (APTR)(LONG)(-22);
            custom->bltamod = (WORD)(-44);
            custom->bltbmod = 0;
            custom->bltcmod = bpr;
            custom->bltdmod = bpr;
            custom->bltcpt  = (APTR)(plane + 10 * bpr); /* word 0 of row 10 */
            custom->bltdpt  = (APTR)(plane + 10 * bpr);
            /* ASH=12 (bits 15:12 = 0xC) */
            custom->bltcon0 = 0xCBCA;
            custom->bltcon1 = (1 << 4) | (1 << 6) | 1;
            custom->bltsize = (12 << 6) | 2;

            wait_blit();

            {
                int ok = 1;
                /* X 12..15 → byte 1 bits 7..4 set: 0xF0
                   X 16..23 → byte 2 bits 7..0 set: 0xFF */
                if (plane[10 * bpr + 0] != 0x00 ||
                    plane[10 * bpr + 1] != 0x0F ||
                    plane[10 * bpr + 2] != 0xFF ||
                    plane[10 * bpr + 3] != 0x00)
                {
                    print("FAIL: Cross-word: ");
                    print_hex(plane[10 * bpr + 0]);
                    print_hex(plane[10 * bpr + 1]);
                    print_hex(plane[10 * bpr + 2]);
                    print_hex(plane[10 * bpr + 3]);
                    print(" expected 00 0F FF 00\n");
                    ok = 0;
                    errors++;
                }
                if (ok) print("OK: LINE word boundary cross works\n");
            }

            /* ---------------------------------------------------------- */
            /* Test 16: Patterned line (BLTBDAT = 0xAAAA dotted)         */
            /* Horizontal X 0..15, Y=15. Pattern alternates pixels.     */
            /* B is rotated right one bit per pixel; LSB selects whether*/
            /* the pixel is drawn (since b_word=0xFFFF when LSB=1).     */
            /* 0xAAAA = 1010_1010_1010_1010. Bit 0 = 0, bit 1 = 1, ... */
            /* Per-iter we use the LSB of the rotated value, then rotate*/
            /* right by 1. So pixel 0 uses bshift=0 → 0xAAAA → LSB=0   */
            /* (no draw with minterm 0xCA: D = (A&B)|(~A&C). With B=0,  */
            /* A=mask, this becomes D = ~A & C = preserve C only).     */
            /* Result for empty C: pixels at odd X (1,3,5,...,15).     */
            /* ---------------------------------------------------------- */
            print("Test 16: LINE patterned (BLTBDAT=0xAAAA)...\n");
            for (i = 0; i < 128; i++) plane[i] = 0;
            wait_blit();

            custom->bltadat = 0x8000;
            custom->bltbdat = 0xAAAA;
            custom->bltafwm = 0xFFFF;
            custom->bltalwm = 0xFFFF;
            custom->bltapt  = (APTR)(LONG)(-30);
            custom->bltamod = (WORD)(-60);
            custom->bltbmod = 0;
            custom->bltcmod = bpr;
            custom->bltdmod = bpr;
            custom->bltcpt  = (APTR)(plane + 15 * bpr);
            custom->bltdpt  = (APTR)(plane + 15 * bpr);
            custom->bltcon0 = 0x0BCA;
            custom->bltcon1 = (1 << 4) | (1 << 6) | 1;
            custom->bltsize = (16 << 6) | 2;

            wait_blit();

            {
                int ok = 1;
                /* Expected: pixels at X=1,3,5,7,9,11,13,15 → 0x55 0x55 */
                if (plane[15 * bpr + 0] != 0x55 || plane[15 * bpr + 1] != 0x55)
                {
                    print("FAIL: Patterned: ");
                    print_hex(plane[15 * bpr + 0]);
                    print_hex(plane[15 * bpr + 1]);
                    print(" expected 0x5555\n");
                    ok = 0;
                    errors++;
                }
                if (ok) print("OK: LINE patterned works\n");
            }

            DisownBlitter();
            FreeMem(plane, 128);
        }
    }

    /* Final result */
    if (errors == 0)
    {
        print("PASS: hw_blitter all tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: hw_blitter had ");
        print_num(errors);
        print(" errors\n");
        return 20;
    }
}
