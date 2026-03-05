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
