/*
 * lxa_custom.c - Amiga custom chip emulation.
 *
 * Contains: blitter emulation, custom chip register write handler,
 * and DMA/interrupt shadow state storage.
 *
 * Phase 125: extracted from lxa.c.
 */

#include "lxa_internal.h"
#include "lxa_memory.h"

/*
 * Hardware blitter emulation state.
 * Shadows the Amiga custom chip blitter registers.
 * Writing to BLTSIZE (or BLTSIZH for ECS) triggers the blit.
 */
static struct {
    uint16_t con0;       /* BLTCON0: shift A (15:12), DMA enables (11:8), minterm (7:0) */
    uint16_t con1;       /* BLTCON1: shift B (15:12), fill/line/direction flags */
    uint16_t afwm;       /* First word mask for channel A */
    uint16_t alwm;       /* Last word mask for channel A */
    uint32_t cpt;        /* Channel C pointer */
    uint32_t bpt;        /* Channel B pointer */
    uint32_t apt;        /* Channel A pointer */
    uint32_t dpt;        /* Channel D (destination) pointer */
    int16_t  cmod;       /* Channel C modulo (signed) */
    int16_t  bmod;       /* Channel B modulo (signed) */
    int16_t  amod;       /* Channel A modulo (signed) */
    int16_t  dmod;       /* Channel D modulo (signed) */
    uint16_t cdat;       /* Channel C data register */
    uint16_t bdat;       /* Channel B data register */
    uint16_t adat;       /* Channel A data register */
    uint16_t sizv;       /* ECS: vertical size (for BLTSIZH trigger) */
} g_blitter = {0};

/* Phase 31: Helper function to get custom chip register name for logging */
static const char *_custom_reg_name(uint16_t reg) __attribute__((unused));
static const char *_custom_reg_name(uint16_t reg)
{
    switch (reg) {
        case CUSTOM_REG_DMACONR: return "DMACONR";
        case CUSTOM_REG_VPOSR: return "VPOSR";
        case CUSTOM_REG_VHPOSR: return "VHPOSR";
        case CUSTOM_REG_ADKCONR: return "ADKCONR";
        case CUSTOM_REG_POTINP: return "POTINP";
        case CUSTOM_REG_JOY0DAT: return "JOY0DAT";
        case CUSTOM_REG_JOY1DAT: return "JOY1DAT";
        case CUSTOM_REG_DENISEID: return "DENISEID";
        case CUSTOM_REG_BLTCON0: return "BLTCON0";
        case CUSTOM_REG_BLTCON1: return "BLTCON1";
        case CUSTOM_REG_BLTAFWM: return "BLTAFWM";
        case CUSTOM_REG_BLTALWM: return "BLTALWM";
        case CUSTOM_REG_BLTCPTH: return "BLTCPTH";
        case CUSTOM_REG_BLTCPTL: return "BLTCPTL";
        case CUSTOM_REG_BLTBPTH: return "BLTBPTH";
        case CUSTOM_REG_BLTBPTL: return "BLTBPTL";
        case CUSTOM_REG_BLTAPTH: return "BLTAPTH";
        case CUSTOM_REG_BLTAPTL: return "BLTAPTL";
        case CUSTOM_REG_BLTDPTH: return "BLTDPTH";
        case CUSTOM_REG_BLTDPTL: return "BLTDPTL";
        case CUSTOM_REG_BLTSIZE: return "BLTSIZE";
        case CUSTOM_REG_BLTSIZV: return "BLTSIZV";
        case CUSTOM_REG_BLTSIZH: return "BLTSIZH";
        case CUSTOM_REG_BLTCMOD: return "BLTCMOD";
        case CUSTOM_REG_BLTBMOD: return "BLTBMOD";
        case CUSTOM_REG_BLTAMOD: return "BLTAMOD";
        case CUSTOM_REG_BLTDMOD: return "BLTDMOD";
        case CUSTOM_REG_BLTCDAT: return "BLTCDAT";
        case CUSTOM_REG_BLTBDAT: return "BLTBDAT";
        case CUSTOM_REG_BLTADAT: return "BLTADAT";
        case CUSTOM_REG_DMACON: return "DMACON";
        case CUSTOM_REG_INTENA: return "INTENA";
        case CUSTOM_REG_INTREQ: return "INTREQ";
        case CUSTOM_REG_ADKCON: return "ADKCON";
        case CUSTOM_REG_POTGO: return "POTGO";
        case CUSTOM_REG_COP1LC: return "COP1LC";
        case CUSTOM_REG_COP2LC: return "COP2LC";
        case CUSTOM_REG_COPJMP1: return "COPJMP1";
        case CUSTOM_REG_COPJMP2: return "COPJMP2";
        default:
            if (reg >= CUSTOM_REG_COLOR00 && reg < CUSTOM_REG_COLOR00 + 64) {
                return "COLORxx";
            }
            return "UNKNOWN";
    }
}

