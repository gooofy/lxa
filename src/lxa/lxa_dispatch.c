/*
 * lxa_dispatch.c - CPU memory access dispatch and ILLG instruction handler.
 *
 * Contains: m68k memory read/write callbacks, op_illg(), and
 * float/double math helpers used by op_illg.
 *
 * Phase 125: extracted from lxa.c.
 */

#include "lxa_internal.h"
#include "lxa_memory.h"
#include "config.h"

/* Forward declarations for float/double helpers defined later in this file */
static float ffp_to_host_float(uint32_t raw);
static uint32_t host_float_to_ffp(float value);
static float host_float_from_bits(uint32_t bits);
static uint32_t host_float_to_bits(float value);
static float host_float_from_m68k_register(int reg);
static void host_float_to_m68k_register(float value, int reg);
static double host_double_from_words(uint32_t hi, uint32_t lo);
static void host_double_to_words(double value, uint32_t *hi, uint32_t *lo);
static double host_double_from_m68k_registers(int hi_reg, int lo_reg);
static void host_double_to_m68k_registers(double value, int hi_reg, int lo_reg);
static void host_double_to_m68k_memory(uint32_t addr, double value);
static int32_t clamp_double_to_long(double value);
static int32_t clamp_float_to_long(float value);
static int32_t compare_double_values(double left, double right);
static int32_t compare_float_values(float left, float right);
static double host_safe_double_divide(double dividend, double divisor);
static float host_safe_float_divide(float dividend, float divisor);

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    // if (g_trace)
    //     DPRINTF("WRITE8  at 0x%08x -> 0x%02x\n", address, value);
    mwrite8 (address, value);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    /* Fast path: RAM write (most common case) */
    if (__builtin_expect((address >= RAM_START) && (address + 1 <= RAM_END), 1))
    {
        mwrite16_ram(address - RAM_START, (uint16_t)value);
        return;
    }

    if ((address >= CUSTOM_START) && (address <= CUSTOM_END))
    {
        _handle_custom_write (address & 0xfff, value);
        return;
    }


    // if (g_trace)
    //     DPRINTF("WRITE16 at 0x%08x -> 0x%04x\n", address, value);
    mwrite8 (address  , (value >> 8) & 0xff);
    mwrite8 (address+1, value & 0xff);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    /* Fast path: RAM write (most common case) */
    if (__builtin_expect((address >= RAM_START) && (address + 3 <= RAM_END), 1))
    {
        mwrite32_ram(address - RAM_START, value);
        return;
    }

    // if (g_trace)
    //     DPRINTF("WRITE32 at 0x%08x -> 0x%08x\n", address, value);

    /*
     * Custom chip area: 32-bit writes must be handled as two 16-bit writes
     * (high word first, low word second) because the custom chip register
     * handler processes 16-bit register writes.  This is critical for
     * blitter pointer registers (BLTAPT, BLTBPT, BLTCPT, BLTDPT) which
     * are written as 32-bit values by apps but stored as high/low halves
     * in adjacent 16-bit registers.
     */
    if ((address >= CUSTOM_START) && (address <= CUSTOM_END))
    {
        _handle_custom_write ((address    ) & 0xfff, (value >> 16) & 0xffff);
        _handle_custom_write ((address + 2) & 0xfff, value & 0xffff);
        return;
    }

    mwrite8 (address  , (value >>24) & 0xff);
    mwrite8 (address+1, (value >>16) & 0xff);
    mwrite8 (address+2, (value >> 8) & 0xff);
    mwrite8 (address+3, value & 0xff);
}

unsigned int m68k_read_memory_8(unsigned int address)
{
    // if (g_trace)
    //     DPRINTF("READ8  at 0x%08x\n", address);
    return mread8 (address);
}

unsigned int m68k_read_memory_16(unsigned int address)
{
    /* Fast path: RAM read (most common case) */
    if (__builtin_expect((address >= RAM_START) && (address + 1 <= RAM_END), 1))
        return mread16_ram(address - RAM_START);

    /* Fast path: ROM read (second most common) */
    if (__builtin_expect((address >= ROM_START) && (address + 1 <= ROM_END), 0))
        return mread16_rom(address - ROM_START);

    // if (g_trace)
    //     DPRINTF("READ16 at 0x%08x\n", address);
    unsigned int result = (unsigned int)mread8 (address) << 8;
    result |= mread8 (address + 1);
    return result;
}

unsigned int m68k_read_memory_32(unsigned int address)
{
    /* Fast path: RAM read (most common case) */
    if (__builtin_expect((address >= RAM_START) && (address + 3 <= RAM_END), 1))
        return mread32_ram(address - RAM_START);

    /* Fast path: ROM read (second most common) */
    if (__builtin_expect((address >= ROM_START) && (address + 3 <= ROM_END), 0))
        return mread32_rom(address - ROM_START);

    // if (g_trace)
    //     DPRINTF("READ32 at 0x%08x\n", address);
    unsigned int result = (unsigned int)mread8 (address)     << 24;
    result |=             (unsigned int)mread8 (address + 1) << 16;
    result |=             (unsigned int)mread8 (address + 2) <<  8;
    result |=             (unsigned int)mread8 (address + 3);
    return result;
}

unsigned int m68k_read_disassembler_16(unsigned int address)
{
    return m68k_read_memory_16 (address);
}

unsigned int m68k_read_disassembler_32(unsigned int address)
{
    return m68k_read_memory_32 (address);
}

#define YEAR_BASE    1900

#define _DAYS_IN_MONTH(x) ((x == 1) ? days_in_feb : __month_lengths[0][x])

