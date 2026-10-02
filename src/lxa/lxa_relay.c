/*
 * lxa_relay.c - relay trace of library calls (Phase 233)
 *
 * Detection works on the jump tables: an instruction fetched at
 * base - n*6 of a traced library is a call of LVO -n*6.  The return is the
 * first instruction at the pushed return address with the caller's stack
 * pointer restored (SP = entry SP + 4).  Libraries are re-read from
 * ExecBase->LibList periodically and after every OpenLibrary(), so disk
 * libraries opened later are seen.  Calls whose return address lies in the
 * ROM are lxa-internal and not logged.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lxa_relay.h"
#include "lxa_memory.h"
#include "m68k.h"

bool g_relay_active = false;

#define MAX_SPECS   32
#define MAX_LVOS    128
#define MAX_LIBS    96
#define MAX_PENDING 512

#define EXECBASE_THISTASK 276
#define EXECBASE_LIBLIST  378
#define LIB_NEGSIZE       16
#define NODE_NAME         10

typedef struct {
    char    name[48];
    bool    all;
    int     nlvo;
    int16_t lvo[MAX_LVOS];
} relay_spec_t;

typedef struct {
    uint32_t base, lo;      /* jump table = [lo, base) */
    int      spec;
    char     name[48];
} relay_lib_t;

typedef struct {
    uint32_t ret_pc, sp;
    int16_t  lvo;
    int      spec;
} relay_pending_t;

static relay_spec_t    s_specs[MAX_SPECS];
static int             s_nspecs;
static relay_lib_t     s_libs[MAX_LIBS];
static int             s_nlibs;
static uint32_t        s_lo = 0xffffffff, s_hi;
static relay_pending_t s_pend[MAX_PENDING];
static int             s_npend;
static FILE           *s_out;
/* fault injection (LXA_TRACE_INJECT="lib:lvo:delta"): the return value of
 * every call of lib/lvo is offset by delta - for testing tracediff */
static char            s_inj_lib[48];
static int             s_inj_lvo;
static int32_t         s_inj_delta;
static uint32_t        s_countdown;

static uint32_t rd32(uint32_t a) { return ((uint32_t)mread8(a) << 24) | ((uint32_t)mread8(a + 1) << 16) |
                                          ((uint32_t)mread8(a + 2) << 8) | mread8(a + 3); }
static uint16_t rd16(uint32_t a) { return (uint16_t)((mread8(a) << 8) | mread8(a + 1)); }

static bool valid_addr(uint32_t a)
{
    return (a >= 0x400 && a <= RAM_END) || (a >= ROM_START && a <= ROM_END);
}

static void read_cstr(uint32_t addr, char *dst, int max)
{
    int i;
    dst[0] = 0;
    if (!valid_addr(addr))
        return;
    for (i = 0; i < max - 1; i++) {
        uint8_t c = mread8(addr + i);
        if (!c)
            break;
        dst[i] = (char)c;
    }
    dst[i] = 0;
}

/* printable C string (same rule as the reference agent's trace_str) */
static void trace_str(uint32_t addr, char *dst)
{
    int i;
    dst[0] = 0;
    if (!valid_addr(addr))
        return;
    for (i = 0; i < 39; i++) {
        uint8_t c = mread8(addr + i);
        if (!c)
            break;
        if (c < 0x20 || (c >= 0x7f && c < 0xa0)) {
            dst[0] = 0;
            return;
        }
        dst[i] = (char)c;
    }
    dst[i] = 0;
}

static void json_str(const char *s)
{
    fputc('"', s_out);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\')
            fprintf(s_out, "\\%c", c);
        else if (c >= 0x80)
            fprintf(s_out, "\\u%04x", c);   /* latin-1 */
        else
            fputc(c, s_out);
    }
    fputc('"', s_out);
}

