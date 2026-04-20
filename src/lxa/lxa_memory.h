/*
 * lxa_memory.h - Inline memory read/write helpers.
 *
 * Defines mread8() and mwrite8() as static inline so they can be inlined
 * into any translation unit that needs them.  Include this header (after
 * lxa_internal.h) in every file that calls these functions directly.
 *
 * Phase 125: lxa.c decomposition.
 */

#ifndef LXA_MEMORY_H
#define LXA_MEMORY_H

#include "lxa_internal.h"

static inline uint8_t mread8(uint32_t address)
{
    /* Note: Reading from address 0 is valid - it's chip RAM on Amiga.
     * The previous NULL pointer check was removed as it caused false
     * positives with applications that legitimately read from low memory.
     */

    if ((address >= RAM_START) && (address <= RAM_END))
    {
        uint32_t addr = address - RAM_START;
        return g_ram[addr];
    }
    else if ((address >= ROM_START) && (address <= ROM_END))
    {
        uint32_t addr = address - ROM_START;
        return g_rom[addr];
    }
    else if ((address >= EXTROM_START) && (address <= EXTROM_END))
    {
        /* Extended ROM area (A3000/A4000) - return 0 to indicate no extended ROM */
        return 0;
    }
    else if ((address >= 0x00A00000) && (address < CUSTOM_START))
    {
        /* RAM overflow area (10MB - just before custom chips at 0xDFF000) - some apps allocate
         * to end of RAM and overflow into what would be expansion RAM, slow RAM, or CIA areas.
         * Return 0 for reads in this area. */
        DPRINTF (LOG_DEBUG, "lxa: mread8 RAM overflow area 0x%08x -> 0x00\n", address);
        return 0;
    }
    else if ((address >= 0x01000000) && (address <= 0x0FFFFFFF))
    {
        /* 16MB-256MB range - Zorro-III expansion area, return 0xFF (no expansion)
         * This covers addresses 0x01000000-0x0FFFFFFF which are used for:
         * - Zorro-III memory expansion
         * - Fast RAM on accelerator cards
         * Returning 0xFF indicates no memory/expansion present.
         */
        return 0xFF;
    }
    else if ((address >= ZORRO2_AUTOCONFIG_START) && (address <= ZORRO2_AUTOCONFIG_END))
    {
        /* Zorro-II autoconfig space - return 0 (no boards) */
        return 0;
    }
    else if ((address >= SLOWRAM_START) && (address <= SLOWRAM_END))
    {
        /* Slow RAM / Ranger RAM area - return 0 (no expansion memory present) */
        DPRINTF (LOG_DEBUG, "lxa: mread8 Slow RAM area 0x%08x -> 0x00\n", address);
        return 0;
    }
    else if ((address >= RANGER_START) && (address <= RANGER_END))
    {
        /* Ranger/expansion RAM area - return 0 (no expansion memory present) */
        DPRINTF (LOG_DEBUG, "lxa: mread8 Ranger RAM area 0x%08x -> 0x00\n", address);
        return 0;
    }
    else if ((address >= CIA_START) && (address <= CIA_END))
    {
        /* CIA chip area - return sensible defaults
         * 0xFF = all bits high = no buttons pressed, no keys, inactive signals
         */
        DPRINTF (LOG_DEBUG, "lxa: mread8 CIA area 0x%08x -> 0xFF\n", address);
        return 0xff;
    }
    else if ((address >= CUSTOM_START) && (address <= CUSTOM_END))
    {
        /* Phase 31: Custom chip area reads (Denise, Agnus, Paula) */
        uint16_t reg = address - CUSTOM_START;
        uint8_t result = 0;
        
        /* Return sensible defaults for common read registers */
        switch (reg & ~1) {  /* Use even address for word-aligned registers */
            case CUSTOM_REG_VPOSR:    /* Vertical position - return 0 (line 0, PAL long frame) */
                result = (reg & 1) ? 0x00 : 0x00;
                break;
            case CUSTOM_REG_VHPOSR:   /* Horiz/vert position - return 0 */
                result = 0;
                break;
            case CUSTOM_REG_JOY0DAT:  /* Joystick 0 - no movement */
                result = 0;
                break;
            case CUSTOM_REG_JOY1DAT:  /* Joystick 1 - no movement */
                result = 0;
                break;
            case CUSTOM_REG_DMACONR:  /* DMA control read - return shadow register */
                /* Bit 14 (BLTBUSY) is always 0 since our blitter executes synchronously */
                result = (reg & 1) ? (g_dmacon & 0xFF) : ((g_dmacon >> 8) & 0xFF);
                break;
            case CUSTOM_REG_DENISEID: /* Denise ID - return 0xFC for ECS Denise */
                result = (reg & 1) ? 0xFC : 0x00;
                break;
            case CUSTOM_REG_POTINP:   /* Pot port read - return 0xFF (no pots) */
                result = 0xFF;
                break;
            case CUSTOM_REG_SERDATR:  /* Serial port data and status read */
                /* Bit 13 (TBE) = Transmit Buffer Empty - always set (ready to transmit)
                 * Bit 12 (TSRE) = Transmit Shift Reg Empty - always set
                 * Bit 14 (RBF) = Receive Buffer Full - always 0 (no data received)
                 * Bit 11 (RBF) = not used
                 * Lower bits = received data byte (0)
                 */
                {
                    uint16_t serdatr = 0x3000;  /* TBE + TSRE set */
                    result = (reg & 1) ? (serdatr & 0xFF) : ((serdatr >> 8) & 0xFF);
                }
                break;
            case CUSTOM_REG_INTENAR:  /* Interrupt enable bits read */
                result = (reg & 1) ? (g_intena & 0xFF) : ((g_intena >> 8) & 0xFF);
                break;
            case CUSTOM_REG_INTREQR:  /* Interrupt request bits read */
                result = (reg & 1) ? (g_intreq & 0xFF) : ((g_intreq >> 8) & 0xFF);
                break;
            case CUSTOM_REG_ADKCONR:  /* Audio/disk control read - return 0 */
                result = 0;
                break;
            default:
                if ((reg & ~1) >= CUSTOM_REG_COLOR00 &&
                    (reg & ~1) <  CUSTOM_REG_COLOR00 + 64)
                {
                    /* Color registers are write-only on real hardware (reads
                     * return undefined), but exposing the shadow makes
                     * automated tests trivial. */
                    uint32_t idx = ((reg & ~1) - CUSTOM_REG_COLOR00) >> 1;
                    uint16_t v = g_color_regs[idx];
                    result = (reg & 1) ? (v & 0xFF) : ((v >> 8) & 0xFF);
                }
                else
                {
                    result = 0;
                }
                break;
        }
        
        DPRINTF (LOG_DEBUG, "lxa: mread8 CUSTOM (0x%08x/0x%03x) -> 0x%02x\n",
                address, reg, result);
        return result;
    }
    else
    {
        /* 
         * Invalid address - print warning but don't enter debugger.
         * Some programs may read from invalid addresses intentionally
         * (e.g., checking for expansion boards, sentinel values, etc.)
         * Return 0 and let the program continue.
         */
        static int invalid_read_count = 0;
        if (invalid_read_count < 10) {
            printf("WARNING: mread8 at invalid address 0x%08x (PC=0x%08x)\n", 
                   address, m68k_get_reg(NULL, M68K_REG_PC));
            invalid_read_count++;
            if (invalid_read_count == 10) {
                printf("WARNING: suppressing further invalid read warnings\n");
            }
        }
    }

    return 0;
}