/*
 * _blitter_execute() - Software emulation of the Amiga hardware blitter.
 *
 * Called when BLTSIZE (OCS) or BLTSIZH (ECS) is written, triggering a blit.
 * Reads source data from channels A/B/C (DMA or data register), applies
 * barrel shift (A/B), first/last word masks (A), minterm logic function,
 * and writes result to channel D.
 *
 * Supports:
 *   - All 256 minterm logic functions
 *   - Channel A/B barrel shift (ASH/BSH, 0-15 bits)
 *   - First/last word masks (BLTAFWM/BLTALWM)
 *   - Per-channel DMA enable/disable (data register fallback)
 *   - Ascending and descending (reverse) blit direction
 *   - Inclusive-OR and exclusive-OR fill modes
 *   - Per-channel signed modulo
 *   - Line draw mode (BLTCON1 bit 0): Bresenham line drawing (Phase 113)
 */
static void _blitter_execute (uint16_t width_words, uint16_t height)
{
    /* Zero means maximum */
    if (width_words == 0) width_words = 64;
    if (height == 0) height = 1024;

    uint16_t con0 = g_blitter.con0;
    uint16_t con1 = g_blitter.con1;
    uint8_t  minterm = con0 & 0xff;
    bool     use_a = (con0 & 0x0800) != 0;
    bool     use_b = (con0 & 0x0400) != 0;
    bool     use_c = (con0 & 0x0200) != 0;
    bool     use_d = (con0 & 0x0100) != 0;
    uint16_t ash   = (con0 >> 12) & 0xf;
    uint16_t bsh   = (con1 >> 12) & 0xf;
    bool     desc  = (con1 & 0x02) != 0;  /* Descending direction */
    bool     fill_or  = (con1 & 0x08) != 0;
    bool     fill_xor = (con1 & 0x10) != 0;
    bool     fill_carry_in = (con1 & 0x04) != 0;

    /* ------------------------------------------------------------------
     * LINE MODE (BLTCON1 bit 0): Bresenham line drawing.
     *
     * Register usage in line mode (per HRM Ch. 6 + Appendix C):
     *   BLTCON0 ASH (bits 15:12)  = starting bit position within first word
     *                                 (15 = leftmost pixel of word).
     *   BLTCON0 minterm (7:0)     = logic op, typically 0xCA = (B & A) | (~B & C)
     *                                 for opaque, 0x4A for transparent overlay.
     *   BLTCON0 channel enables   = A always on, C and D normally on.
     *
     *   BLTCON1 bit 0 = LINE
     *   BLTCON1 bit 1 = SING (ONEDOT — write at most one pixel per row)
     *   BLTCON1 bit 2 = AUL  (direction of dominant-axis step: 0=incr, 1=decr)
     *   BLTCON1 bit 3 = SUL  (direction of minor-axis step: 0=incr, 1=decr)
     *   BLTCON1 bit 4 = SUD  (1: dominant=X minor=Y; 0: dominant=Y minor=X)
     *   BLTCON1 bit 6 = SIGN (initial sign of Bresenham accumulator)
     *
     *   BLTAPT  = Bresenham accumulator: 4*dy - 2*dx (signed 16-bit, sign-extended)
     *   BLTAMOD = 4 * (dy - dx)   (added when sign clear)
     *   BLTBMOD = 4 * dy          (added when sign set)
     *   BLTADAT = pixel mask, normally 0x8000 (single bit at MSB)
     *   BLTBDAT = pattern (texture) — rotated right one bit per pixel
     *   BLTCPT  = BLTDPT = base address of the destination word containing the
     *                       starting pixel
     *   BLTCMOD = BLTDMOD = bytes per row
     *
     *   BLTSIZE: width-words MUST be 2 (one A word + one C/D word per "step"),
     *            height = number of pixels to draw = max(dx,dy) + 1.
     *
     * Per-pixel: read C, compute D = minterm(A,B,C), write D, then advance
     * (cpt, ash) along the chosen octant using the Bresenham accumulator
     * stored in apt; rotate B by one bit for textured lines.
     * ------------------------------------------------------------------ */
    if (con1 & 0x0001)
    {
        bool sing = (con1 & 0x02) != 0;
        bool sud  = (con1 & 0x10) != 0;
        bool sul  = (con1 & 0x08) != 0;
        bool aul  = (con1 & 0x04) != 0;
        bool sign = (con1 & 0x40) != 0;

        uint16_t pixel_mask = g_blitter.adat;       /* normally 0x8000 */
        uint16_t pattern    = g_blitter.bdat;       /* texture pattern */
        uint16_t bshift     = bsh;                  /* current rotation of pattern */

        uint32_t cpt = g_blitter.cpt;
        uint32_t dpt = g_blitter.dpt;
        int32_t  acc = (int16_t)g_blitter.apt;      /* sign-extend low 16 bits */
        int16_t  amod_l = g_blitter.amod;           /* error step when !sign */
        int16_t  bmod_l = g_blitter.bmod;           /* error step when sign */
        int16_t  cmod_l = g_blitter.cmod;           /* bytes per row */
        uint16_t cur_ash = ash;
        bool     onedot_drawn = false;

        DPRINTF (LOG_DEBUG, "lxa: BLITTER LINE: con0=0x%04x con1=0x%04x "
                 "pixels=%d ash=%d bshift=%d sud=%d sul=%d aul=%d sign=%d "
                 "sing=%d apt=0x%08x amod=%d bmod=%d cmod=%d minterm=0x%02x\n",
                 con0, con1, height, ash, bshift, sud, sul, aul, sign, sing,
                 g_blitter.apt, amod_l, bmod_l, cmod_l, minterm);

        if (width_words != 2)
        {
            DPRINTF (LOG_INFO, "lxa: BLITTER LINE: unusual width_words=%d "
                     "(expected 2)\n", width_words);
        }

        /* Number of pixels to draw = height field of BLTSIZE. */
        for (uint16_t step = 0; step < height; step++)
        {
            /* --- Read C (destination word) --- */
            uint16_t c_data = 0;
            if (use_c && cpt < RAM_SIZE - 1)
                c_data = (g_ram[cpt] << 8) | g_ram[cpt + 1];

            /* --- A is a single-bit mask shifted to current pixel column.
             *     pixel_mask is normally 0x8000 (MSB), then shifted right
             *     by cur_ash to land on the pixel inside the word. --- */
            uint16_t a_word = (uint16_t)(pixel_mask >> cur_ash);

            /* --- B is the texture pattern. blineb starts as bdat rotated
             *     right by bshift; per-pixel we rotate one more bit and
             *     replicate the LSB across the whole word (HRM behavior). --- */
            uint16_t b_rot;
            if (bshift == 0)
                b_rot = pattern;
            else
                b_rot = (uint16_t)((pattern >> bshift) |
                                   (pattern << (16 - bshift)));
            uint16_t b_word = (b_rot & 1) ? 0xFFFF : 0x0000;

            /* --- ONEDOT (SING): suppress writes after the first pixel of
             *     a "step group". onedot_drawn is reset whenever Y advances. --- */
            if (sing && onedot_drawn)
                a_word = 0;

            /* --- Minterm logic: bitwise composition of A, B, C --- */
            uint16_t result = 0;
            for (int bit = 15; bit >= 0; bit--)
            {
                uint16_t mask = (uint16_t)(1 << bit);
                int a_bit = (a_word & mask) ? 1 : 0;
                int b_bit = (b_word & mask) ? 1 : 0;
                int c_bit = (c_data & mask) ? 1 : 0;
                int idx = (a_bit << 2) | (b_bit << 1) | c_bit;
                if (minterm & (1 << idx))
                    result |= mask;
            }

            /* --- Write D --- */
            if (use_d && dpt < RAM_SIZE - 1)
            {
                g_ram[dpt]     = (uint8_t)((result >> 8) & 0xff);
                g_ram[dpt + 1] = (uint8_t)(result & 0xff);
            }

            if (a_word != 0)
                onedot_drawn = true;

            /* --- Bresenham step.
             *     Per HRM/UAE: the SUD bit selects which axis steps only on
             *     a sign flip ("minor" axis, when !sign), and which axis
             *     steps every pixel ("dominant" axis). SUL controls the
             *     direction of the minor step; AUL controls the dominant.
             *
             *       SUD = 1: minor = Y (SUL), dominant = X (AUL)
             *       SUD = 0: minor = X (SUL), dominant = Y (AUL)
             *
             *     The error accumulator is updated with amod when !sign
             *     (a minor step was taken) and with bmod when sign is set. */
            if (!sign)
            {
                /* Minor step */
                if (sud)
                {
                    /* Minor = Y */
                    if (sul) { cpt -= cmod_l; dpt -= cmod_l; }
                    else     { cpt += cmod_l; dpt += cmod_l; }
                    onedot_drawn = false;   /* SING resets per Y step */
                }
                else
                {
                    /* Minor = X (advance one pixel column) */
                    if (sul)
                    {
                        if (cur_ash == 0) { cur_ash = 15; cpt -= 2; dpt -= 2; }
                        else              { cur_ash--; }
                    }
                    else
                    {
                        if (cur_ash == 15) { cur_ash = 0; cpt += 2; dpt += 2; }
                        else               { cur_ash++; }
                    }
                }
            }
            /* Dominant step (every pixel) */
            if (sud)
            {
                /* Dominant = X */
                if (aul)
                {
                    if (cur_ash == 0) { cur_ash = 15; cpt -= 2; dpt -= 2; }
                    else              { cur_ash--; }
                }
                else
                {
                    if (cur_ash == 15) { cur_ash = 0; cpt += 2; dpt += 2; }
                    else               { cur_ash++; }
                }
            }
            else
            {
                /* Dominant = Y */
                if (aul) { cpt -= cmod_l; dpt -= cmod_l; }
                else     { cpt += cmod_l; dpt += cmod_l; }
                onedot_drawn = false;
            }

            /* --- Update Bresenham accumulator and sign flag.
             *     amod = 4*(dy - dx) is added when current sign is clear,
             *     bmod = 4*dy is added when sign is set.
             *     Then sign is recomputed from the new accumulator. --- */
            if (!sign)
                acc += amod_l;
            else
                acc += bmod_l;
            sign = (acc < 0);

            /* --- Rotate B pattern by one bit (HRM: rotated right). --- */
            bshift = (bshift - 1) & 15;
        }

        /* --- Write back final state to blitter registers (HRM: pointers
         *     and BLTCON1 reflect the final position after the blit). --- */
        g_blitter.cpt = cpt;
        g_blitter.dpt = dpt;
        g_blitter.apt = (uint32_t)(int32_t)acc;
        g_blitter.con1 = (uint16_t)((con1 & ~0x40) | (sign ? 0x40 : 0));
        g_blitter.con1 = (uint16_t)((g_blitter.con1 & 0x0fff) | (bshift << 12));

        DPRINTF (LOG_DEBUG, "lxa: BLITTER LINE: done (%d pixels)\n", height);
        return;
    }

    uint32_t apt = g_blitter.apt;
    uint32_t bpt = g_blitter.bpt;
    uint32_t cpt = g_blitter.cpt;
    uint32_t dpt = g_blitter.dpt;

    int16_t  amod = g_blitter.amod;
    int16_t  bmod = g_blitter.bmod;
    int16_t  cmod = g_blitter.cmod;
    int16_t  dmod = g_blitter.dmod;

    /* Barrel shifter state: holds previous word for cross-word shifting */
    uint16_t a_prev = 0;
    uint16_t b_prev = 0;

    DPRINTF (LOG_DEBUG, "lxa: BLITTER: con0=0x%04x con1=0x%04x size=%dx%d "
             "A=%d B=%d C=%d D=%d ash=%d bsh=%d desc=%d minterm=0x%02x\n",
             con0, con1, width_words, height,
             use_a, use_b, use_c, use_d, ash, bsh, desc, minterm);

    int ptr_step = desc ? -2 : 2;

    for (uint16_t row = 0; row < height; row++)
    {
        /* Reset barrel shifter at start of each row */
        a_prev = 0;
        b_prev = 0;

        /* Fill mode carry state (per-row) */
        bool fill_carry = fill_carry_in;

        for (uint16_t col = 0; col < width_words; col++)
        {
            /* --- Read source channels --- */

            uint16_t a_data;
            if (use_a)
            {
                if (apt < RAM_SIZE - 1)
                    a_data = (g_ram[apt] << 8) | g_ram[apt + 1];
                else
                    a_data = 0;
                apt += ptr_step;
            }
            else
            {
                a_data = g_blitter.adat;
            }

            uint16_t b_data;
            if (use_b)
            {
                if (bpt < RAM_SIZE - 1)
                    b_data = (g_ram[bpt] << 8) | g_ram[bpt + 1];
                else
                    b_data = 0;
                bpt += ptr_step;
            }
            else
            {
                b_data = g_blitter.bdat;
            }

            uint16_t c_data;
            if (use_c)
            {
                if (cpt < RAM_SIZE - 1)
                    c_data = (g_ram[cpt] << 8) | g_ram[cpt + 1];
                else
                    c_data = 0;
                cpt += ptr_step;
            }
            else
            {
                c_data = g_blitter.cdat;
            }

            /* --- Apply barrel shift to channels A and B --- */

            uint16_t a_shifted;
            if (ash == 0)
            {
                a_shifted = a_data;
            }
            else
            {
                /* Combine previous and current word, shift right by ash bits */
                uint32_t combined = ((uint32_t)a_prev << 16) | a_data;
                a_shifted = (combined >> ash) & 0xffff;
            }
            a_prev = a_data;

            uint16_t b_shifted;
            if (bsh == 0)
            {
                b_shifted = b_data;
            }
            else
            {
                uint32_t combined = ((uint32_t)b_prev << 16) | b_data;
                b_shifted = (combined >> bsh) & 0xffff;
            }
            b_prev = b_data;

            /* --- Apply first/last word masks to channel A --- */

            uint16_t a_masked = a_shifted;
            if (col == 0)
                a_masked &= g_blitter.afwm;
            if (col == width_words - 1)
                a_masked &= g_blitter.alwm;
            /* If only 1 word wide, both masks are applied (handled above) */

            /* --- Evaluate minterm logic function --- */
            /* For each bit: index = (A<<2)|(B<<1)|C, result = (minterm>>index)&1 */

            uint16_t result = 0;
            for (int bit = 15; bit >= 0; bit--)
            {
                uint16_t mask = 1 << bit;
                int a_bit = (a_masked & mask) ? 1 : 0;
                int b_bit = (b_shifted & mask) ? 1 : 0;
                int c_bit = (c_data & mask) ? 1 : 0;
                int idx = (a_bit << 2) | (b_bit << 1) | c_bit;
                if (minterm & (1 << idx))
                    result |= mask;
            }

            /* --- Apply fill mode (if enabled) --- */

            if (fill_or || fill_xor)
            {
                uint16_t filled = 0;
                /*
                 * Fill processes bits right-to-left within each word.
                 * When a set bit is encountered, fill_carry toggles.
                 * While carry is set, bits are filled (OR or XOR).
                 */
                for (int bit = 0; bit <= 15; bit++)
                {
                    uint16_t mask = 1 << bit;
                    bool src_bit = (result & mask) != 0;

                    if (src_bit)
                        fill_carry = !fill_carry;

                    if (fill_or)
                    {
                        if (fill_carry || src_bit)
                            filled |= mask;
                    }
                    else /* fill_xor */
                    {
                        if (fill_carry)
                            filled |= mask;
                    }
                }
                result = filled;
            }

            /* --- Write result to destination --- */

            if (use_d)
            {
                if (dpt < RAM_SIZE - 1)
                {
                    g_ram[dpt]     = (result >> 8) & 0xff;
                    g_ram[dpt + 1] = result & 0xff;
                }
                dpt += ptr_step;
            }
        }

        /* End of row: apply modulo to advance pointers */
        if (use_a) apt += (desc ? -amod : amod);
        if (use_b) bpt += (desc ? -bmod : bmod);
        if (use_c) cpt += (desc ? -cmod : cmod);
        if (use_d) dpt += (desc ? -dmod : dmod);
    }

    /* Update pointer registers with final values (real hardware does this) */
    g_blitter.apt = apt;
    g_blitter.bpt = bpt;
    g_blitter.cpt = cpt;
    g_blitter.dpt = dpt;

    DPRINTF (LOG_DEBUG, "lxa: BLITTER: done (%d words x %d rows = %d words total)\n",
             width_words, height, width_words * height);
}