static void refresh_libs(void)
{
    uint32_t sysbase = rd32(4), node;
    int guard = 0;
    s_nlibs = 0;
    s_lo = 0xffffffff;
    s_hi = 0;
    /* exec not up yet unless ExecBase->ChkBase (offset 38) == ~SysBase */
    if (!valid_addr(sysbase) || rd32(sysbase + 38) != ~sysbase)
        return;
    for (node = rd32(sysbase + EXECBASE_LIBLIST); valid_addr(node) && valid_addr(rd32(node)) && guard < 512;
         node = rd32(node), guard++) {
        char name[48];
        int i;
        read_cstr(rd32(node + NODE_NAME), name, sizeof(name));
        for (i = 0; i < s_nspecs; i++) {
            if (!strcmp(name, s_specs[i].name) && s_nlibs < MAX_LIBS) {
                relay_lib_t *l = &s_libs[s_nlibs++];
                l->base = node;
                l->lo = node - rd16(node + LIB_NEGSIZE);
                l->spec = i;
                strcpy(l->name, name);
                if (l->lo < s_lo) s_lo = l->lo;
                if (l->base > s_hi) s_hi = l->base;
            }
        }
    }
}

static void task_name(char *dst, int max)
{
    uint32_t sysbase = rd32(4), task;
    dst[0] = 0;
    if (!valid_addr(sysbase))
        return;
    task = rd32(sysbase + EXECBASE_THISTASK);
    if (valid_addr(task))
        read_cstr(rd32(task + NODE_NAME), dst, max);
}

static void head(const char *lib, int lvo)
{
    char task[64];
    task_name(task, sizeof(task));
    fprintf(s_out, "{\"lib\":\"%s\",\"lvo\":%d,\"task\":", lib, lvo);
    json_str(task);
}

void lxa_relay_check_slow(uint32_t pc)
{
    int i;
    uint32_t sp;

    if (s_countdown-- == 0) {
        s_countdown = s_nlibs ? 200000 : 5000;
        refresh_libs();
    }
    if (s_npend) {
        sp = m68k_get_reg(NULL, M68K_REG_A7);
        for (i = s_npend - 1; i >= 0; i--) {
            if (s_pend[i].ret_pc == pc && s_pend[i].sp == sp) {
                if (s_pend[i].spec < 0) {          /* exec OpenLibrary returned */
                    refresh_libs();
                    s_pend[i] = s_pend[--s_npend];
                    break;
                }
                if (s_inj_lib[0] && s_pend[i].lvo == s_inj_lvo && !strcmp(s_inj_lib, s_specs[s_pend[i].spec].name))
                    m68k_set_reg(M68K_REG_D0, m68k_get_reg(NULL, M68K_REG_D0) + (uint32_t)s_inj_delta);
                head(s_specs[s_pend[i].spec].name, s_pend[i].lvo);
                fprintf(s_out, ",\"ret\":%u}\n", m68k_get_reg(NULL, M68K_REG_D0));
                s_pend[i] = s_pend[--s_npend];
                break;
            }
        }
    }
    /* OpenLibrary()/OldOpenLibrary() through exec's jump table: re-read
     * the library list when it returns, so the first calls into a freshly
     * loaded disk library are traced too */
    {
        uint32_t sysbase = rd32(4);
        if ((pc == sysbase - 552 || pc == sysbase - 408) && s_npend < MAX_PENDING) {
            uint32_t a7 = m68k_get_reg(NULL, M68K_REG_A7);
            s_pend[s_npend].ret_pc = rd32(a7);
            s_pend[s_npend].sp = a7 + 4;
            s_pend[s_npend].lvo = 0;
            s_pend[s_npend].spec = -1;
            s_npend++;
        }
    }
    if (pc < s_lo || pc >= s_hi)
        return;
    for (i = 0; i < s_nlibs; i++) {
        relay_lib_t *l = &s_libs[i];
        if (pc >= l->lo && pc < l->base && (l->base - pc) % 6 == 0) {
            const relay_spec_t *sp_ = &s_specs[l->spec];
            int lvo = -(int)(l->base - pc), k, match = sp_->all;
            uint32_t r[16];
            char str[4][40];
            static const int sidx[4] = {1, 2, 8, 9};
            static const char *sname[4] = {"d1", "d2", "a0", "a1"};
            for (k = 0; !match && k < sp_->nlvo; k++)
                match = sp_->lvo[k] == lvo;
            if (!match)
                return;
            for (k = 0; k < 16; k++)
                r[k] = m68k_get_reg(NULL, (m68k_register_t)(M68K_REG_D0 + k));
            /* calls from lxa's own ROM code (e.g. the program loader, or one
             * library calling another) are library-internal, not behaviour */
            if (rd32(r[15]) >= ROM_START && rd32(r[15]) <= ROM_END)
                return;
            head(l->name, lvo);
            fprintf(s_out, ",\"d\":[%u,%u,%u,%u,%u,%u,%u,%u],\"a\":[%u,%u,%u,%u],\"str\":{",
                    r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10], r[11]);
            for (k = 0; k < 4; k++) {
                trace_str(r[sidx[k]], str[k]);
                fprintf(s_out, "%s\"%s\":", k ? "," : "", sname[k]);
                if (str[k][0])
                    json_str(str[k]);
                else
                    fputs("null", s_out);
            }
            fputs("}}\n", s_out);
            if (s_npend < MAX_PENDING) {
                s_pend[s_npend].ret_pc = rd32(r[15]);
                s_pend[s_npend].sp = r[15] + 4;
                s_pend[s_npend].lvo = (int16_t)lvo;
                s_pend[s_npend].spec = l->spec;
                s_npend++;
            }
            return;
        }
    }
}

