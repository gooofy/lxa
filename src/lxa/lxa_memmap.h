/*
 * lxa_memmap.h - the emulated machine's RAM (Phase 240).
 *
 * Shared by the emulator core, liblxa and the display code: the RAM
 * regions, their host arrays and the translation of an emulated address
 * into a host pointer.
 */
#ifndef LXA_MEMMAP_H
#define LXA_MEMMAP_H

#include <stddef.h>
#include <stdint.h>

/* The memory of the reference A4000 (Phase 240, SysInfo MEMORY/BOARDS):
 * 2 MB chip RAM at 0, 8 MB 32-bit motherboard fast RAM ending at
 * $08000000 and 16 MB Zorro III fast RAM at $40000000 (FS-UAE
 * zorro_iii_memory).  The blitter reaches chip RAM only. */
#define RAM_START   0x000000
#define RAM_SIZE    (2 * 1024 * 1024)
#define RAM_END     (RAM_START + RAM_SIZE - 1)

#define FASTRAM_START 0x07800000u
#define FASTRAM_SIZE  (8u * 1024u * 1024u)
#define FASTRAM_END   (FASTRAM_START + FASTRAM_SIZE - 1u)

#define Z3RAM_START 0x40000000u
#define Z3RAM_SIZE  (16u * 1024u * 1024u)
#define Z3RAM_END   (Z3RAM_START + Z3RAM_SIZE - 1u)

extern uint8_t  g_ram[];
extern uint8_t  g_fastram[];
extern uint8_t  g_z3ram[];

/* The host address of len bytes of emulated RAM at addr (chip, fast or
 * Zorro III RAM, all of them inside one region), else NULL. */
static inline uint8_t *lxa_ram_ptr(uint32_t addr, uint32_t len)
{
    if (len == 0)
        len = 1;
    if (addr <= RAM_END && len <= (uint32_t)RAM_END - addr + 1)
        return &g_ram[addr - RAM_START];
    if (addr >= FASTRAM_START && addr <= FASTRAM_END && len <= FASTRAM_END - addr + 1)
        return &g_fastram[addr - FASTRAM_START];
    if (addr >= Z3RAM_START && addr <= Z3RAM_END && len <= Z3RAM_END - addr + 1)
        return &g_z3ram[addr - Z3RAM_START];
    return NULL;
}

/* bytes of emulated RAM from addr to the end of its region (0: no RAM) */
static inline uint32_t lxa_ram_avail(uint32_t addr)
{
    if (addr <= RAM_END)
        return (uint32_t)RAM_END - addr + 1;
    if (addr >= FASTRAM_START && addr <= FASTRAM_END)
        return FASTRAM_END - addr + 1;
    if (addr >= Z3RAM_START && addr <= Z3RAM_END)
        return Z3RAM_END - addr + 1;
    return 0;
}
#endif /* LXA_MEMMAP_H */