void _handle_custom_write (uint16_t reg, uint16_t value);
void _handle_custom_write_ext (uint16_t reg, uint16_t value)
{
    _handle_custom_write(reg, value);
}

void _handle_custom_write (uint16_t reg, uint16_t value)
{

    switch (reg)
    {
        case CUSTOM_REG_INTENA:
        {
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: INTENA value=0x%04x\n", value);

            if (value & 0x8000)
            {
                /* Set bits */
                g_intena |= value & 0x7fff;

                /*
                 * When INTENA is re-enabled (especially MASTER bit), check if
                 * a VBlank interrupt is pending and arm it now.  This emulates
                 * real Amiga hardware behavior: when INTENA re-enables and an
                 * interrupt is already pending on Paula, it propagates to the
                 * CPU immediately.
                 *
                 * This fixes a timing issue where lxa_run_cycles sets
                 * g_pending_irq while INTENA MASTER is temporarily off
                 * (e.g. during Disable()/Enable() around Wait()), and the
                 * pending VBlank never gets armed because subsequent checks
                 * also find INTENA disabled at the wrong moment.
                 */
                if ((g_intena & INTENA_MASTER) && (g_intena & INTENA_VBLANK) &&
                    (g_pending_irq & (1 << 3)))
                {
                    g_pending_irq &= ~(1 << 3);
                    m68k_set_irq(0);    /* Musashi edge-trigger: lower first */
                    m68k_set_irq(3);
                }
            }
            else
            {
                /* Clear bits */
                g_intena &= ~(value & 0x7fff);
            }

            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write -> INTENA=0x%04x\n", g_intena);

            break;
        }
        case CUSTOM_REG_DMACON:
        {
            /* DMA control - update shadow register like INTENA (set/clear semantics) */
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: DMACON value=0x%04x\n", value);
            if (value & 0x8000)
            {
                /* Set bits */
                g_dmacon |= value & 0x7fff;
            }
            else
            {
                /* Clear bits */
                g_dmacon &= ~(value & 0x7fff);
            }
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write -> DMACON=0x%04x\n", g_dmacon);
            break;
        }
        /* Phase 31: Additional custom chip registers (Denise, Agnus, Paula) - log and ignore */
        case CUSTOM_REG_INTREQ:
        {
            /* Interrupt request - update shadow register with set/clear semantics */
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: INTREQ value=0x%04x\n", value);
            if (value & 0x8000)
            {
                g_intreq |= value & 0x7fff;
            }
            else
            {
                g_intreq &= ~(value & 0x7fff);
            }
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write -> INTREQ=0x%04x\n", g_intreq);
            break;
        }
        case CUSTOM_REG_COPJMP1:
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: COPJMP1 (strobe)\n");
            copper_jump(1);
            break;
        case CUSTOM_REG_COPJMP2:
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: COPJMP2 (strobe)\n");
            copper_jump(2);
            break;
        case CUSTOM_REG_COP1LC:
            /* Long pointer at 0x080: high word. Low word lives at 0x082. */
            copper_set_cop1lc((copper_get_cop1lc() & 0x0000FFFF) | ((uint32_t)value << 16));
            break;
        case 0x082:
            copper_set_cop1lc((copper_get_cop1lc() & 0xFFFF0000) | value);
            break;
        case CUSTOM_REG_COP2LC:
            copper_set_cop2lc((copper_get_cop2lc() & 0x0000FFFF) | ((uint32_t)value << 16));
            break;
        case 0x086:
            copper_set_cop2lc((copper_get_cop2lc() & 0xFFFF0000) | value);
            break;
        case CUSTOM_REG_COPCON:
            copper_set_copcon(value);
            break;
        case CUSTOM_REG_ADKCON:
        case CUSTOM_REG_POTGO:
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: %s value=0x%04x (ignored)\n",
                    _custom_reg_name(reg), value);
            break;

        /* --- Hardware blitter register writes --- */
        case CUSTOM_REG_BLTCON0:
            g_blitter.con0 = value;
            break;
        case CUSTOM_REG_BLTCON1:
            g_blitter.con1 = value;
            break;
        case CUSTOM_REG_BLTAFWM:
            g_blitter.afwm = value;
            break;
        case CUSTOM_REG_BLTALWM:
            g_blitter.alwm = value;
            break;
        case CUSTOM_REG_BLTCPTH:
            g_blitter.cpt = (g_blitter.cpt & 0x0000ffff) | ((uint32_t)value << 16);
            break;
        case CUSTOM_REG_BLTCPTL:
            g_blitter.cpt = (g_blitter.cpt & 0xffff0000) | value;
            break;
        case CUSTOM_REG_BLTBPTH:
            g_blitter.bpt = (g_blitter.bpt & 0x0000ffff) | ((uint32_t)value << 16);
            break;
        case CUSTOM_REG_BLTBPTL:
            g_blitter.bpt = (g_blitter.bpt & 0xffff0000) | value;
            break;
        case CUSTOM_REG_BLTAPTH:
            g_blitter.apt = (g_blitter.apt & 0x0000ffff) | ((uint32_t)value << 16);
            break;
        case CUSTOM_REG_BLTAPTL:
            g_blitter.apt = (g_blitter.apt & 0xffff0000) | value;
            break;
        case CUSTOM_REG_BLTDPTH:
            g_blitter.dpt = (g_blitter.dpt & 0x0000ffff) | ((uint32_t)value << 16);
            break;
        case CUSTOM_REG_BLTDPTL:
            g_blitter.dpt = (g_blitter.dpt & 0xffff0000) | value;
            break;
        case CUSTOM_REG_BLTSIZE:
        {
            /* OCS: Writing BLTSIZE triggers the blit.
             * Bits 15:6 = height (0=1024), bits 5:0 = width in words (0=64) */
            uint16_t h = (value >> 6) & 0x3ff;
            uint16_t w = value & 0x3f;
            _blitter_execute (w, h);
            break;
        }
        case CUSTOM_REG_BLTSIZV:
            /* ECS: Store vertical size, blit triggered by BLTSIZH */
            g_blitter.sizv = value & 0x7fff;
            break;
        case CUSTOM_REG_BLTSIZH:
        {
            /* ECS: Writing BLTSIZH triggers the blit */
            uint16_t w = value & 0x7ff;
            uint16_t h = g_blitter.sizv;
            _blitter_execute (w, h);
            break;
        }
        case CUSTOM_REG_BLTCMOD:
            g_blitter.cmod = (int16_t)value;
            break;
        case CUSTOM_REG_BLTBMOD:
            g_blitter.bmod = (int16_t)value;
            break;
        case CUSTOM_REG_BLTAMOD:
            g_blitter.amod = (int16_t)value;
            break;
        case CUSTOM_REG_BLTDMOD:
            g_blitter.dmod = (int16_t)value;
            break;
        case CUSTOM_REG_BLTCDAT:
            g_blitter.cdat = value;
            break;
        case CUSTOM_REG_BLTBDAT:
            g_blitter.bdat = value;
            break;
        case CUSTOM_REG_BLTADAT:
            g_blitter.adat = value;
            break;
        default:
            /* Many apps write to custom chip registers - just log and ignore */
            if (reg >= CUSTOM_REG_COLOR00 && reg < CUSTOM_REG_COLOR00 + 64) {
                /* Color register write — store in the shadow palette so the
                 * copper interpreter and host-side queries can observe it.
                 * Each color is one 16-bit register (RGB4: 0x0RGB), stride 2. */
                uint32_t idx = (reg - CUSTOM_REG_COLOR00) >> 1;
                if (idx < 32) {
                    g_color_regs[idx] = value & 0x0FFF;
                }
                DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: COLOR%02u value=0x%04x\n",
                        idx, value);
            } else {
                DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: %s (0x%03x) value=0x%04x (ignored)\n",
                        _custom_reg_name(reg), reg, value);
            }
    }
}
