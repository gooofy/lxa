/*
 * lxa_coverage.c - ROM code coverage (Phase 202)
 *
 * When the environment variable LXA_ROM_COVERAGE names a directory, every
 * executed ROM instruction address is recorded in a bitmap (one bit per
 * 16-bit word of the 512 KB ROM).  On shutdown the bitmap is OR-merged into
 * <dir>/rom.cov under an exclusive flock, so any number of test processes
 * can contribute to one coverage file.  tools/coverage_report.py maps the
 * bits to source lines via the ROM linker map and the -g object files.
 *
 * Disk libraries (rexxsyslib, amigaguide, datatypes, ...) are loaded into RAM
 * at varying addresses, so RAM execution is tracked in a second bitmap and,
 * at flush time, every library/device jump-table vector that points into
 * RAM is resolved and recorded as "<name> <lvo> <hit>" in <dir>/disklibs.cov
 * (merged, keeping hits).
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#include "lxa_coverage.h"
#include "m68k.h"

#define COV_ROM_START  0xf80000u
#define COV_ROM_SIZE   (512u * 1024u)
#define COV_BITMAP_LEN (COV_ROM_SIZE / 2u / 8u)    /* 32 KB */
#define COV_RAM_SIZE   (10u * 1024u * 1024u)
#define COV_RAM_LEN    (COV_RAM_SIZE / 2u / 8u)    /* 640 KB */

/* ExecBase list offsets (NDK exec/execbase.h) */
#define EB_DEVICELIST  350
#define EB_LIBLIST     378

uint8_t *g_rom_cov_bitmap = NULL;
static uint8_t *s_ram_cov_bitmap = NULL;
static char s_cov_dir[4096];

void lxa_coverage_init(void)
{
    const char *dir = getenv("LXA_ROM_COVERAGE");

    if (!dir || !dir[0])
        return;

    if (!g_rom_cov_bitmap)
        g_rom_cov_bitmap = calloc(1, COV_BITMAP_LEN);
    else
        memset(g_rom_cov_bitmap, 0, COV_BITMAP_LEN);
    if (!s_ram_cov_bitmap)
        s_ram_cov_bitmap = calloc(1, COV_RAM_LEN);
    else
        memset(s_ram_cov_bitmap, 0, COV_RAM_LEN);

    snprintf(s_cov_dir, sizeof(s_cov_dir), "%s", dir);
}

void lxa_coverage_mark_slow(uint32_t pc)
{
    uint32_t word;

    if (pc < COV_RAM_SIZE)
    {
        word = pc >> 1;
        s_ram_cov_bitmap[word >> 3] |= (uint8_t)(1u << (word & 7));
        return;
    }
    if (pc < COV_ROM_START || pc >= COV_ROM_START + COV_ROM_SIZE)
        return;
    word = (pc - COV_ROM_START) >> 1;
    g_rom_cov_bitmap[word >> 3] |= (uint8_t)(1u << (word & 7));
}

static int ram_hit(uint32_t addr)
{
    uint32_t word = addr >> 1;
    return addr < COV_RAM_SIZE && ((s_ram_cov_bitmap[word >> 3] >> (word & 7)) & 1);
}

/* Record RAM-resident jump-table vectors of one exec list into `out`. */
static void collect_list(uint32_t list_addr, FILE *out)
{
    uint32_t node = m68k_read_memory_32(list_addr);
    int guard = 0;

    while (node && node != list_addr + 4 && guard++ < 512)
    {
        uint32_t name_ptr = m68k_read_memory_32(node + 10);
        uint16_t negsize = m68k_read_memory_16(node + 16);
        char name[64];
        int i;

        for (i = 0; i < 63 && name_ptr; i++)
        {
            name[i] = (char)m68k_read_memory_8(name_ptr + i);
            if (!name[i])
                break;
        }
        name[i < 63 ? i : 63] = 0;

        if (name[0] && negsize >= 6 && negsize < 0x2000)
        {
            int lvo;
            for (lvo = 6; lvo <= negsize; lvo += 6)
            {
                uint32_t v = node - (uint32_t)lvo;
                uint32_t target;
                if (m68k_read_memory_16(v) != 0x4EF9)       /* JMP abs.l */
                    continue;
                target = m68k_read_memory_32(v + 2);
                if (target >= COV_RAM_SIZE)                  /* ROM or bogus */
                    continue;
                fprintf(out, "%s -%d %d\n", name, lvo, ram_hit(target));
            }
        }
        node = m68k_read_memory_32(node);
    }
}

/* Merge "<name> <lvo> <hit>" records into <dir>/disklibs.cov (hits win). */
static void flush_disklibs(void)
{
    char path[4200];
    char *mem = NULL;
    size_t memlen = 0;
    FILE *cur;
    uint32_t sysbase = m68k_read_memory_32(4);
    int fd;

    if (!sysbase || sysbase >= COV_RAM_SIZE)
        return;

    cur = open_memstream(&mem, &memlen);
    if (!cur)
        return;
    collect_list(sysbase + EB_LIBLIST, cur);
    collect_list(sysbase + EB_DEVICELIST, cur);
    fclose(cur);

    snprintf(path, sizeof(path), "%s/disklibs.cov", s_cov_dir);
    fd = open(path, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd >= 0)
    {
        if (flock(fd, LOCK_EX) == 0)
        {
            if (memlen && write(fd, mem, memlen) != (ssize_t)memlen)
                perror("lxa_coverage_flush: disklibs");
            flock(fd, LOCK_UN);
        }
        close(fd);
    }
    free(mem);
}

void lxa_coverage_flush(void)
{
    char path[4200];
    uint8_t merged[COV_BITMAP_LEN];
    int fd;
    ssize_t n;
    uint32_t i;

    if (!g_rom_cov_bitmap || !s_cov_dir[0])
        return;

    flush_disklibs();
    memset(s_ram_cov_bitmap, 0, COV_RAM_LEN);

    snprintf(path, sizeof(path), "%s/rom.cov", s_cov_dir);
    fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0)
    {
        perror("lxa_coverage_flush: open");
        return;
    }

    if (flock(fd, LOCK_EX) == 0)
    {
        memset(merged, 0, sizeof(merged));
        n = pread(fd, merged, sizeof(merged), 0);
        (void)n;
        for (i = 0; i < COV_BITMAP_LEN; i++)
            merged[i] |= g_rom_cov_bitmap[i];
        if (pwrite(fd, merged, sizeof(merged), 0) != (ssize_t)sizeof(merged))
            perror("lxa_coverage_flush: write");
        flock(fd, LOCK_UN);
    }
    close(fd);

    memset(g_rom_cov_bitmap, 0, COV_BITMAP_LEN);
}