static const int16_t _DAYS_BEFORE_MONTH[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

#define _DAYS_IN_YEAR(year) (isleap(year+YEAR_BASE) ? 366 : 365)

static inline int isleap (int y)
{
    // This routine must return exactly 0 or 1, because the result is used to index on __month_lengths[].
    // The order of checks below is the fastest for a random year.
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

int op_illg(int level)
{
    uint32_t d0 = m68k_get_reg(NULL, M68K_REG_D0);
    //DPRINTF (LOG_INFO, "ILLEGAL, d=%d\n", d0);

#ifdef PROFILE_BUILD
    struct timespec _prof_start, _prof_end;
    clock_gettime(CLOCK_MONOTONIC, &_prof_start);
#endif

    switch (d0)
    {
        case EMU_CALL_LPUTC:
        {
            uint32_t lvl = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t ch  = m68k_get_reg(NULL, M68K_REG_D2);

            lputc (lvl, ch);

            break;
        }

        case EMU_CALL_LPUTS:
        {
            uint32_t lvl = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t s68k = m68k_get_reg(NULL, M68K_REG_D2);

            char *s = _mgetstr (s68k);

            lputs (lvl, s);

            break;
        }

        case EMU_CALL_STOP:
        {
            g_rv = m68k_get_reg(NULL, M68K_REG_D1);
            fprintf(stderr, "EMU_CALL_STOP called, rv=%d, current PC=0x%08x\n", 
                     g_rv, m68k_get_reg(NULL, M68K_REG_PC));
            DPRINTF (LOG_DEBUG, "EMU_CALL_STOP called, rv=%d, current PC=0x%08x\n", 
                     g_rv, m68k_get_reg(NULL, M68K_REG_PC));
            
            /*
             * Check if there are other tasks running. If so, we need to
             * properly terminate the current task via RemTask(NULL) and
             * let the scheduler continue running other tasks.
             * 
             * If no other tasks are running, we can exit immediately.
             */
            if (other_tasks_running())
            {
                DPRINTF (LOG_DEBUG, "other tasks running, calling RemTask(NULL) instead of stopping\n");
                
                /*
                 * Set up the CPU to call RemTask(NULL):
                 * - A6 = SysBase
                 * - A1 = NULL (remove current task)
                 * 
                 * The library jump table contains:
                 *   offset -288: JMP instruction (0x4ef9) + 32-bit address
                 * 
                 * We jump directly to the RemTask entry point. Since RemTask(NULL)
                 * doesn't return when removing the current task, we don't need to
                 * set up a return address.
                 */
                uint32_t sysbase = m68k_read_memory_32(4);
                
                /* The function address is at offset +2 from the jump table entry 
                 * (skip the JMP instruction opcode) */
                uint32_t remtask_addr = m68k_read_memory_32(sysbase - 288 + 2);
                
                DPRINTF (LOG_DEBUG, "setting PC=0x%08x (RemTask), A6=0x%08x, A1=0\n", remtask_addr, sysbase);
                
                m68k_set_reg(M68K_REG_A6, sysbase);
                m68k_set_reg(M68K_REG_A1, 0);  // NULL = remove current task
                m68k_set_reg(M68K_REG_PC, remtask_addr);
                
                /* Don't end timeslice or set g_running to FALSE - let it continue */
            }
            else
            {
                DPRINTF (LOG_DEBUG, "*** no other tasks, stopping emulator\n");
                m68k_end_timeslice();
                g_running = FALSE;
            }
            break;
        }

        case EMU_CALL_EXIT:
        {
            g_rv = m68k_get_reg(NULL, M68K_REG_D1);
            fprintf(stderr, "EMU_CALL_EXIT called, rv=%d\n", g_rv);
            DPRINTF (LOG_DEBUG, "*** emulator exit via libnix, rv=%d\n", g_rv);
            
            /*
             * Check if there are other tasks running. If so, we need to
             * properly terminate the current task via RemTask(NULL) and
             * let the scheduler continue running other tasks.
             * 
             * If no other tasks are running, we can exit immediately.
             */
            if (other_tasks_running())
            {
                DPRINTF (LOG_DEBUG, "*** other tasks running, calling RemTask(NULL) instead of exiting\n");
                
                /*
                 * Set up the CPU to call RemTask(NULL):
                 * - A6 = SysBase
                 * - A1 = NULL (remove current task)
                 * 
                 * The library jump table contains:
                 *   offset -288: JMP instruction (0x4ef9) + 32-bit address
                 * 
                 * We jump directly to the RemTask entry point. Since RemTask(NULL)
                 * doesn't return when removing the current task, we don't need to
                 * set up a return address.
                 */
                uint32_t sysbase = m68k_read_memory_32(4);
                /* The function address is at offset +2 from the jump table entry 
                 * (skip the JMP instruction opcode) */
                uint32_t remtask_addr = m68k_read_memory_32(sysbase - 288 + 2);
                
                DPRINTF (LOG_DEBUG, "*** calling RemTask at 0x%08x\n", remtask_addr);
                
                m68k_set_reg(M68K_REG_A6, sysbase);
                m68k_set_reg(M68K_REG_A1, 0);  // NULL = remove current task
                m68k_set_reg(M68K_REG_PC, remtask_addr);
                
                /* Don't end timeslice or set g_running to FALSE - let it continue */
            }
            else
            {
                DPRINTF (LOG_DEBUG, "*** no other tasks, exiting emulator\n");
                m68k_end_timeslice();
                g_running = FALSE;
            }
            break;
        }

        case EMU_CALL_TRACE:
            g_trace = m68k_get_reg(NULL, M68K_REG_D1);
            _update_debug_active();
            DPRINTF (LOG_DEBUG, "set emulator tracing to %d\n", g_trace);
            break;

        case EMU_CALL_EXCEPTION:
        {
            uint32_t excn = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t isp  = m68k_get_reg(NULL, M68K_REG_ISP);

            uint32_t d0 = m68k_read_memory_32 (isp);
            uint32_t d1 = m68k_read_memory_32 (isp+4);
            uint32_t pc = m68k_read_memory_32 (isp+10);

            m68k_set_reg (M68K_REG_D0, d0);
            m68k_set_reg (M68K_REG_D1, d1);

            LPRINTF (LOG_WARNING, "*** EXCEPTION CAUGHT: pc=0x%08x #%2d ", pc, excn);

            switch (excn)
            {
                case  2: LPRINTF (LOG_WARNING, "bus error\n"); break;
                case  3: LPRINTF (LOG_WARNING, "address error\n"); break;
                case  4: LPRINTF (LOG_WARNING, "illegal instruction\n"); break;
                case  5: LPRINTF (LOG_WARNING, "divide by zero\n"); break;
                case  6: LPRINTF (LOG_WARNING, "chk instruction\n"); break;
                case  7: LPRINTF (LOG_WARNING, "trapv instruction\n"); break;
                case  8: LPRINTF (LOG_WARNING, "privilege violation\n"); break;
                case  9: LPRINTF (LOG_WARNING, "trace\n"); break;
                case 10: LPRINTF (LOG_WARNING, "line 1010 emulator\n"); break;
                case 11: LPRINTF (LOG_WARNING, "line 1111 emulator\n"); break;
                case 32: LPRINTF (LOG_WARNING, "trap #0\n"); break;
                case 33: LPRINTF (LOG_WARNING, "trap #1\n"); break;
                case 34: LPRINTF (LOG_WARNING, "trap #2 (stack overflow)\n"); break;
                case 35: LPRINTF (LOG_WARNING, "trap #3\n"); break;
                case 36: LPRINTF (LOG_WARNING, "trap #4\n"); break;
                case 37: LPRINTF (LOG_WARNING, "trap #5\n"); break;
                case 38: LPRINTF (LOG_WARNING, "trap #6\n"); break;
                case 39: LPRINTF (LOG_WARNING, "trap #7\n"); break;
                case 40: LPRINTF (LOG_WARNING, "trap #8\n"); break;
                case 41: LPRINTF (LOG_WARNING, "trap #9\n"); break;
                case 42: LPRINTF (LOG_WARNING, "trap #10\n"); break;
                case 43: LPRINTF (LOG_WARNING, "trap #11\n"); break;
                case 44: LPRINTF (LOG_WARNING, "trap #12\n"); break;
                case 45: LPRINTF (LOG_WARNING, "trap #13\n"); break;
                case 46: LPRINTF (LOG_WARNING, "trap #14\n"); break;
                default: LPRINTF (LOG_WARNING, "???\n"); break;
            }

            hexdump (LOG_WARNING, isp, 16);

            /*
             * Phase 32: Don't halt on exceptions in multitasking scenarios.
             * 
             * Instead of entering the debugger and halting, we just log the exception
             * and let the exception handler return via RTE. In a real Amiga, this would
             * typically cause the crashing task to hang or loop, but other tasks can
             * continue running.
             *
             * This allows test programs (running as background processes) to detect
             * windows that were opened before the app crashed.
             *
             * For debugging, use -d flag which enables verbose output.
             *
             * Phase 54: Trap exceptions (vectors 32-46) are always fatal.
             * Traps like #2 (stack overflow) indicate the task cannot continue.
             * We must halt the emulator since returning would cause undefined behavior.
             */
            bool is_trap = (excn >= 32 && excn <= 46);
            
            if (g_debug || is_trap) {
                if (is_trap) {
                    LPRINTF (LOG_ERROR, "*** FATAL: Trap #%d - task cannot continue\n", excn - 32);
                    fprintf(stderr, "*** FATAL: Trap #%d at PC=0x%08x\n", excn - 32, pc);
                }
                _debug(pc);
                m68k_end_timeslice();
                g_running = FALSE;
            } else {
                LPRINTF (LOG_WARNING, "*** Exception in task - continuing (use -d to halt and debug)\n");
            }
            break;
        }

        case EMU_CALL_WAIT:
        {
            /*
             * Phase 6.5: Improved wait handling
             *
             * When the scheduler dispatch loop has no ready tasks, it calls
             * EMU_CALL_WAIT. We poll SDL events and sleep briefly.
             * 
             * We also ensure VBlank interrupt is set if pending, so that input
             * processing can happen when we return to the dispatch loop.
             */
            if (display_poll_events())
            {
                /* Quit requested via SDL */
                g_running = false;
            }
            
            /*
             * Check if ALL tasks have exited (no tasks ready, no tasks waiting,
             * and no current task). If so, the emulator should stop.
             * 
             * This handles the case where the last task exits via Exit()/RemTask()
             * rather than emu_stop(), leaving the Dispatch loop waiting forever.
             */
            if (!other_tasks_running())
            {
                uint32_t sysbase = m68k_read_memory_32(4);
                uint32_t thisTask = m68k_read_memory_32(sysbase + EXECBASE_THISTASK);
                
                if (thisTask == 0)
                {
                    fprintf(stderr, "*** EMU_CALL_WAIT: no tasks left, stopping emulator\n");
                    DPRINTF(LOG_DEBUG, "*** EMU_CALL_WAIT: no tasks left, stopping emulator\n");
                    m68k_end_timeslice();
                    g_running = false;
                    break;
                }
            }
            
            /* If VBlank is pending, set the IRQ now so it fires after we return */
            if ((g_pending_irq & (1 << 3)) && 
                (g_intena & INTENA_MASTER) && (g_intena & INTENA_VBLANK))
            {
                g_pending_irq &= ~(1 << 3);
                m68k_set_irq(0);    /* Musashi edge-trigger: lower first */
                m68k_set_irq(3);
            }
            
            usleep(1000);  /* 1ms - avoid busy-waiting */
            break;
        }

        case EMU_CALL_DELAY:
        {
            /*
             * EMU_CALL_DELAY: Simple delay for the specified milliseconds.
             * Note: This is not used by the current Delay() implementation
             * but kept for potential future use.
             */
            uint32_t ms = m68k_get_reg(NULL, M68K_REG_D1);
            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_DELAY %u ms\n", ms);
            usleep(ms * 1000);
            break;
        }

        case EMU_CALL_MONITOR:
        {
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_MONITOR\n");

            _debug(m68k_get_reg(NULL, M68K_REG_PC));

            break;
        }

        case EMU_CALL_LOADFILE:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_LOADFILE d1=0x%08x\n", d1);

            for (int i=0; i<=strlen(g_loadfile); i++)
                m68k_write_memory_8 (d1++, g_loadfile[i]);

            break;
        }

        case EMU_CALL_GETARGS:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GETARGS d1=0x%08x, len=%d\n", d1, g_args_len);

            for (int i=0; i<=g_args_len; i++)
                m68k_write_memory_8 (d1++, g_args[i]);

            break;
        }

        case EMU_CALL_LOADED:
        {
            /* Handle pending breakpoints */
            for (pending_bp_t *pbp = _g_pending_bps; pbp; pbp=pbp->next)
            {
                uint32_t addr = _debug_parse_addr (pbp->name);
                if (!addr)
                {
                    fprintf (stderr, "*** error: failed to resolve pending breakpoint '%s'\n", pbp->name);
                    exit(23);
                }
                DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_LOADED pbp '%s' -> addr=0x%08lx\n", pbp->name, addr);
                _debug_add_bp (addr);
            }
            
            /*
             * Set up program-local assigns: Add the program's subdirectories to
             * standard assigns so programs can find their local config files.
             * This simulates what users typically did on real Amigas via startup
             * scripts: "Assign S: PROGDIR:S ADD"
             */
            {
                char amiga_progdir[PATH_MAX];
                char linux_progdir[PATH_MAX];
                char subdir[PATH_MAX];
                struct stat st;
                
                /* Extract the directory containing the loaded program (Amiga path) */
                strncpy(amiga_progdir, g_loadfile, sizeof(amiga_progdir) - 1);
                amiga_progdir[sizeof(amiga_progdir) - 1] = '\0';
                
                /* Find the last path separator (either / or after :) */
                char *last_slash = strrchr(amiga_progdir, '/');
                char *colon = strrchr(amiga_progdir, ':');
                
                /* If no slash, or colon is after the last slash, the file is directly in the volume/assign */
                if (last_slash && (!colon || last_slash > colon)) {
                    *last_slash = '\0';
                } else if (colon) {
                    /* Path is "VOLUME:filename" - directory is "VOLUME:" */
                    *(colon + 1) = '\0';
                } else {
                    /* No separator found - shouldn't happen, but handle gracefully */
                    amiga_progdir[0] = '\0';
                }
                
                /* Resolve Amiga path to Linux path */
                if (amiga_progdir[0] && vfs_resolve_path(amiga_progdir, linux_progdir, sizeof(linux_progdir))) {
                    /* Set PROGDIR: for this program (using the Linux path) */
                    vfs_set_progdir(linux_progdir);
                    
                    /* Standard Amiga directories to add to assigns */
                    struct {
                        const char *dir;
                        const char *assign;
                    } subdirs[] = {
                        {"s", "S"},
                        {"S", "S"},
                        {"libs", "LIBS"},
                        {"Libs", "LIBS"},
                        /* Bundled BOOPSI gadget classes — exposed via
                         * the GADGETS: assign. Apps typically open them
                         * with OpenLibrary("gadgets/foo.gadget"); the
                         * ROM falls back to GADGETS:foo.gadget when
                         * LIBS:gadgets/... is not found. */
                        {"gadgets", "GADGETS"},
                        {"Gadgets", "GADGETS"},
                        {"c", "C"},
                        {"C", "C"},
                        {"devs", "DEVS"},
                        {"Devs", "DEVS"},
                        {"l", "L"},
                        {"L", "L"},
                        {NULL, NULL}
                    };
                    
                    for (int i = 0; subdirs[i].dir; i++) {
                        snprintf(subdir, sizeof(subdir), "%s/%s", linux_progdir, subdirs[i].dir);
                        if (stat(subdir, &st) == 0 && S_ISDIR(st.st_mode)) {
                            /* Prepend this path to the assign (so it's searched first) */
                            vfs_assign_prepend_path(subdirs[i].assign, subdir);
                            DPRINTF(LOG_DEBUG, "lxa: Added %s to %s: assign\n", subdir, subdirs[i].assign);
                        }
                    }
                }
            }
            
            break;
        }

        case EMU_CALL_SYMBOL:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);

            char *name = _mgetstr(d2);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_SYMBOL name=%s, offset=0x%08lx\n", name, d1);
            _symtab_add (name, d1);

            break;
        }

        case EMU_CALL_GETSYSTIME:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);

            struct timeval tv;
            gettimeofday(&tv, 0);
            struct tm t;
            localtime_r(&tv.tv_sec, &t);
            DPRINTF (LOG_DEBUG, "lxa: lxa: op_illg(): EMU_CALL_GETSYSTIME %02ld:%02ld:%02ld\n",
                     t.tm_hour, t.tm_min, t.tm_sec);

            time_t tim = t.tm_sec + t.tm_min * 60 + t.tm_hour * 3600;

            /* compute days in year */
            long days = t.tm_mday - 1;
            days += _DAYS_BEFORE_MONTH[t.tm_mon];
            if (t.tm_mon > 1 && isleap (t.tm_year+YEAR_BASE))
                days++;

            /* compute day of the year */
            t.tm_yday = days;

            // amiga's epoch starts at Jan 1st, 1978
            if (t.tm_year > 78)
            {
                for (int year = 78; year < t.tm_year; year++)
                    days += _DAYS_IN_YEAR (year);
            }

            /* compute total seconds */
            tim += (time_t) days * 24*60*60;

            m68k_write_memory_32 (d1,   tim);        // ULONG tv_secs
            m68k_write_memory_32 (d1+4, tv.tv_usec); // ULONG tv_micro

            break;
        }

        case EMU_CALL_DOS_OPEN:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_OPEN name=0x%08x, accessMode=0x%08x, fh=0x%08x\n", d1, d2, d3);

            uint32_t res = _dos_open (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_READ:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_READ file=0x%08x, buffer=0x%08x, len=%d\n", d1, d2, d3);

            uint32_t res = _dos_read (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_SEEK:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SEEK file=0x%08x, position=0x%08x, mode=%d\n", d1, d2, d3);

            uint32_t res = _dos_seek (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_CLOSE:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_CLOSE file=0x%08x\n", d1);

            uint32_t res = _dos_close (d1);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_INPUT:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_INPUT, fh=0x%08x\n", fh);
            _dos_stdinout_fh (fh, 1);
            break;
        }

        case EMU_CALL_DOS_OUTPUT:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_OUTPUT, fh=0x%08x\n", fh);
            _dos_stdinout_fh (fh, 0);
            break;
        }

        case EMU_CALL_DOS_WRITE:
        {
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_WRITE\n");

            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_WRITE file=0x%08x, buffer=0x%08x, len=%d\n", d1, d2, d3);

            uint32_t res = _dos_write (d1, d2, d3);

            //_debug(m68k_get_reg(NULL, M68K_REG_PC));

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        /*
         * Phase 3: Lock and Examine API emucalls
         */
        case EMU_CALL_DOS_LOCK:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            int32_t mode = (int32_t)m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_LOCK name=0x%08x, mode=%d\n", name, mode);

            uint32_t res = _dos_lock(name, mode);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_UNLOCK:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_UNLOCK lock_id=%d\n", lock_id);

            _dos_unlock(lock_id);
            break;
        }

        case EMU_CALL_DOS_DUPLOCK:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_DUPLOCK lock_id=%d\n", lock_id);

            uint32_t res = _dos_duplock(lock_id);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_EXAMINE:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t fib = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_EXAMINE lock_id=%d, fib=0x%08x\n", lock_id, fib);

            uint32_t res = _dos_examine(lock_id, fib);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_EXNEXT:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t fib = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_EXNEXT lock_id=%d, fib=0x%08x\n", lock_id, fib);

            uint32_t res = _dos_exnext(lock_id, fib);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_INFO:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t infodata = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_INFO lock_id=%d, infodata=0x%08x\n", lock_id, infodata);

            uint32_t res = _dos_info(lock_id, infodata);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_SAMELOCK:
        {
            uint32_t lock1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t lock2 = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SAMELOCK lock1=%d, lock2=%d\n", lock1, lock2);

            int32_t res = _dos_samelock(lock1, lock2);
            m68k_set_reg(M68K_REG_D0, (uint32_t)res);
            break;
        }

        case EMU_CALL_DOS_PARENTDIR:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_PARENTDIR lock_id=%d\n", lock_id);

            uint32_t res = _dos_parentdir(lock_id);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_CREATEDIR:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_CREATEDIR name=0x%08x\n", name);

            uint32_t res = _dos_createdir(name);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_DELETE:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_DELETE name=0x%08x\n", name);

            uint32_t res = _dos_deletefile(name);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_RENAME:
        {
            uint32_t old_name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t new_name = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_RENAME old=0x%08x, new=0x%08x\n", old_name, new_name);

            uint32_t res = _dos_rename(old_name, new_name);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_NAMEFROMLOCK:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t buf = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t len = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_NAMEFROMLOCK lock_id=%d, buf=0x%08x, len=%d\n", lock_id, buf, len);

            uint32_t res = _dos_namefromlock(lock_id, buf, len);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_SETPROTECTION:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t protect = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SETPROTECTION name=0x%08x, protect=0x%08x\n", name, protect);

            uint32_t res = _dos_setprotection(name, protect);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_SETCOMMENT:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t comment = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SETCOMMENT name=0x%08x, comment=0x%08x\n", name, comment);

            uint32_t res = _dos_setcomment(name, comment);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_SETOWNER:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t owner = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t err = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SETOWNER name=0x%08x, owner=0x%08x, err=0x%08x\n",
                    name, owner, err);

            uint32_t res = _dos_setowner(name, owner, err);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_FLUSH:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_FLUSH fh=0x%08x\n", fh);

            uint32_t res = _dos_flush(fh);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        /* Phase 7: Assignment System */
        case EMU_CALL_DOS_ASSIGN_ADD:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t path = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t type = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_ASSIGN_ADD name=0x%08x, path=0x%08x, type=%d\n", name, path, type);

            uint32_t res = _dos_assign_add(name, path, type);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_ASSIGN_REMOVE:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_ASSIGN_REMOVE name=0x%08x\n", name);

            uint32_t res = _dos_assign_remove(name);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_ASSIGN_LIST:
        {
            uint32_t buf = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t buflen = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_ASSIGN_LIST buf=0x%08x, buflen=%d\n", buf, buflen);

            uint32_t res = _dos_assign_list(buf, buflen);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_ASSIGN_REMOVE_PATH:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t path = m68k_get_reg(NULL, M68K_REG_D2);

            uint32_t res = _dos_assign_remove_path(name, path);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_GETDEVPROC:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t dp = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t errptr = m68k_get_reg(NULL, M68K_REG_D3);

            uint32_t res = _dos_getdevproc(name, dp, errptr);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_NOTIFY_START:
        {
            uint32_t notify = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t fullname = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t flags = m68k_get_reg(NULL, M68K_REG_D3);

            uint32_t res = _dos_notify_start(notify, fullname, flags);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_NOTIFY_END:
        {
            uint32_t notify = m68k_get_reg(NULL, M68K_REG_D1);

            _dos_notify_end(notify);
            break;
        }

        case EMU_CALL_DOS_NOTIFY_POLL:
        {
            uint32_t res = _dos_notify_poll();
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        /*
         * Phase 10: File Handle Utilities
         */
        case EMU_CALL_DOS_DUPLOCKFROMFH:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_DUPLOCKFROMFH fh=0x%08x\n", fh);

            uint32_t res = _dos_duplockfromfh(fh);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_EXAMINEFH:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t fib = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_EXAMINEFH fh=0x%08x, fib=0x%08x\n", fh, fib);

            uint32_t res = _dos_examinefh(fh, fib);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_NAMEFROMFH:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t buf = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t len = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_NAMEFROMFH fh=0x%08x, buf=0x%08x, len=%d\n", fh, buf, len);

            uint32_t res = _dos_namefromfh(fh, buf, len);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_PARENTOFFH:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_PARENTOFFH fh=0x%08x\n", fh);

            uint32_t res = _dos_parentoffh(fh);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_OPENFROMLOCK:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_OPENFROMLOCK lock_id=%d, fh=0x%08x\n", lock_id, fh);

            uint32_t res = _dos_openfromlock(lock_id, fh);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_WAITFORCHAR:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t timeout = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_WAITFORCHAR fh=0x%08x, timeout=%u\n", fh, timeout);

            uint32_t res = _dos_waitforchar(fh, timeout);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_SETFILESIZE:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            int32_t offset = (int32_t)m68k_get_reg(NULL, M68K_REG_D2);
            int32_t mode = (int32_t)m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SETFILESIZE fh=0x%08x, offset=%d, mode=%d\n",
                    fh, offset, mode);

            m68k_set_reg(M68K_REG_D0, _dos_setfilesize(fh, offset, mode));
            break;
        }

        case EMU_CALL_DOS_SETFILEDATE:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t date = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t err = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SETFILEDATE name=0x%08x, date=0x%08x, err=0x%08x\n",
                    name, date, err);

            m68k_set_reg(M68K_REG_D0, _dos_setfiledate(name, date, err));
            break;
        }

        case EMU_CALL_DOS_READLINK:
        {
            uint32_t path = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t buffer = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t size = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t err = m68k_get_reg(NULL, M68K_REG_D4);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_READLINK path=0x%08x, buffer=0x%08x, size=%u, err=0x%08x\n",
                    path, buffer, size, err);

            m68k_set_reg(M68K_REG_D0, _dos_readlink(path, buffer, size, err));
            break;
        }

        case EMU_CALL_DOS_MAKELINK:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t dest = m68k_get_reg(NULL, M68K_REG_D2);
            int32_t soft = (int32_t)m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t err = m68k_get_reg(NULL, M68K_REG_D4);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_MAKELINK name=0x%08x, dest=0x%08x, soft=%d, err=0x%08x\n",
                    name, dest, soft, err);

            m68k_set_reg(M68K_REG_D0, _dos_makelink(name, dest, soft, err));
            break;
        }

        case EMU_CALL_DOS_LOCKRECORD:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t mode = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t timeout = m68k_get_reg(NULL, M68K_REG_D5);

            DPRINTF(LOG_DEBUG,
                    "lxa: op_illg(): EMU_CALL_DOS_LOCKRECORD fh=0x%08x offset=%u length=%u mode=%u timeout=%u\n",
                    fh, offset, length, mode, timeout);

            m68k_set_reg(M68K_REG_D0, _dos_lockrecord(fh, offset, length, mode, timeout));
            break;
        }

        case EMU_CALL_DOS_UNLOCKRECORD:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG,
                    "lxa: op_illg(): EMU_CALL_DOS_UNLOCKRECORD fh=0x%08x offset=%u length=%u\n",
                    fh, offset, length);

            m68k_set_reg(M68K_REG_D0, _dos_unlockrecord(fh, offset, length));
            break;
        }

        case EMU_CALL_DOS_CHANGEMODE:
        {
            uint32_t type = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t object = m68k_get_reg(NULL, M68K_REG_D2);
            int32_t new_mode = (int32_t)m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t err = m68k_get_reg(NULL, M68K_REG_D4);

            DPRINTF(LOG_DEBUG,
                    "lxa: op_illg(): EMU_CALL_DOS_CHANGEMODE type=%u object=0x%08x new_mode=%d err=0x%08x\n",
                    type, object, new_mode, err);

            m68k_set_reg(M68K_REG_D0, _dos_change_mode(type, object, new_mode, err));
            break;
        }

        case EMU_CALL_TRACKDISK_READ:
        {
            uint32_t unit = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t data = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t is_ext = m68k_get_reg(NULL, M68K_REG_D5);

            m68k_set_reg(M68K_REG_D0, _trackdisk_read(unit, data, length, offset, is_ext));
            break;
        }

        case EMU_CALL_TRACKDISK_WRITE:
        {
            uint32_t unit = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t data = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t is_ext = m68k_get_reg(NULL, M68K_REG_D5);

            m68k_set_reg(M68K_REG_D0, _trackdisk_write(unit, data, length, offset, is_ext));
            break;
        }

        case EMU_CALL_TRACKDISK_FORMAT:
        {
            uint32_t unit = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t data = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t is_ext = m68k_get_reg(NULL, M68K_REG_D5);

            m68k_set_reg(M68K_REG_D0, _trackdisk_format(unit, data, length, offset, is_ext));
            break;
        }

        case EMU_CALL_TRACKDISK_SEEK:
        {
            uint32_t unit = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t is_ext = m68k_get_reg(NULL, M68K_REG_D3);

            m68k_set_reg(M68K_REG_D0, _trackdisk_seek(unit, offset, is_ext));
            break;
        }

        /*
         * Graphics Library emucalls (2000-2999)
         * Phase 13: Graphics Foundation
         */

        case EMU_CALL_GFX_INIT:
        {
            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_INIT\n");
            bool success = display_init();
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_GFX_SHUTDOWN:
        {
            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_SHUTDOWN\n");
            display_shutdown();
            break;
        }

        case EMU_CALL_GFX_OPEN_DISPLAY:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            int width = (int)d1;
            int height = (int)d2;
            int depth = (int)d3;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_OPEN_DISPLAY %dx%dx%d\n",
                    width, height, depth);

            display_t *display = display_open(width, height, depth, "LXA Amiga Display");
            /* Store display handle as a host pointer - we'll use the pointer address as a handle */
            m68k_set_reg(M68K_REG_D0, (uint32_t)(uintptr_t)display);
            break;
        }

        case EMU_CALL_GFX_CLOSE_DISPLAY:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_t *display = (display_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_CLOSE_DISPLAY handle=0x%08x\n", d1);

            display_close(display);
            break;
        }

        case EMU_CALL_GFX_REFRESH:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_t *display = (display_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_REFRESH handle=0x%08x\n", d1);

            display_refresh(display);
            break;
        }

        case EMU_CALL_GFX_SET_COLOR:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            display_t *display = (display_t *)(uintptr_t)d1;
            int index = (int)(d2 & 0xFF);
            uint8_t r = (d3 >> 16) & 0xFF;
            uint8_t g = (d3 >> 8) & 0xFF;
            uint8_t b = d3 & 0xFF;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_SET_COLOR handle=0x%08x, idx=%d, rgb=0x%06x\n",
                    d1, index, d3 & 0xFFFFFF);

            display_set_color(display, index, r, g, b);
            break;
        }

        case EMU_CALL_GFX_SET_PALETTE4:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t a0 = m68k_get_reg(NULL, M68K_REG_A0);
            display_t *display = (display_t *)(uintptr_t)d1;
            int start = (int)d2;
            int count = (int)d3;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_SET_PALETTE4 handle=0x%08x, start=%d, count=%d, colors=0x%08x\n",
                    d1, start, count, a0);

            if (a0 && count > 0)
            {
                /* Read RGB4 values from m68k memory */
                uint16_t *colors = malloc(count * sizeof(uint16_t));
                if (colors)
                {
                    for (int i = 0; i < count; i++)
                    {
                        colors[i] = m68k_read_memory_16(a0 + i * 2);
                    }
                    display_set_palette_rgb4(display, start, count, colors);
                    free(colors);
                }
            }
            break;
        }

        case EMU_CALL_GFX_SET_PALETTE32:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t a0 = m68k_get_reg(NULL, M68K_REG_A0);
            display_t *display = (display_t *)(uintptr_t)d1;
            int start = (int)d2;
            int count = (int)d3;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_SET_PALETTE32 handle=0x%08x, start=%d, count=%d, colors=0x%08x\n",
                    d1, start, count, a0);

            if (a0 && count > 0)
            {
                /* Read RGB32 values from m68k memory */
                uint32_t *colors = malloc(count * sizeof(uint32_t));
                if (colors)
                {
                    for (int i = 0; i < count; i++)
                    {
                        colors[i] = m68k_read_memory_32(a0 + i * 4);
                    }
                    display_set_palette_rgb32(display, start, count, colors);
                    free(colors);
                }
            }
            break;
        }

        case EMU_CALL_GFX_WRITE_PIXEL:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            display_t *display = (display_t *)(uintptr_t)d1;
            int x = (int)d2;
            int y = (int)d3;
            uint8_t pen = (uint8_t)d4;

            /* Write pixel directly to display pixel buffer */
            display_update_chunky(display, x, y, 1, 1, &pen, 1);
            break;
        }

        case EMU_CALL_GFX_READ_PIXEL:
        {
            /* TODO: Implement pixel read - requires access to display internal buffer */
            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_READ_PIXEL (not implemented)\n");
            m68k_set_reg(M68K_REG_D0, 0);
            break;
        }

        case EMU_CALL_GFX_RECT_FILL:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t d5 = m68k_get_reg(NULL, M68K_REG_D5);
            uint32_t d6 = m68k_get_reg(NULL, M68K_REG_D6);
            display_t *display = (display_t *)(uintptr_t)d1;
            int x1 = (int)d2;
            int y1 = (int)d3;
            int x2 = (int)d4;
            int y2 = (int)d5;
            uint8_t pen = (uint8_t)d6;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_RECT_FILL handle=0x%08x, (%d,%d)-(%d,%d), pen=%d\n",
                    d1, x1, y1, x2, y2, pen);

            /* Create filled rectangle in a temporary buffer and blit it */
            int width = x2 - x1 + 1;
            int height = y2 - y1 + 1;
            if (width > 0 && height > 0)
            {
                uint8_t *pixels = malloc(width * height);
                if (pixels)
                {
                    memset(pixels, pen, width * height);
                    display_update_chunky(display, x1, y1, width, height, pixels, width);
                    free(pixels);
                }
            }
            break;
        }

        case EMU_CALL_GFX_DRAW_LINE:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t d5 = m68k_get_reg(NULL, M68K_REG_D5);
            uint32_t d6 = m68k_get_reg(NULL, M68K_REG_D6);
            display_t *display = (display_t *)(uintptr_t)d1;
            int x1 = (int)d2;
            int y1 = (int)d3;
            int x2 = (int)d4;
            int y2 = (int)d5;
            uint8_t pen = (uint8_t)d6;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_DRAW_LINE handle=0x%08x, (%d,%d)-(%d,%d), pen=%d\n",
                    d1, x1, y1, x2, y2, pen);

            /* Bresenham's line algorithm */
            int dx = abs(x2 - x1);
            int dy = -abs(y2 - y1);
            int sx = x1 < x2 ? 1 : -1;
            int sy = y1 < y2 ? 1 : -1;
            int err = dx + dy;

            while (1)
            {
                display_update_chunky(display, x1, y1, 1, 1, &pen, 1);
                if (x1 == x2 && y1 == y2)
                    break;
                int e2 = 2 * err;
                if (e2 >= dy)
                {
                    err += dy;
                    x1 += sx;
                }
                if (e2 <= dx)
                {
                    err += dx;
                    y1 += sy;
                }
            }
            break;
        }

        case EMU_CALL_GFX_UPDATE_PLANAR:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t d5 = m68k_get_reg(NULL, M68K_REG_D5);
            uint32_t d6 = m68k_get_reg(NULL, M68K_REG_D6);
            uint32_t d7 = m68k_get_reg(NULL, M68K_REG_D7);
            uint32_t a0 = m68k_get_reg(NULL, M68K_REG_A0);
            display_t *display = (display_t *)(uintptr_t)d1;
            int x = (int)d2;
            int y = (int)d3;
            int width = (int)d4;
            int height = (int)d5;
            int bytes_per_row = (int)d6;
            int depth = (int)d7;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_UPDATE_PLANAR handle=0x%08x, (%d,%d) %dx%d, bpr=%d, depth=%d, planes=0x%08x\n",
                    d1, x, y, width, height, bytes_per_row, depth, a0);

            if (a0 && depth > 0 && depth <= 8 && width > 0 && height > 0)
            {
                /* Read plane pointers from m68k memory */
                const uint8_t *planes[8] = {NULL};
                uint8_t *plane_data[8] = {NULL};
                int valid_planes = 0;

                for (int i = 0; i < depth; i++)
                {
                    uint32_t plane_ptr = m68k_read_memory_32(a0 + i * 4);
                    if (plane_ptr)
                    {
                        /* Map m68k plane pointer to host memory */
                        plane_data[i] = malloc(bytes_per_row * height);
                        if (plane_data[i])
                        {
                            /* Copy plane data from m68k memory */
                            for (int row = 0; row < height; row++)
                            {
                                for (int col = 0; col < bytes_per_row; col++)
                                {
                                    plane_data[i][row * bytes_per_row + col] =
                                        m68k_read_memory_8(plane_ptr + row * bytes_per_row + col);
                                }
                            }
                            planes[i] = plane_data[i];
                            valid_planes++;
                        }
                    }
                }

                /* Update display with planar data */
                if (valid_planes > 0)
                {
                    display_update_planar(display, x, y, width, height, planes, bytes_per_row, depth);
                }

                /* Free temporary plane buffers */
                for (int i = 0; i < depth; i++)
                {
                    free(plane_data[i]);
                }
            }
            break;
        }

        case EMU_CALL_GFX_UPDATE_CHUNKY:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t d5 = m68k_get_reg(NULL, M68K_REG_D5);
            uint32_t d6 = m68k_get_reg(NULL, M68K_REG_D6);
            uint32_t a0 = m68k_get_reg(NULL, M68K_REG_A0);
            display_t *display = (display_t *)(uintptr_t)d1;
            int x = (int)d2;
            int y = (int)d3;
            int width = (int)d4;
            int height = (int)d5;
            int pitch = (int)d6;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_UPDATE_CHUNKY handle=0x%08x, (%d,%d) %dx%d, pitch=%d, pixels=0x%08x\n",
                    d1, x, y, width, height, pitch, a0);

            if (a0 && width > 0 && height > 0)
            {
                /* Read pixel data from m68k memory */
                uint8_t *pixels = malloc(width * height);
                if (pixels)
                {
                    for (int row = 0; row < height; row++)
                    {
                        for (int col = 0; col < width; col++)
                        {
                            pixels[row * width + col] = m68k_read_memory_8(a0 + row * pitch + col);
                        }
                    }
                    display_update_chunky(display, x, y, width, height, pixels, width);
                    free(pixels);
                }
            }
            break;
        }

        case EMU_CALL_GFX_BLT_BITMAP:
        {
            /*
             * Phase 112: native host-C implementation of graphics.library
             * BltBitMap(). Argument struct (28 bytes) lives in m68k memory,
             * pointer in D1. Returns planesAffected mask in D0.
             *
             * Layout (matches struct LxaBltBitMapArgs in lxa_graphics.c):
             *   +0   ULONG  srcBM (m68k addr of struct BitMap)
             *   +4   WORD   xSrc
             *   +6   WORD   ySrc
             *   +8   ULONG  destBM
             *   +12  WORD   xDest
             *   +14  WORD   yDest
             *   +16  WORD   xSize
             *   +18  WORD   ySize
             *   +20  UBYTE  minterm
             *   +21  UBYTE  planeMask
             *   +22  UWORD  pixelMaskBpr
             *   +24  ULONG  pixelMask (m68k addr or 0)
             *
             * struct BitMap layout (offsets within m68k memory):
             *   +0   UWORD  BytesPerRow
             *   +2   UWORD  Rows
             *   +4   UBYTE  Flags
             *   +5   UBYTE  Depth
             *   +6   UWORD  pad
             *   +8   PLANEPTR Planes[8]   (each is m68k addr, 32 bits)
             */
            uint32_t args_ptr = m68k_get_reg(NULL, M68K_REG_D1);
            if (!args_ptr) {
                m68k_set_reg(M68K_REG_D0, 0);
                break;
            }

            /* Sign-extend 16-bit fields */
            #define READ_W(addr) ((int16_t)m68k_read_memory_16(addr))

            uint32_t srcBM         = m68k_read_memory_32(args_ptr + 0);
            int      xSrc          = READ_W(args_ptr + 4);
            int      ySrc          = READ_W(args_ptr + 6);
            uint32_t destBM        = m68k_read_memory_32(args_ptr + 8);
            int      xDest         = READ_W(args_ptr + 12);
            int      yDest         = READ_W(args_ptr + 14);
            int      xSize         = READ_W(args_ptr + 16);
            int      ySize         = READ_W(args_ptr + 18);
            uint8_t  minterm       = m68k_read_memory_8(args_ptr + 20);
            uint8_t  planeMask     = m68k_read_memory_8(args_ptr + 21);
            uint16_t pixelMaskBpr  = m68k_read_memory_16(args_ptr + 22);
            uint32_t pixelMaskAddr = m68k_read_memory_32(args_ptr + 24);

            #undef READ_W

            if (!srcBM || !destBM || xSize <= 0 || ySize <= 0 || planeMask == 0) {
                m68k_set_reg(M68K_REG_D0, 0);
                break;
            }

            /* Read BitMap headers */
            uint16_t srcBpr   = m68k_read_memory_16(srcBM + 0);
            uint16_t srcRows  = m68k_read_memory_16(srcBM + 2);
            uint8_t  srcDepth = m68k_read_memory_8 (srcBM + 5);
            uint16_t dstBpr   = m68k_read_memory_16(destBM + 0);
            uint16_t dstRows  = m68k_read_memory_16(destBM + 2);
            uint8_t  dstDepth = m68k_read_memory_8 (destBM + 5);

            int srcWidth  = srcBpr * 8;
            int dstWidth  = dstBpr * 8;
            int srcHeight = srcRows;
            int dstHeight = dstRows;

            /* Clip - same algorithm as m68k BltBitMapCore */
            int sx = xSrc, sy = ySrc, dx = xDest, dy = yDest;
            int aw = xSize, ah = ySize;

            if (sx < 0) { dx -= sx; aw += sx; sx = 0; }
            if (sy < 0) { dy -= sy; ah += sy; sy = 0; }
            if (dx < 0) { sx -= dx; aw += dx; dx = 0; }
            if (dy < 0) { sy -= dy; ah += dy; dy = 0; }

            if (sx + aw > srcWidth)  aw = srcWidth  - sx;
            if (sy + ah > srcHeight) ah = srcHeight - sy;
            if (dx + aw > dstWidth)  aw = dstWidth  - dx;
            if (dy + ah > dstHeight) ah = dstHeight - dy;

            if (aw <= 0 || ah <= 0) {
                m68k_set_reg(M68K_REG_D0, 0);
                break;
            }

            int depth = srcDepth < dstDepth ? srcDepth : dstDepth;

            /*
             * Read source plane addresses, allocate temp src plane row
             * buffers (we need stable host-side bytes for arbitrary x-shift
             * blits). We read src row data once per row into a small
             * stack buffer to avoid per-bit m68k_read_memory_8 calls in
             * the hot loop.
             *
             * For each plane:
             *   1. If planeMask bit clear -> skip
             *   2. If destPlane is NULL or 0xFFFFFFFF -> skip
             *   3. Determine row direction and column direction (overlap)
             *   4. For each row:
             *        - Read src row bytes [src_byte_start..src_byte_end] from m68k
             *        - Read dst row bytes [dst_byte_start..dst_byte_end] from m68k
             *          (only the bytes we'll modify)
             *        - Apply minterm + pixelMask + plane mask using bit-shifted src
             *        - Write modified dst row bytes back to m68k
             *
             * Common-case fast path: minterm==0xC0, no pixelMask, source
             * not equal to destination plane (no overlap), same x-byte
             * alignment -> straight memcpy after reading src and writing
             * dst directly. We don't bother with that explicit fast path
             * here; per-byte processing in C is already 100x faster than
             * m68k-emulated per-bit, and that's enough.
             */
            uint32_t srcPlanes[8] = {0};
            uint32_t dstPlanes[8] = {0};
            for (int p = 0; p < depth; p++) {
                srcPlanes[p] = m68k_read_memory_32(srcBM  + 8 + p * 4);
                dstPlanes[p] = m68k_read_memory_32(destBM + 8 + p * 4);
            }

            int planesAffected = 0;
            uint8_t srcRow[1024];   /* enough for 8192 px wide */
            uint8_t dstRow[1024];
            uint8_t pmRow[1024];

            for (int p = 0; p < depth; p++) {
                if (!(planeMask & (1u << p)))
                    continue;
                uint32_t srcPlane = srcPlanes[p];
                uint32_t dstPlane = dstPlanes[p];
                if (!dstPlane || dstPlane == 0xFFFFFFFFu)
                    continue;

                planesAffected++;

                /*
                 * Determine if src/dst overlap on this plane.
                 * If srcPlane==dstPlane and the rectangles overlap, we
                 * need to choose row/column scan direction so that we
                 * read each cell before we overwrite it.
                 */
                int overlap = 0;
                if (srcPlane && srcPlane != 0xFFFFFFFFu && srcPlane == dstPlane) {
                    int sx0=sx, sy0=sy, sx1=sx+aw-1, sy1=sy+ah-1;
                    int dx0=dx, dy0=dy, dx1=dx+aw-1, dy1=dy+ah-1;
                    if (!(sx1 < dx0 || dx1 < sx0 || sy1 < dy0 || dy1 < sy0))
                        overlap = 1;
                }

                int row_start, row_end, row_step;
                if (overlap && dy > sy) {
                    row_start = ah - 1; row_end = -1; row_step = -1;
                } else {
                    row_start = 0; row_end = ah; row_step = 1;
                }

                int col_reverse_default = (overlap && dy == sy && dx > sx);

                /* Bytes per row on the source - we read enough to cover
                 * sx..sx+aw-1 PLUS one extra for the shift carry. */
                int src_byte_start = sx >> 3;
                int src_byte_end_excl = ((sx + aw + 7) >> 3); /* exclusive */
                int src_byte_count = src_byte_end_excl - src_byte_start;
                int dst_byte_start = dx >> 3;
                int dst_byte_end_excl = ((dx + aw + 7) >> 3);
                int dst_byte_count = dst_byte_end_excl - dst_byte_start;

                if (src_byte_count > (int)sizeof(srcRow)) src_byte_count = sizeof(srcRow);
                if (dst_byte_count > (int)sizeof(dstRow)) dst_byte_count = sizeof(dstRow);

                int src_shift = sx & 7;     /* leftmost bit of source rect */
                int dst_shift = dx & 7;     /* leftmost bit of dest rect */

                /*
                 * For each output bit (col in 0..aw-1):
                 *   src bit  = bit at (sx+col, srcY) in source plane
                 *   dst bit  = bit at (dx+col, destY) in dest plane
                 *   new dst bit = ApplyMinterm(src bit, dst bit, minterm)
                 *
                 * We process this byte-by-output-byte, building a window
                 * over the source bytes and shifting on the fly. This is
                 * O(aw/8) per row not O(aw) per row.
                 *
                 * To keep the code straightforward and bug-free for this
                 * first cut, we process bit-by-bit within each row but
                 * read the source/dest rows ONCE up front. That gives us
                 * a ~50x speedup over the m68k per-bit version because
                 * each m68k_read_memory_8/write_memory_8 is much cheaper
                 * here than a full m68k cpu cycle round-trip in the
                 * emulator. Future optimization: unroll into byte-aligned
                 * fast paths for minterm 0xC0.
                 */
                for (int row = row_start; row != row_end; row += row_step) {
                    int srcY = sy + row;
                    int destY = dy + row;
                    int col_reverse = col_reverse_default;

                    /* Read src row bytes. Special-case NULL source plane
                     * (treat as all-0s) and 0xFFFFFFFF source plane (treat
                     * as all-1s) - these encode the GadTools/Image
                     * "planeonoff" feature where a plane is logically
                     * constant rather than backed by memory. */
                    if (!srcPlane) {
                        for (int b = 0; b < src_byte_count; b++) srcRow[b] = 0x00;
                    } else if (srcPlane == 0xFFFFFFFFu) {
                        for (int b = 0; b < src_byte_count; b++) srcRow[b] = 0xFF;
                    } else {
                        uint32_t src_row_addr = srcPlane + (uint32_t)srcY * srcBpr + src_byte_start;
                        for (int b = 0; b < src_byte_count; b++)
                            srcRow[b] = m68k_read_memory_8(src_row_addr + b);
                    }

                    /* Read dst row bytes (we need them for minterm cases
                     * that depend on dest, and for preserving bits we
                     * don't touch within partial-byte edges) */
                    uint32_t dst_row_addr = dstPlane + (uint32_t)destY * dstBpr + dst_byte_start;
                    for (int b = 0; b < dst_byte_count; b++)
                        dstRow[b] = m68k_read_memory_8(dst_row_addr + b);

                    /* Read pixel mask row if present */
                    if (pixelMaskAddr) {
                        int pm_byte_start = sx >> 3;
                        int pm_byte_count = src_byte_count;
                        uint32_t pm_addr = pixelMaskAddr + (uint32_t)srcY * pixelMaskBpr + pm_byte_start;
                        for (int b = 0; b < pm_byte_count && b < (int)sizeof(pmRow); b++)
                            pmRow[b] = m68k_read_memory_8(pm_addr + b);
                    }

                    /* Bit loop */
                    int col_first, col_last, col_step_dir;
                    if (col_reverse) {
                        col_first = aw - 1; col_last = -1; col_step_dir = -1;
                    } else {
                        col_first = 0; col_last = aw; col_step_dir = 1;
                    }

                    for (int col = col_first; col != col_last; col += col_step_dir) {
                        int s_bit_idx = src_shift + col;        /* in srcRow buffer */
                        int d_bit_idx = dst_shift + col;        /* in dstRow buffer */
                        int s_byte = s_bit_idx >> 3;
                        int s_bit  = 7 - (s_bit_idx & 7);
                        int d_byte = d_bit_idx >> 3;
                        int d_bit  = 7 - (d_bit_idx & 7);

                        /* Pixel mask check */
                        if (pixelMaskAddr) {
                            int pm_idx = s_bit_idx;
                            int pm_byte = pm_idx >> 3;
                            int pm_b    = 7 - (pm_idx & 7);
                            if (!((pmRow[pm_byte] >> pm_b) & 1))
                                continue;
                        }

                        uint8_t srcBit = (srcRow[s_byte] >> s_bit) & 1;
                        uint8_t dstBit = (dstRow[d_byte] >> d_bit) & 1;
                        uint8_t newBit = 0;
                        if ((minterm & 0x10) && !srcBit && !dstBit) newBit = 1;
                        if ((minterm & 0x20) && !srcBit &&  dstBit) newBit = 1;
                        if ((minterm & 0x40) &&  srcBit && !dstBit) newBit = 1;
                        if ((minterm & 0x80) &&  srcBit &&  dstBit) newBit = 1;

                        if (newBit)
                            dstRow[d_byte] |=  (uint8_t)(1 << d_bit);
                        else
                            dstRow[d_byte] &= (uint8_t)~(1 << d_bit);
                    }

                    /* Write modified dst row back */
                    for (int b = 0; b < dst_byte_count; b++)
                        m68k_write_memory_8(dst_row_addr + b, dstRow[b]);
                }
            }

            m68k_set_reg(M68K_REG_D0, (uint32_t)planesAffected);
            DPRINTF(LOG_DEBUG, "lxa_dispatch: BltBitMap srcBM=%08x destBM=%08x src=(%d,%d) dst=(%d,%d) size=%dx%d minterm=%02x mask=%02x srcBpr=%d srcRows=%d srcDepth=%d dstBpr=%d dstRows=%d dstDepth=%d aw=%d ah=%d planesAffected=%d\n",
                srcBM, destBM, xSrc, ySrc, xDest, yDest, xSize, ySize,
                (unsigned)minterm, (unsigned)planeMask,
                (int)srcBpr, (int)srcRows, (int)srcDepth,
                (int)dstBpr, (int)dstRows, (int)dstDepth,
                aw, ah, planesAffected);
            break;
        }

        case EMU_CALL_GFX_GET_SIZE:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_t *display = (display_t *)(uintptr_t)d1;
            int width, height, depth;

            display_get_size(display, &width, &height, &depth);

            /* Pack into D0: high word = width, low word = height; D1 = depth */
            m68k_set_reg(M68K_REG_D0, ((uint32_t)width << 16) | (uint32_t)height);
            m68k_set_reg(M68K_REG_D1, (uint32_t)depth);
            break;
        }

        case EMU_CALL_GFX_AVAILABLE:
        {
            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_AVAILABLE\n");
            m68k_set_reg(M68K_REG_D0, display_available() ? 1 : 0);
            break;
        }

        case EMU_CALL_GFX_POLL_EVENTS:
        {
            bool quit = display_poll_events();
            m68k_set_reg(M68K_REG_D0, quit ? 1 : 0);
            if (quit)
            {
                DPRINTF(LOG_INFO, "lxa: display quit requested\n");
                m68k_end_timeslice();
                g_running = FALSE;
            }
            break;
        }

        case EMU_CALL_GFX_TEXT_HOOK:
        {
            /*
             * Phase 130: Text() interception hook.
             * Called from _graphics_Text() before rendering each string.
             * D1 = emulated pointer to raw string bytes (CONST_STRPTR)
             * D2 = character count (UWORD)
             * D3 = x position after layer/baseline adjustment (WORD)
             * D4 = y position after layer/baseline adjustment (WORD)
             *
             * Reads the string bytes from emulated memory and forwards them
             * to the host-side g_text_hook callback if one is registered.
             */
            LPRINTF(LOG_DEBUG, "lxa_dispatch: EMU_CALL_GFX_TEXT_HOOK hook=%p\n", (void*)g_text_hook);
            if (g_text_hook)
            {
                uint32_t str_ptr = m68k_get_reg(NULL, M68K_REG_D1);
                uint32_t count   = m68k_get_reg(NULL, M68K_REG_D2);
                int      x       = (int)(int16_t)m68k_get_reg(NULL, M68K_REG_D3);
                int      y       = (int)(int16_t)m68k_get_reg(NULL, M68K_REG_D4);

                /* Copy string bytes from emulated memory to a host buffer */
                if (str_ptr && count > 0 && count <= 4096)
                {
                    char buf[4097];
                    uint32_t i;
                    for (i = 0; i < count; i++)
                        buf[i] = (char)m68k_read_memory_8(str_ptr + i);
                    buf[count] = '\0';
                    g_text_hook(buf, (int)count, x, y, g_text_hook_userdata);
                }
            }
            break;
        }

        /*
         * Intuition Library emucalls (3000-3999)
         * Phase 13.5: Screen Management
         */

        case EMU_CALL_INT_OPEN_SCREEN:
        {
            /* d1: (width << 16) | height, d2: depth, d3: title_ptr,
             * d4: flags (Phase 147a: bit 0 = is_workbench_screen) */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t width = (d1 >> 16) & 0xFFFF;
            uint32_t height = d1 & 0xFFFF;
            uint32_t depth = d2;
            uint32_t title_ptr = d3;
            bool is_workbench_screen = (d4 & 1u) != 0;

            char title[128] = "LXA Screen";
            if (title_ptr != 0)
            {
                /* Read title string from m68k memory */
                int i;
                for (i = 0; i < 127; i++)
                {
                    char c = (char)m68k_read_memory_8(title_ptr + i);
                    if (c == 0)
                        break;
                    title[i] = c;
                }
                title[i] = 0;
            }

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_INT_OPEN_SCREEN %dx%dx%d '%s' workbench=%d\n",
                    width, height, depth, title, (int)is_workbench_screen);

            /* Initialize display subsystem if not already done */
            display_init();

            /* Phase 147a: in rootless mode the Workbench screen does NOT
             * get its own SDL host window — its windows become native
             * host windows individually (see _window_uses_native_host()
             * in lxa_intuition.c). Custom screens always own a host
             * window in rootless mode. Non-rootless mode always wants
             * a host window. */
            bool wants_host_window = !(is_workbench_screen && config_get_rootless_mode());
            display_t *disp = display_open_ex((int)width, (int)height, (int)depth,
                                              title, wants_host_window);

            /* Phase 131: log screen open event */
            if (disp)
                lxa_push_intui_event(LXA_INTUI_EVENT_OPEN_SCREEN, -1, title,
                                     0, 0, (int)width, (int)height);

            /* Return display handle (pointer cast to uint32_t) */
            m68k_set_reg(M68K_REG_D0, (uint32_t)(uintptr_t)disp);
            break;
        }

        case EMU_CALL_INT_CLOSE_SCREEN:
        {
            /* d1: display_handle */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_t *disp = (display_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_INT_CLOSE_SCREEN handle=0x%08x\n", d1);

            if (disp)
            {
                display_close(disp);
                /* Phase 131: log screen close event */
                lxa_push_intui_event(LXA_INTUI_EVENT_CLOSE_SCREEN, -1, NULL, 0, 0, 0, 0);
            }

            m68k_set_reg(M68K_REG_D0, 1);  /* Success */
            break;
        }

        case EMU_CALL_INT_REFRESH_SCREEN:
        {
            /* d1: display_handle, d2: planes_ptr, d3: (bpr << 16) | depth */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            display_t *disp = (display_t *)(uintptr_t)d1;
            uint32_t planes_ptr = d2;
            uint32_t bpr = (d3 >> 16) & 0xFFFF;
            uint32_t depth = d3 & 0xFFFF;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_INT_REFRESH_SCREEN handle=0x%08x, planes=0x%08x, bpr=%d, depth=%d\n",
                    d1, planes_ptr, bpr, depth);

            if (disp && planes_ptr)
            {
                int w, h, d;
                display_get_size(disp, &w, &h, &d);

                /* Read plane pointers from m68k memory */
                const uint8_t *planes[8] = {0};
                for (uint32_t i = 0; i < depth && i < 8; i++)
                {
                    uint32_t plane_addr = m68k_read_memory_32(planes_ptr + i * 4);
                    if (plane_addr)
                    {
                        planes[i] = (const uint8_t *)&g_ram[plane_addr];
                    }
                }

                /* Update display from planar data */
                display_update_planar(disp, 0, 0, w, h, planes, bpr, depth);

                /* Refresh display */
                display_refresh(disp);
            }

            m68k_set_reg(M68K_REG_D0, 1);  /* Success */
            break;
        }

        case EMU_CALL_INT_SET_SCREEN_BITMAP:
        {
            /* d1: display_handle, d2: planes_ptr (address of BitMap.Planes[] array)
             * d3: (bpr << 16) | depth
             * 
             * This tells the host where the Amiga screen's bitmap lives so that
             * display_refresh_all() can automatically convert planar data to the
             * SDL display at VBlank time.
             */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            display_t *disp = (display_t *)(uintptr_t)d1;
            uint32_t planes_ptr = d2;
            uint32_t bpr = (d3 >> 16) & 0xFFFF;
            uint32_t depth = d3 & 0xFFFF;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_INT_SET_SCREEN_BITMAP handle=0x%08x, planes=0x%08x, bpr=%d, depth=%d\n",
                    d1, planes_ptr, bpr, depth);

            if (disp)
            {
                display_set_amiga_bitmap(disp, planes_ptr, (bpr << 16) | depth);
            }

            m68k_set_reg(M68K_REG_D0, 1);  /* Success */
            break;
        }

        /*
         * Input handling emucalls (Phase 14)
         */
        
        case EMU_CALL_INT_POLL_INPUT:
        {
            /* Poll for next input event
             * Returns event type in D0:
             *   0 = no event
             *   1 = mouse button
             *   2 = mouse move
             *   3 = key
             *   4 = close window
             *   5 = quit
             */
            
            /* First, poll SDL for new events (this populates the event queue) */
            display_poll_events();
            
            display_event_t event;
            bool got_event = display_get_event(&event);
            if (got_event)
            {
                /* Store event data in static vars for subsequent GET calls */
                g_last_event = event;
                m68k_set_reg(M68K_REG_D0, (uint32_t)event.type);
                DPRINTF(LOG_DEBUG, "lxa: POLL_INPUT: got event type=%d btn=0x%02x pos=(%d,%d)\n", event.type, event.button_code, event.mouse_x, event.mouse_y);
            }
            else
            {
                m68k_set_reg(M68K_REG_D0, 0);
            }
            break;
        }

        case EMU_CALL_INT_GET_MOUSE_POS:
        {
            /* Returns (x << 16) | y from the last polled event.
             * We use g_last_event.mouse_x/y rather than display_get_mouse_pos()
             * to ensure the position corresponds to the event that was dequeued,
             * not the current mouse position (which may have changed). */
            int x = g_last_event.mouse_x;
            int y = g_last_event.mouse_y;
            m68k_set_reg(M68K_REG_D0, ((uint32_t)x << 16) | ((uint32_t)y & 0xFFFF));
            break;
        }

        case EMU_CALL_INT_GET_MOUSE_BTN:
        {
            /* Returns button_code | (qualifier << 8) from last event.
             * The ROM extracts: code = result & 0xFF, qualifier = (result >> 8) & 0xFFFF.
             * This matches the packing used by EMU_CALL_INT_GET_KEY. */
            uint32_t result = ((uint32_t)g_last_event.qualifier << 8) |
                             ((uint32_t)g_last_event.button_code & 0xFF);
            m68k_set_reg(M68K_REG_D0, result);
            break;
        }

        case EMU_CALL_INT_GET_KEY:
        {
            /* Returns rawkey | (qualifier << 16) from last event */
            uint32_t result = ((uint32_t)g_last_event.qualifier << 16) | 
                             ((uint32_t)g_last_event.rawkey & 0xFFFF);
            m68k_set_reg(M68K_REG_D0, result);
            break;
        }

        case EMU_CALL_INT_GET_EVENT_WIN:
        {
            /* Returns display handle for window that received the event */
            m68k_set_reg(M68K_REG_D0, (uint32_t)(uintptr_t)g_last_event.window);
            break;
        }

        /*
         * Rootless windowing emucalls (Phase 15)
         */

        case EMU_CALL_INT_OPEN_WINDOW:
        {
            /* d1: screen_handle, d2: (x << 16) | y, d3: (w << 16) | h, d4: title_ptr,
             * d5: flags (bit 0 = uses_native_host) */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t d5 = m68k_get_reg(NULL, M68K_REG_D5);
            display_t *screen = (display_t *)(uintptr_t)d1;
            int x = (int16_t)((d2 >> 16) & 0xFFFF);
            int y = (int16_t)(d2 & 0xFFFF);
            int w = (int16_t)((d3 >> 16) & 0xFFFF);
            int h = (int16_t)(d3 & 0xFFFF);
            uint32_t title_ptr = d4;
            bool uses_native_host = (d5 & 1u) != 0;
            char title[128] = "LXA Window";
            if (title_ptr != 0)
            {
                int i;
                for (i = 0; i < 127; i++)
                {
                    char c = (char)m68k_read_memory_8(title_ptr + i);
                    if (c == 0)
                        break;
                    title[i] = c;
                }
                title[i] = 0;
            }

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_OPEN_WINDOW x=%d, y=%d, w=%d, h=%d native_host=%d '%s'\n",
                    x, y, w, h, (int)uses_native_host, title);

            /* Reject negative or zero window dimensions that result from
             * signed WORD overflow on the ROM side. */
            if (w <= 0 || h <= 0)
            {
                LPRINTF(LOG_WARNING, "lxa: EMU_CALL_INT_OPEN_WINDOW rejected invalid dimensions %dx%d\n", w, h);
                m68k_set_reg(M68K_REG_D0, 0);
                break;
            }

            /* Get depth from screen if available */
            int depth = 2;  /* Default */
            if (screen)
            {
                int sw, sh, sd;
                display_get_size(screen, &sw, &sh, &sd);
                depth = sd;
            }

            display_window_t *win = display_window_open_ex(screen, x, y, w, h, depth, title,
                                                           uses_native_host);
            /* Phase 131: log window open event */
            if (win)
            {
                int win_idx = display_get_window_count() - 1;
                lxa_push_intui_event(LXA_INTUI_EVENT_OPEN_WINDOW, win_idx, title, x, y, w, h);
            }
            m68k_set_reg(M68K_REG_D0, (uint32_t)(uintptr_t)win);
            break;
        }

        case EMU_CALL_INT_CLOSE_WINDOW:
        {
            /* d1: window_handle */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_CLOSE_WINDOW handle=0x%08x\n", d1);

            if (win)
            {
                display_window_close(win);
                /* Phase 131: log window close event */
                lxa_push_intui_event(LXA_INTUI_EVENT_CLOSE_WINDOW, -1, NULL, 0, 0, 0, 0);
            }
            m68k_set_reg(M68K_REG_D0, 1);  /* Success */
            break;
        }

        case EMU_CALL_INT_MOVE_WINDOW:
        {
            /* d1: window_handle, d2: (x << 16) | y */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;
            int x = (int16_t)((d2 >> 16) & 0xFFFF);
            int y = (int16_t)(d2 & 0xFFFF);

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_MOVE_WINDOW handle=0x%08x, x=%d, y=%d\n",
                    d1, x, y);

            bool success = display_window_move(win, x, y);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_SIZE_WINDOW:
        {
            /* d1: window_handle, d2: width, d3: height
             * Called from _intuition_SizeWindow via emucall3(). */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;
            int w = (int)d2;
            int h = (int)d3;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_SIZE_WINDOW handle=0x%08x, w=%d, h=%d\n",
                    d1, w, h);

            bool success = display_window_size(win, w, h);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_WINDOW_TOFRONT:
        {
            /* d1: window_handle */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_WINDOW_TOFRONT handle=0x%08x\n", d1);

            bool success = display_window_to_front(win);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_WINDOW_TOBACK:
        {
            /* d1: window_handle */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_WINDOW_TOBACK handle=0x%08x\n", d1);

            bool success = display_window_to_back(win);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_SET_TITLE:
        {
            /* d1: window_handle, d2: title_ptr */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;
            uint32_t title_ptr = d2;

            char title[128] = "";
            if (title_ptr != 0)
            {
                int i;
                for (i = 0; i < 127; i++)
                {
                    char c = (char)m68k_read_memory_8(title_ptr + i);
                    if (c == 0)
                        break;
                    title[i] = c;
                }
                title[i] = 0;
            }

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_SET_TITLE handle=0x%08x, title='%s'\n",
                    d1, title);

            bool success = display_window_set_title(win, title);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_REFRESH_WINDOW:
        {
            /* d1: window_handle, d2: planes_ptr, d3: (bpr << 16) | depth */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;
            uint32_t planes_ptr = d2;
            uint32_t bpr = (d3 >> 16) & 0xFFFF;
            uint32_t depth = d3 & 0xFFFF;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_REFRESH_WINDOW handle=0x%08x, planes=0x%08x, bpr=%d, depth=%d\n",
                    d1, planes_ptr, bpr, depth);

            if (win && planes_ptr)
            {
                /* Read plane pointers from m68k memory */
                const uint8_t *planes[8] = {0};
                for (uint32_t i = 0; i < depth && i < 8; i++)
                {
                    uint32_t plane_addr = m68k_read_memory_32(planes_ptr + i * 4);
                    if (plane_addr)
                    {
                        planes[i] = (const uint8_t *)&g_ram[plane_addr];
                    }
                }

                /* Get window size - we need to figure out the dimensions somehow */
                /* For now, use bpr * 8 as width estimate and read from window */
                int w = bpr * 8;
                int h = 200;  /* Default, ideally this would come from the window struct */

                display_window_update_planar(win, 0, 0, w, h, planes, bpr, depth);
                display_window_refresh(win);
            }

            m68k_set_reg(M68K_REG_D0, 1);  /* Success */
            break;
        }

        case EMU_CALL_INT_GET_ROOTLESS:
        {
            /* Returns true if rootless mode is enabled */
            bool rootless = display_get_rootless_mode();
            m68k_set_reg(M68K_REG_D0, rootless ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_ATTACH_WINDOW:
        {
            /* d1: window_handle, d2: emulated Window* */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_ATTACH_WINDOW handle=0x%08x, window=0x%08x\n",
                    d1, d2);

            m68k_set_reg(M68K_REG_D0,
                         display_window_attach_amiga_window(win, d2) ? 1 : 0);
            break;
        }

        /*
         * Timer device emucalls (Phase 45)
         * These manage the host-side timer queue for async TR_ADDREQUEST.
         */
        case EMU_CALL_TIMER_ADD:
        {
            /* Add a timer request to the host-side queue.
             * d1 = ioreq pointer (68k)
             * d2 = delay seconds
             * d3 = delay microseconds
             * Returns: d0 = 1 on success, 0 on failure
             */
            uint32_t ioreq = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t secs = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t micros = m68k_get_reg(NULL, M68K_REG_D3);
            
            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_TIMER_ADD ioreq=0x%08x delay=%u.%06u\n",
                    ioreq, secs, micros);
            
            bool success = _timer_add_request(ioreq, secs, micros);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }
        
        case EMU_CALL_TIMER_REMOVE:
        {
            /* Remove a timer request from the queue (for AbortIO).
             * d1 = ioreq pointer (68k)
             * Returns: d0 = 1 if found and removed, 0 if not found
             */
            uint32_t ioreq = m68k_get_reg(NULL, M68K_REG_D1);
            
            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_TIMER_REMOVE ioreq=0x%08x\n", ioreq);
            
            bool success = _timer_remove_request(ioreq);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }
        
        case EMU_CALL_TIMER_CHECK:
        {
            /* Check for expired timers and mark them.
             * This is called from the m68k side (e.g., in VBlank handler).
             * Returns: d0 = number of expired timers marked
             */
            int count = _timer_check_expired();
            m68k_set_reg(M68K_REG_D0, (uint32_t)count);
            break;
        }

        case EMU_CALL_TIMER_GET_EXPIRED:
        {
            /* Get the next expired timer IORequest.
             * Returns: d0 = ioreq pointer (m68k), or 0 if none available
             */
            uint32_t ioreq = _timer_get_expired();
            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_TIMER_GET_EXPIRED -> 0x%08x\n", ioreq);
            m68k_set_reg(M68K_REG_D0, ioreq);
            break;
        }

        case EMU_CALL_AUDIO_PLAY:
        {
            uint32_t packed = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t data_ptr = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t period = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t volume = m68k_get_reg(NULL, M68K_REG_D5);

            _audio_queue_fragment(packed, data_ptr, length, period, volume);
            m68k_set_reg(M68K_REG_D0, 1);
            break;
        }

        /*
         * Console device emucalls
         */
        case EMU_CALL_CON_READ:
        {
            /* Read from console input.
             * d1 = buffer address (68k memory)
             * d2 = max bytes to read
             * Returns: d0 = bytes read, or -1 if no data available yet
             */
            uint32_t buf68k = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t maxlen = m68k_get_reg(NULL, M68K_REG_D2);
            
            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_CON_READ buf=0x%08x maxlen=%d\n", buf68k, maxlen);
            
            /* For now, read from stdin (non-blocking) */
            /* Check if input is available using select() */
            fd_set readfds;
            struct timeval tv = {0, 0};  /* No timeout - non-blocking */
            
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            
            int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
            
            if (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds))
            {
                /* Input is available */
                char buf[256];
                int to_read = (maxlen > sizeof(buf)) ? sizeof(buf) : maxlen;
                ssize_t n = read(STDIN_FILENO, buf, to_read);
                
                if (n > 0)
                {
                    /* Copy to 68k memory, converting LF to CR for Amiga compatibility */
                    for (int i = 0; i < n; i++)
                    {
                        char c = buf[i];
                        if (c == '\n')
                            c = '\r';  /* Convert LF to CR */
                        m68k_write_memory_8(buf68k + i, c);
                    }
                    /* Log what was read */
                    DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_CON_READ -> %zd bytes: 0x%02x '%c'\n", 
                            n, (unsigned char)buf[0], 
                            (buf[0] >= 32 && buf[0] < 127) ? buf[0] : '.');
                    m68k_set_reg(M68K_REG_D0, (uint32_t)n);
                }
                else if (n == 0)
                {
                    /* EOF */
                    m68k_set_reg(M68K_REG_D0, 0);
                }
                else
                {
                    /* Error */
                    m68k_set_reg(M68K_REG_D0, (uint32_t)-1);
                }
            }
            else
            {
                /* No input available */
                m68k_set_reg(M68K_REG_D0, (uint32_t)-1);
            }
            break;
        }

        case EMU_CALL_CON_INPUT_READY:
        {
            /* Check if console input is ready (non-blocking).
             * Returns: d0 = 1 if input ready, 0 if not
             */
            fd_set readfds;
            struct timeval tv = {0, 0};
            
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            
            int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
            
            m68k_set_reg(M68K_REG_D0, (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds)) ? 1 : 0);
            break;
        }

        /*
         * Test Infrastructure emucalls (4100-4199)
         * These provide automated testing capabilities for UI and input handling.
         */
        case EMU_CALL_TEST_INJECT_KEY:
        {
            /* Inject a keyboard event into the event queue.
             * Input:  d1 = rawkey code
             *         d2 = qualifier bits
             *         d3 = key down (1) or up (0)
             * Output: d0 = 1 on success, 0 on failure
             */
            uint32_t rawkey = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t qualifier = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t down = m68k_get_reg(NULL, M68K_REG_D3);
            
            bool result = display_inject_key((int)rawkey, (int)qualifier, down != 0);
            m68k_set_reg(M68K_REG_D0, result ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_INJECT_STRING:
        {
            /* Inject a string as a sequence of key events.
             * Input:  a0 = pointer to null-terminated string
             * Output: d0 = 1 on success, 0 on failure
             */
            uint32_t str_ptr = m68k_get_reg(NULL, M68K_REG_A0);
            char str[1024];
            
            /* Copy string from 68k memory */
            int i;
            for (i = 0; i < (int)sizeof(str) - 1; i++)
            {
                char c = (char)m68k_read_memory_8(str_ptr + i);
                if (c == '\0')
                    break;
                str[i] = c;
            }
            str[i] = '\0';
            
            bool result = display_inject_string(str);
            m68k_set_reg(M68K_REG_D0, result ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_INJECT_MOUSE:
        {
            /* Inject a mouse event into the event queue.
             * Input:  d1 = (x << 16) | y
             *         d2 = button state (bit 0=left, bit 1=right, bit 2=middle)
             *         d3 = event type (DISPLAY_EVENT_MOUSEMOVE=2, DISPLAY_EVENT_MOUSEBUTTON=3)
             * Output: d0 = 1 on success, 0 on failure
             */
            uint32_t pos = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t buttons = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t event_type = m68k_get_reg(NULL, M68K_REG_D3);
            
            int x = (int)(pos >> 16);
            int y = (int)(pos & 0xFFFF);
            
            bool result = display_inject_mouse(x, y, (int)buttons, (display_event_type_t)event_type);
            m68k_set_reg(M68K_REG_D0, result ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_CAPTURE_SCREEN:
        {
            /* Capture the display to a file.
             * Input:  a0 = pointer to filename string
             *         d1 = display handle (0 for default)
             * Output: d0 = 1 on success, 0 on failure
             */
            uint32_t filename_ptr = m68k_get_reg(NULL, M68K_REG_A0);
            /* uint32_t display_handle = m68k_get_reg(NULL, M68K_REG_D1); */
            char filename[256];
            
            /* Copy filename from 68k memory */
            int i;
            for (i = 0; i < (int)sizeof(filename) - 1; i++)
            {
                char c = (char)m68k_read_memory_8(filename_ptr + i);
                if (c == '\0')
                    break;
                filename[i] = c;
            }
            filename[i] = '\0';
            
            /* Use default display for now */
            bool result = display_capture_screen(NULL, filename);
            m68k_set_reg(M68K_REG_D0, result ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_CAPTURE_WINDOW:
        {
            /* Capture a window to a file.
             * Input:  a0 = pointer to filename string
             *         d1 = window handle
             * Output: d0 = 1 on success, 0 on failure
             */
            uint32_t filename_ptr = m68k_get_reg(NULL, M68K_REG_A0);
            uint32_t window_handle = m68k_get_reg(NULL, M68K_REG_D1);
            char filename[256];
            
            /* Copy filename from 68k memory */
            int i;
            for (i = 0; i < (int)sizeof(filename) - 1; i++)
            {
                char c = (char)m68k_read_memory_8(filename_ptr + i);
                if (c == '\0')
                    break;
                filename[i] = c;
            }
            filename[i] = '\0';
            
            bool result = display_capture_window((display_window_t *)(uintptr_t)window_handle, filename);
            m68k_set_reg(M68K_REG_D0, result ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_COMPARE_SCREEN:
        {
            /* Compare screen to a reference image.
             * Input:  a0 = pointer to reference filename
             * Output: d0 = similarity percentage (0-100), or -1 on error
             */
            uint32_t filename_ptr = m68k_get_reg(NULL, M68K_REG_A0);
            char filename[256];
            
            /* Copy filename from 68k memory */
            int i;
            for (i = 0; i < (int)sizeof(filename) - 1; i++)
            {
                char c = (char)m68k_read_memory_8(filename_ptr + i);
                if (c == '\0')
                    break;
                filename[i] = c;
            }
            filename[i] = '\0';
            
            int similarity = display_compare_to_reference(filename);
            m68k_set_reg(M68K_REG_D0, (uint32_t)similarity);
            break;
        }

        /* Phase 39b: Validation emucalls */
        case 4113:  /* EMU_CALL_TEST_GET_SCREEN_DIMS */
        {
            /* Get active screen dimensions.
             * Output: d0 = (depth << 16) | 1 on success, 0 on failure
             *         d1 = (width << 16) | height
             */
            int width, height, depth;
            if (display_get_active_dimensions(&width, &height, &depth))
            {
                m68k_set_reg(M68K_REG_D0, ((uint32_t)depth << 16) | 1);
                m68k_set_reg(M68K_REG_D1, ((uint32_t)width << 16) | (uint16_t)height);
            }
            else
            {
                m68k_set_reg(M68K_REG_D0, 0);
                m68k_set_reg(M68K_REG_D1, 0);
            }
            break;
        }

        case 4114:  /* EMU_CALL_TEST_GET_CONTENT */
        {
            /* Get number of non-background pixels on active screen.
             * Output: d0 = pixel count, or -1 if no screen
             */
            int content = display_get_content_pixels();
            m68k_set_reg(M68K_REG_D0, (uint32_t)content);
            break;
        }

        case 4115:  /* EMU_CALL_TEST_GET_REGION */
        {
            /* Get non-background pixels in a screen region.
             * Input:  d1 = (x << 16) | y
             *         d2 = (width << 16) | height
             * Output: d0 = pixel count, or -1 on error
             */
            uint32_t pos = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t dims = m68k_get_reg(NULL, M68K_REG_D2);
            int x = (int16_t)((pos >> 16) & 0xFFFF);
            int y = (int16_t)(pos & 0xFFFF);
            int width = (int16_t)((dims >> 16) & 0xFFFF);
            int height = (int16_t)(dims & 0xFFFF);
            
            int content = display_get_region_content(x, y, width, height);
            m68k_set_reg(M68K_REG_D0, (uint32_t)content);
            break;
        }

        case 4116:  /* EMU_CALL_TEST_GET_WIN_COUNT */
        {
            /* Get number of open windows.
             * Output: d0 = window count
             */
            int count = display_get_window_count();
            m68k_set_reg(M68K_REG_D0, (uint32_t)count);
            break;
        }

        case 4117:  /* EMU_CALL_TEST_GET_WIN_DIMS */
        {
            /* Get window dimensions by index.
             * Input:  d1 = window index
             * Output: d0 = 1 on success, 0 on failure
             *         d1 = (width << 16) | height
             */
            int index = (int)m68k_get_reg(NULL, M68K_REG_D1);
            int width, height;
            
            if (display_get_window_dimensions(index, &width, &height))
            {
                m68k_set_reg(M68K_REG_D0, 1);
                m68k_set_reg(M68K_REG_D1, ((uint32_t)width << 16) | (uint16_t)height);
            }
            else
            {
                m68k_set_reg(M68K_REG_D0, 0);
                m68k_set_reg(M68K_REG_D1, 0);
            }
            break;
        }

        case 4118:  /* EMU_CALL_TEST_GET_WIN_CONTENT */
        {
            /* Get non-background pixels in a window.
             * Input:  d1 = window index
             * Output: d0 = pixel count, or -1 on error
             */
            int index = (int)m68k_get_reg(NULL, M68K_REG_D1);
            int content = display_get_window_content(index);
            m68k_set_reg(M68K_REG_D0, (uint32_t)content);
            break;
        }

        case EMU_CALL_TEST_SET_HEADLESS:
        {
            /* Set headless mode (no window rendering).
             * Input:  d1 = enable (1) or disable (0)
             * Output: d0 = previous headless state
             */
            uint32_t enable = m68k_get_reg(NULL, M68K_REG_D1);
            
            bool previous = display_set_headless(enable != 0);
            m68k_set_reg(M68K_REG_D0, previous ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_GET_HEADLESS:
        {
            /* Get current headless mode state.
             * Output: d0 = 1 if headless, 0 if not
             */
            bool is_headless = display_get_headless();
            m68k_set_reg(M68K_REG_D0, is_headless ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_WAIT_IDLE:
        {
            /* Wait for event queue to be empty.
             * Input:  d1 = timeout in milliseconds (0 = just check, don't wait)
             * Output: d0 = 1 if queue is empty, 0 if timeout
             */
            uint32_t timeout_ms = m68k_get_reg(NULL, M68K_REG_D1);
            
            if (timeout_ms == 0)
            {
                /* Just check, don't wait */
                m68k_set_reg(M68K_REG_D0, display_event_queue_empty() ? 1 : 0);
            }
            else
            {
                /* Wait with timeout using gettimeofday */
                struct timeval start_tv, now_tv;
                gettimeofday(&start_tv, NULL);
                
                while (!display_event_queue_empty())
                {
                    gettimeofday(&now_tv, NULL);
                    uint32_t elapsed_ms = (uint32_t)((now_tv.tv_sec - start_tv.tv_sec) * 1000 +
                                                     (now_tv.tv_usec - start_tv.tv_usec) / 1000);
                    if (elapsed_ms >= timeout_ms)
                    {
                        m68k_set_reg(M68K_REG_D0, 0);  /* Timeout */
                        break;
                    }
                    usleep(1000);  /* 1ms delay to avoid busy-waiting */
                }
                if (display_event_queue_empty())
                {
                    m68k_set_reg(M68K_REG_D0, 1);  /* Success */
                }
            }
            break;
        }

        /*
         * Phase 61: IEEE Double Precision Math emucalls (5000-5011)
         * 
         * These handlers bridge the m68k mathieeedoubbas.library to host native double.
         * Double values are passed as two 32-bit values (d1=hi, d2=lo).
         * For two-operand functions: d1:d2=left, d3:d4=right.
         * Results that are doubles are returned in d0:d1 (hi:lo).
         */
        
        case EMU_CALL_IEEEDP_FIX:
        {
            /* IEEEDPFix: Convert double to integer (truncate toward zero)
             * Input:  d1:d2 = IEEE double (hi:lo)
             * Output: d0 = LONG integer
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            int32_t result = clamp_double_to_long(value);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_FIX(%f) = %d\n", value, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }
        
        case EMU_CALL_IEEEDP_FLT:
        {
            /* IEEEDPFlt: Convert integer to double
             * Input:  d1 = LONG integer
             * Output: d0:d1 = IEEE double (hi:lo)
             */
            int32_t integer = (int32_t)m68k_get_reg(NULL, M68K_REG_D1);
            
            double value = (double)integer;
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_FLT(%d) = %f\n", integer, value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_CMP:
        {
            /* IEEEDPCmp: Compare two doubles
             * Input:  d1:d2 = left, d3:d4 = right
             * Output: d0 = -1 (left < right), 0 (equal), +1 (left > right)
             */
            double left = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double right = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            int32_t result = compare_double_values(left, right);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_CMP(%f, %f) = %d\n", left, right, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }
        
        case EMU_CALL_IEEEDP_TST:
        {
            /* IEEEDPTst: Test double against zero
             * Input:  d1:d2 = IEEE double
             * Output: d0 = -1 (negative), 0 (zero), +1 (positive)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            int32_t result = compare_double_values(value, 0.0);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_TST(%f) = %d\n", value, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }
        
        case EMU_CALL_IEEEDP_ABS:
        {
            /* IEEEDPAbs: Absolute value
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = |input|
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = fabs(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_ABS -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_NEG:
        {
            /* IEEEDPNeg: Negate
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = -input
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = -value;
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_NEG -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_ADD:
        {
            /* IEEEDPAdd: Addition
             * Input:  d1:d2 = left, d3:d4 = right
             * Output: d0:d1 = left + right
             */
            double left = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double right = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            double result = left + right;
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_ADD(%f, %f) = %f\n", left, right, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_SUB:
        {
            /* IEEEDPSub: Subtraction
             * Input:  d1:d2 = left, d3:d4 = right
             * Output: d0:d1 = left - right
             */
            double left = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double right = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            double result = left - right;
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_SUB(%f, %f) = %f\n", left, right, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_MUL:
        {
            /* IEEEDPMul: Multiplication
             * Input:  d1:d2 = factor1, d3:d4 = factor2
             * Output: d0:d1 = factor1 * factor2
             */
            double f1 = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double f2 = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            double result = f1 * f2;
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_MUL(%f, %f) = %f\n", f1, f2, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_DIV:
        {
            /* IEEEDPDiv: Division
             * Input:  d1:d2 = dividend, d3:d4 = divisor
             * Output: d0:d1 = dividend / divisor
             */
            double dividend = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double divisor = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            double result = host_safe_double_divide(dividend, divisor);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_DIV(%f, %f) = %f\n", dividend, divisor, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_FLOOR:
        {
            /* IEEEDPFloor: Largest integer not greater than input
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = floor(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = floor(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_FLOOR -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_CEIL:
        {
            /* IEEEDPCeil: Smallest integer not less than input
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = ceil(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = ceil(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_CEIL -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }

        /*
         * Phase 77: IEEE Single Precision Basic Math (5100-5111)
         * mathieeesingbas.library handlers
         *
         * Single-precision floats are 32-bit IEEE 754, passed in one register.
         * One-operand: d1=float -> d0=result
         * Two-operand: d1=left, d2=right -> d0=result
         */

        case EMU_CALL_IEEESP_FIX:
        {
            /* IEEESPFix: Convert single float to integer (truncate toward zero)
             * Input:  d1 = IEEE single float
             * Output: d0 = LONG integer
             */
            float value = host_float_from_m68k_register(M68K_REG_D1);
            int32_t result = clamp_float_to_long(value);

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_FIX(%f) = %d\n", value, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }

        case EMU_CALL_IEEESP_FLT:
        {
            /* IEEESPFlt: Convert integer to single float
             * Input:  d1 = LONG integer
             * Output: d0 = IEEE single float
             */
            int32_t integer = (int32_t)m68k_get_reg(NULL, M68K_REG_D1);

            float value = (float)integer;

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_FLT(%d) = %f\n", integer, value);
            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_CMP:
        {
            /* IEEESPCmp: Compare two single floats
             * Input:  d1 = left, d2 = right
             * Output: d0 = -1 (left < right), 0 (equal), +1 (left > right)
             */
            float left = host_float_from_m68k_register(M68K_REG_D1);
            float right = host_float_from_m68k_register(M68K_REG_D2);
            int32_t result = compare_float_values(left, right);

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_CMP(%f, %f) = %d\n", left, right, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }

        case EMU_CALL_IEEESP_TST:
        {
            /* IEEESPTst: Test single float against zero
             * Input:  d1 = IEEE single float
             * Output: d0 = -1 (negative), 0 (zero), +1 (positive)
             */
            float value = host_float_from_m68k_register(M68K_REG_D1);
            int32_t result = compare_float_values(value, 0.0f);

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_TST(%f) = %d\n", value, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }

        case EMU_CALL_IEEESP_ABS:
        {
            /* IEEESPAbs: Absolute value
             * Input:  d1 = IEEE single float
             * Output: d0 = |input|
             */
            float value = fabsf(host_float_from_m68k_register(M68K_REG_D1));

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_ABS -> %f\n", value);
            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_NEG:
        {
            /* IEEESPNeg: Negate
             * Input:  d1 = IEEE single float
             * Output: d0 = -input
             */
            float value = -host_float_from_m68k_register(M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_NEG -> %f\n", value);
            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_ADD:
        {
            /* IEEESPAdd: Addition
             * Input:  d1 = left, d2 = right
             * Output: d0 = left + right
             */
            float left = host_float_from_m68k_register(M68K_REG_D1);
            float right = host_float_from_m68k_register(M68K_REG_D2);
            float result = left + right;

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_ADD(%f, %f) = %f\n", left, right, result);
            host_float_to_m68k_register(result, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_SUB:
        {
            /* IEEESPSub: Subtraction
             * Input:  d1 = left, d2 = right
             * Output: d0 = left - right
             */
            float left = host_float_from_m68k_register(M68K_REG_D1);
            float right = host_float_from_m68k_register(M68K_REG_D2);
            float result = left - right;

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_SUB(%f, %f) = %f\n", left, right, result);
            host_float_to_m68k_register(result, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_MUL:
        {
            /* IEEESPMul: Multiplication
             * Input:  d1 = factor1, d2 = factor2
             * Output: d0 = factor1 * factor2
             */
            float f1 = host_float_from_m68k_register(M68K_REG_D1);
            float f2 = host_float_from_m68k_register(M68K_REG_D2);
            float result = f1 * f2;

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_MUL(%f, %f) = %f\n", f1, f2, result);
            host_float_to_m68k_register(result, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_DIV:
        {
            /* IEEESPDiv: Division
             * Input:  d1 = dividend, d2 = divisor
             * Output: d0 = dividend / divisor
             */
            float dividend = host_float_from_m68k_register(M68K_REG_D1);
            float divisor = host_float_from_m68k_register(M68K_REG_D2);
            float result = host_safe_float_divide(dividend, divisor);

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_DIV(%f, %f) = %f\n", dividend, divisor, result);
            host_float_to_m68k_register(result, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_FLOOR:
        {
            /* IEEESPFloor: Largest integer not greater than input
             * Input:  d1 = IEEE single float
             * Output: d0 = floor(input) as IEEE single float
             */
            float value = floorf(host_float_from_m68k_register(M68K_REG_D1));

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_FLOOR -> %f\n", value);
            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_CEIL:
        {
            /* IEEESPCeil: Smallest integer not less than input
             * Input:  d1 = IEEE single float
             * Output: d0 = ceil(input) as IEEE single float
             */
            float value = ceilf(host_float_from_m68k_register(M68K_REG_D1));

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_CEIL -> %f\n", value);
            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_FFP_ATAN:
        case EMU_CALL_FFP_SIN:
        case EMU_CALL_FFP_COS:
        case EMU_CALL_FFP_TAN:
        case EMU_CALL_FFP_SINH:
        case EMU_CALL_FFP_COSH:
        case EMU_CALL_FFP_TANH:
        case EMU_CALL_FFP_EXP:
        case EMU_CALL_FFP_LOG:
        case EMU_CALL_FFP_SQRT:
        case EMU_CALL_FFP_ASIN:
        case EMU_CALL_FFP_ACOS:
        case EMU_CALL_FFP_LOG10:
        case EMU_CALL_FFP_FLOOR:
        case EMU_CALL_FFP_CEIL:
        {
            uint32_t raw = m68k_get_reg(NULL, M68K_REG_D1);
            float input = ffp_to_host_float(raw);
            float result = 0.0f;

            switch (d0)
            {
                case EMU_CALL_FFP_ATAN:  result = atanf(input); break;
                case EMU_CALL_FFP_SIN:   result = sinf(input); break;
                case EMU_CALL_FFP_COS:   result = cosf(input); break;
                case EMU_CALL_FFP_TAN:   result = tanf(input); break;
                case EMU_CALL_FFP_SINH:  result = sinhf(input); break;
                case EMU_CALL_FFP_COSH:  result = coshf(input); break;
                case EMU_CALL_FFP_TANH:  result = tanhf(input); break;
                case EMU_CALL_FFP_EXP:   result = expf(input); break;
                case EMU_CALL_FFP_LOG:   result = logf(input); break;
                case EMU_CALL_FFP_SQRT:  result = sqrtf(input); break;
                case EMU_CALL_FFP_ASIN:  result = asinf(input); break;
                case EMU_CALL_FFP_ACOS:  result = acosf(input); break;
                case EMU_CALL_FFP_LOG10: result = log10f(input); break;
                case EMU_CALL_FFP_FLOOR: result = floorf(input); break;
                case EMU_CALL_FFP_CEIL:  result = ceilf(input); break;
            }

            m68k_set_reg(M68K_REG_D0, host_float_to_ffp(result));
            break;
        }

        case EMU_CALL_FFP_SINCOS:
        {
            uint32_t raw = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t cos_ptr = m68k_get_reg(NULL, M68K_REG_D2);
            float input = ffp_to_host_float(raw);
            float sin_result = sinf(input);
            float cos_result = cosf(input);

            if (cos_ptr != 0)
            {
                m68k_write_memory_32(cos_ptr, host_float_to_ffp(cos_result));
            }

            m68k_set_reg(M68K_REG_D0, host_float_to_ffp(sin_result));
            break;
        }

        case EMU_CALL_FFP_POW:
        {
            float base = ffp_to_host_float(m68k_get_reg(NULL, M68K_REG_D1));
            float power = ffp_to_host_float(m68k_get_reg(NULL, M68K_REG_D2));
            float result = powf(base, power);

            m68k_set_reg(M68K_REG_D0, host_float_to_ffp(result));
            break;
        }

        case EMU_CALL_FFP_TIEEE:
        {
            float value = ffp_to_host_float(m68k_get_reg(NULL, M68K_REG_D1));

            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_FFP_FIEEE:
        {
            float value = host_float_from_m68k_register(M68K_REG_D1);

            m68k_set_reg(M68K_REG_D0, host_float_to_ffp(value));
            break;
        }

        /*
         * Phase 61: IEEE Double Precision Transcendental Math (5020-5039)
         * mathieeedoubtrans.library handlers
         */
        
        case EMU_CALL_IEEEDP_ATAN:
        {
            /* IEEEDPAtan: Arc tangent
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = atan(input) in radians
             */
            double input = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double result = atan(input);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_ATAN(%f) -> %f\n", input, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_SIN:
        {
            /* IEEEDPSin: Sine
             * Input:  d1:d2 = IEEE double (radians)
             * Output: d0:d1 = sin(input)
             */
            double input = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double result = sin(input);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_SIN -> %f\n", result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_COS:
        {
            /* IEEEDPCos: Cosine
             * Input:  d1:d2 = IEEE double (radians)
             * Output: d0:d1 = cos(input)
             */
            double input = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double result = cos(input);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_COS -> %f\n", result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_TAN:
        {
            /* IEEEDPTan: Tangent
             * Input:  d1:d2 = IEEE double (radians)
             * Output: d0:d1 = tan(input)
             */
            double input = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double result = tan(input);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_TAN -> %f\n", result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_SINCOS:
        {
            /* IEEEDPSincos: Sine and Cosine
             * Input:  d1:d2 = IEEE double (radians), a0 = pointer to store cos
             * Output: d0:d1 = sin(input), *a0 = cos(input)
             */
            double input = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double sin_result = sin(input);
            double cos_result = cos(input);
            uint32_t cos_ptr = m68k_get_reg(NULL, M68K_REG_A0);
            host_double_to_m68k_memory(cos_ptr, cos_result);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_SINCOS -> sin=%f, cos=%f\n", sin_result, cos_result);
            host_double_to_m68k_registers(sin_result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_SINH:
        {
            /* IEEEDPSinh: Hyperbolic sine
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = sinh(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = sinh(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_SINH -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_COSH:
        {
            /* IEEEDPCosh: Hyperbolic cosine
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = cosh(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = cosh(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_COSH -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_TANH:
        {
            /* IEEEDPTanh: Hyperbolic tangent
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = tanh(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = tanh(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_TANH -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_EXP:
        {
            /* IEEEDPExp: Natural exponential (e^x)
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = exp(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = exp(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_EXP -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_LOG:
        {
            /* IEEEDPLog: Natural logarithm
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = log(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = log(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_LOG -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_POW:
        {
            /* IEEEDPPow: Power function (arg^exp)
             * Input:  d1:d2 = arg (base), d3:d4 = exp (exponent)
             * Output: d0:d1 = arg^exp
             */
            double base = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double exponent = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            double result = pow(base, exponent);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_POW(%f, %f) = %f\n", base, exponent, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_SQRT:
        {
            /* IEEEDPSqrt: Square root
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = sqrt(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = sqrt(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_SQRT -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_TIEEE:
        {
            /* IEEEDPTieee: Convert double to IEEE single
             * Input:  d1:d2 = IEEE double
             * Output: d0 = IEEE single
             */
            double dval = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_TIEEE(%f) -> single\n", dval);
            host_float_to_m68k_register((float)dval, M68K_REG_D0);
            break;
        }
        
        case EMU_CALL_IEEEDP_FIEEE:
        {
            /* IEEEDPFieee: Convert IEEE single to double
             * Input:  d1 = IEEE single
             * Output: d0:d1 = IEEE double
             */
            double dval = (double)host_float_from_m68k_register(M68K_REG_D1);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_FIEEE(single) -> %f\n", dval);
            host_double_to_m68k_registers(dval, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_ASIN:
        {
            /* IEEEDPAsin: Arc sine
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = asin(input) in radians
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = asin(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_ASIN -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_ACOS:
        {
            /* IEEEDPAcos: Arc cosine
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = acos(input) in radians
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = acos(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_ACOS -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_LOG10:
        {
            /* IEEEDPLog10: Base-10 logarithm
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = log10(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = log10(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_LOG10 -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }

        default:
        {
            /*
             * Undefined EMU_CALL - this happens when the PC jumps to
             * invalid/corrupt code. This typically occurs during the main
             * process's libnix exit sequence when stack corruption happens
             * due to multitasking timing issues.
             * 
             * When this happens during normal program exit (after the main
             * task finishes its work), we should exit gracefully rather than
             * crashing. The return value has already been set by g_rv from
             * a previous EMU_CALL_EXIT or EMU_CALL_STOP.
             */
            DPRINTF (LOG_WARNING, "*** undefined EMU_CALL #%d (corrupt PC at 0x%08x)\n", 
                     d0, m68k_get_reg(NULL, M68K_REG_PC));
            
            /*
             * Just exit the emulator. Any child tasks will be terminated
             * along with the emulator. This is acceptable because:
             * 1. The main task has already finished its work
             * 2. Child tasks that didn't get to cleanup are acceptable loss
             * 3. This prevents hangs from orphaned child tasks
             */
            m68k_end_timeslice();
            g_running = FALSE;
            break;
        }
    }

#ifdef PROFILE_BUILD
    clock_gettime(CLOCK_MONOTONIC, &_prof_end);
    if (d0 < LXA_PROFILE_MAX_EMUCALL)
    {
        g_profile_calls[d0]++;
        g_profile_ns[d0] += (uint64_t)(_prof_end.tv_sec  - _prof_start.tv_sec)  * 1000000000ULL
                           + (uint64_t)(_prof_end.tv_nsec - _prof_start.tv_nsec);
    }
#endif

    return M68K_INT_ACK_AUTOVECTOR;
}

#define MAX_JITTER 1024

static float ffp_to_host_float(uint32_t raw)
{
    uint32_t mantissa;
    int exponent;
    float value;

    if (raw == 0)
    {
        return 0.0f;
    }

    mantissa = raw >> 8;
    exponent = (int)(raw & 0x7f) - 64;
    value = ldexpf((float)mantissa / 16777216.0f, exponent);

    if (raw & 0x80)
    {
        value = -value;
    }

    return value;
}

static float host_float_from_bits(uint32_t bits)
{
    union { float f; uint32_t u; } value;

    value.u = bits;
    return value.f;
}

static uint32_t host_float_to_bits(float value)
{
    union { float f; uint32_t u; } bits;

    bits.f = value;
    return bits.u;
}

static float host_float_from_m68k_register(int reg)
{
    return host_float_from_bits(m68k_get_reg(NULL, reg));
}

static void host_float_to_m68k_register(float value, int reg)
{
    m68k_set_reg(reg, host_float_to_bits(value));
}

static double host_double_from_words(uint32_t hi, uint32_t lo)
{
    uint64_t bits = ((uint64_t)hi << 32) | (uint64_t)lo;
    double value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void host_double_to_words(double value, uint32_t *hi, uint32_t *lo)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    *hi = (uint32_t)(bits >> 32);
    *lo = (uint32_t)bits;
}

static double host_double_from_m68k_registers(int hi_reg, int lo_reg)
{
    return host_double_from_words(m68k_get_reg(NULL, hi_reg), m68k_get_reg(NULL, lo_reg));
}

static void host_double_to_m68k_registers(double value, int hi_reg, int lo_reg)
{
    uint32_t hi;
    uint32_t lo;

    host_double_to_words(value, &hi, &lo);
    m68k_set_reg(hi_reg, hi);
    m68k_set_reg(lo_reg, lo);
}

static void host_double_to_m68k_memory(uint32_t addr, double value)
{
    uint32_t hi;
    uint32_t lo;

    if (addr == 0)
    {
        return;
    }

    host_double_to_words(value, &hi, &lo);
    m68k_write_memory_32(addr, hi);
    m68k_write_memory_32(addr + 4, lo);
}

static int32_t clamp_double_to_long(double value)
{
    if (value > 2147483647.0)
    {
        return 0x7FFFFFFF;
    }
    if (value < -2147483648.0)
    {
        return (int32_t)0x80000000;
    }

    return (int32_t)value;
}

static int32_t clamp_float_to_long(float value)
{
    if (value > 2147483647.0f)
    {
        return 0x7FFFFFFF;
    }
    if (value < -2147483648.0f)
    {
        return (int32_t)0x80000000;
    }

    return (int32_t)value;
}

static int32_t compare_double_values(double left, double right)
{
    if (left < right)
    {
        return -1;
    }
    if (left > right)
    {
        return 1;
    }

    return 0;
}

static int32_t compare_float_values(float left, float right)
{
    if (left < right)
    {
        return -1;
    }
    if (left > right)
    {
        return 1;
    }

    return 0;
}

static double host_safe_double_divide(double dividend, double divisor)
{
    if (divisor == 0.0)
    {
        return (signbit(dividend) != signbit(divisor)) ? -HUGE_VAL : HUGE_VAL;
    }

    return dividend / divisor;
}

static float host_safe_float_divide(float dividend, float divisor)
{
    if (divisor == 0.0f)
    {
        return (dividend >= 0.0f) ? HUGE_VALF : -HUGE_VALF;
    }

    return dividend / divisor;
}

static uint32_t host_float_to_ffp(float value)
{
    float mantissa;
    int exponent;
    uint32_t sign = 0;
    uint32_t mantissa_bits;
    int encoded_exponent;

    if (value == 0.0f)
    {
        return 0;
    }

    if (isnan(value))
    {
        return 0;
    }

    if (signbit(value))
    {
        sign = 0x80;
        value = -value;
    }

    if (isinf(value))
    {
        return sign | 0xffffff7fU;
    }

    mantissa = frexpf(value, &exponent);
    mantissa_bits = (uint32_t)lrintf(mantissa * 16777216.0f);

    if (mantissa_bits >= 0x1000000U)
    {
        mantissa_bits >>= 1;
        exponent++;
    }

    encoded_exponent = exponent + 64;
    if (encoded_exponent <= 0)
    {
        return 0;
    }
    if (encoded_exponent > 0x7f)
    {
        return sign | 0xffffff7fU;
    }

    return (mantissa_bits << 8) | sign | (uint32_t)encoded_exponent;
}