static inline void mwrite8(uint32_t address, uint8_t value)
{
    if ((address >= RAM_START) && (address <= RAM_END))
    {
        uint32_t addr = address - RAM_START;
        g_ram[addr] = value;
    }
    else if ((address >= 0x00A00000) && (address < CUSTOM_START))
    {
        /* RAM overflow area (10MB - just before custom chips at 0xDFF000) - some apps allocate
         * to end of RAM and overflow into what would be expansion RAM, slow RAM, or CIA areas.
         * Silently ignore these writes. */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 RAM overflow area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= 0x01000000) && (address <= 0x0FFFFFFF))
    {
        /* Zorro-III expansion area writes - ignore (no expansion present) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 Zorro-III area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= SLOWRAM_START) && (address <= SLOWRAM_END))
    {
        /* Slow RAM area writes - ignore (no expansion memory present) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 Slow RAM area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= RANGER_START) && (address <= RANGER_END))
    {
        /* Ranger/expansion RAM area writes - ignore (no expansion memory present) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 Ranger RAM area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= ZORRO2_AUTOCONFIG_START) && (address <= ZORRO2_AUTOCONFIG_END))
    {
        /* Zorro-II autoconfig area writes - ignore (no expansion boards) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 Zorro-II autoconfig area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= EXTROM_START) && (address <= EXTROM_END))
    {
        /* Extended ROM area writes - ignore (some apps probe this area) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 Extended ROM area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= ROM_START) && (address <= ROM_END))
    {
        /* ROM area writes - ignore (ROM is read-only, some apps try to write for various reasons) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 ROM area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= CIA_START) && (address <= CIA_END))
    {
        /* CIA chip area - ignore writes, we don't emulate CIA hardware */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 CIA area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= CUSTOM_START) && (address <= CUSTOM_END))
    {
        /* Custom chip area - handle via custom write handler (byte writes are rare but possible) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 custom area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else
    {
        printf("ERROR: mwrite8 at invalid address 0x%08x\n", address);
        _debug(m68k_get_reg(NULL, M68K_REG_PC));
        assert (false);
    }
}

#endif /* LXA_MEMORY_H */