bool lxa_relay_start(const char *spec, const char *path)
{
    char buf[2048], *lib, *save1 = NULL;
    lxa_relay_stop();
    s_out = fopen(path ? path : "lxa-trace.jsonl", "w");
    if (!s_out)
        return false;
    s_nspecs = 0;
    snprintf(buf, sizeof(buf), "%s", spec);
    for (lib = strtok_r(buf, ";", &save1); lib && s_nspecs < MAX_SPECS; lib = strtok_r(NULL, ";", &save1)) {
        relay_spec_t *s = &s_specs[s_nspecs++];
        char *colon = strchr(lib, ':'), *tok, *save2 = NULL;
        memset(s, 0, sizeof(*s));
        if (colon)
            *colon = 0;
        snprintf(s->name, sizeof(s->name), "%s%s", lib, strstr(lib, ".") ? "" : ".library");
        if (!colon || !strcmp(colon + 1, "*")) {
            s->all = true;
            continue;
        }
        for (tok = strtok_r(colon + 1, ",", &save2); tok && s->nlvo < MAX_LVOS; tok = strtok_r(NULL, ",", &save2)) {
            int v = atoi(tok);
            s->lvo[s->nlvo++] = (int16_t)(v > 0 ? -v : v);
        }
    }
    s_npend = 0;
    s_countdown = 0;
    g_relay_active = true;
    return true;
}

void lxa_relay_stop(void)
{
    g_relay_active = false;
    if (s_out) {
        fclose(s_out);
        s_out = NULL;
    }
}

void lxa_relay_init_from_env(void)
{
    const char *spec = getenv("LXA_TRACE");
    const char *inj = getenv("LXA_TRACE_INJECT");
    if (inj && *inj) {
        char lib[48];
        int lvo, delta;
        if (sscanf(inj, "%47[^:]:%d:%d", lib, &lvo, &delta) == 3) {
            snprintf(s_inj_lib, sizeof(s_inj_lib), "%s%s", lib, strstr(lib, ".") ? "" : ".library");
            s_inj_lvo = lvo > 0 ? -lvo : lvo;
            s_inj_delta = delta;
        }
    }
    if (spec && *spec)
        lxa_relay_start(spec, getenv("LXA_TRACE_FILE"));
}
