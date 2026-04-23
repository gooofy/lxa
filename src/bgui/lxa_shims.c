/*
 * lxa_shims.c — runtime stubs needed to link bgui.library with bebbo
 *               (m68k-amigaos-gcc) instead of the original SAS/C build.
 *
 * The shims here are deliberately the smallest possible, kept in one
 * place so future BGUI patches don't accidentally reintroduce SAS/C-only
 * helpers without a matching shim.
 *
 * Categories:
 *
 *   1. __restore_a4 — libnix's `-fbaserel` runtime helper.  BGUI is
 *      compiled with -fbaserel because four sites use `getreg(REG_A4)` to
 *      stash a per-class "BGUIGlobalData" pointer (see classes.c:310,
 *      classes.c:344, classes.c:464, listclass.c:3063).  However we do
 *      NOT use libnix's library scaffolding (BGUI's own lib.c provides
 *      LibInit), so libnix's `___a4_init` symbol does not exist.  The
 *      four BGUI sites only treat A4 as an opaque cookie — they save it,
 *      restore it, and pass it as an extra dispatcher argument that is
 *      never dereferenced as a real data pointer.  Therefore the
 *      cheapest correct shim is: leave A4 alone.  A bare RTS satisfies
 *      that invariant.
 *
 *   2. stch_l / stcu_d — SAS/C string-to-number helpers.  Two callers:
 *      blitter.c:223 (parse a long out of pr_Arguments) and
 *      labelclass.c:567 (decimal pen number into a string).  Replaced
 *      with portable C using strtol/sprintf semantics.
 *
 *   3. kprintf — debug print used unconditionally on one BGUI relayout
 *      path (baseclass.c:1159, currently NOT wrapped in WW()).  Stubbed
 *      to no-op; if you need the trace, route through DPRINTF later.
 *
 *   4. AsmCreatePool / AsmDeletePool / AsmAllocPooled / AsmFreePooled —
 *      SAS/C asm wrappers around V39 Exec memory pools.  Implemented
 *      here as thin wrappers over the real Exec V39 entry points.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

/* ----- 1. __restore_a4 -------------------------------------------------- */

__asm__(
"   .globl  ___restore_a4       \n"
"___restore_a4:                 \n"
"   rts                         \n"
);

/* ----- 2a. stch_l(string, &val) ----------------------------------------- *
 * SAS/C semantics: parse leading hex/decimal/octal digits from `s` into
 * `*val`.  Returns the number of characters consumed (0 on no digits).
 * BGUI uses it on pr_Arguments parsing in blitter.c.                       */
LONG stch_l(const char *s, LONG *val)
{
    char *end = NULL;
    long  v   = strtol(s, &end, 0);
    if (end == s) {
        return 0;
    }
    if (val) *val = (LONG)v;
    return (LONG)(end - s);
}

/* ----- 2b. stcu_d(buf, val) --------------------------------------------- *
 * SAS/C semantics: write `val` as an unsigned decimal string into `buf`,
 * with no trailing NUL.  Returns the number of digits written.  Used by
 * labelclass.c:567 to format a pen number into the inline label parser
 * scratch buffer.                                                          */
LONG stcu_d(char *buf, ULONG val)
{
    char  tmp[16];
    LONG  n = 0;
    if (val == 0) {
        buf[0] = '0';
        return 1;
    }
    while (val && n < (LONG)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (val % 10));
        val /= 10;
    }
    {
        LONG i;
        for (i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
    }
    return n;
}

/* ----- 3. kprintf — route to host log via EMU_CALL_LPUTS --------------- *
 * BGUI's WW(kprintf(...)) macro is per-file no-op'd, but we can flip it to
 * an active macro at debug time (see windowclass.c top-of-file).
 *
 * Strategy: format the string into a static buffer using exec's RawDoFmt,
 * then ship the formatted text to the host log via EMU_CALL_LPUTS at
 * LOG_INFO so it always shows up in lxa.log without bumping ROM debug
 * level (which floods the log and times tests out).
 *
 * Argument marshalling: AmigaOS RawDoFmt expects word-packed args (%d / %x
 * are WORD, %ld / %lx / %s are LONG).  C va_list passes everything as
 * long.  We scan the format string and pack a UWORD[] array accordingly.
 */

#define LXA_LOG_INFO 1
#define LXA_EMU_CALL_LPUTC 1
#define LXA_EMU_CALL_LPUTS 4

/* Single-char trap: matches what ROM lputc uses; known to fire op_illg. */
static void lxa_emu_lputc(char c)
{
    register unsigned long d0 __asm("d0") = LXA_EMU_CALL_LPUTC;
    register unsigned long d1 __asm("d1") = LXA_LOG_INFO;
    register unsigned long d2 __asm("d2") = (unsigned char)c;
    __asm__ volatile (
        "illegal"
        : "+d"(d0), "+d"(d1), "+d"(d2)
        :
        : "cc", "memory"
    );
}

static void lxa_emu_lputs(const char *s)
{
    register unsigned long d0 __asm("d0") = LXA_EMU_CALL_LPUTS;
    register unsigned long d1 __asm("d1") = LXA_LOG_INFO;
    register const char    *d2 __asm("d2") = s;
    __asm__ volatile (
        "illegal"
        : "+d"(d0), "+d"(d1), "+d"(d2)
        :
        : "cc", "memory"
    );
}

static UBYTE s_kp_emit_buf[1024];
static int   s_kp_emit_pos;

static void s_kp_putc_buffer(register UBYTE ch __asm("d0"),
                             register APTR data __asm("a3"))
{
    (void)data;
    if (s_kp_emit_pos < (int)sizeof(s_kp_emit_buf) - 1) {
        s_kp_emit_buf[s_kp_emit_pos++] = ch;
    }
}

static void s_kp_emit_dec(unsigned long v)
{
    /* Avoid 32-bit /,% which pull in libgcc's __udivsi3/__umodsi3 — those
     * are baserel and crash because we built BGUI without -fbaserel.
     * Instead build the digits via repeated subtraction of powers of 10. */
    static const unsigned long pow10[] = {
        1000000000UL, 100000000UL, 10000000UL, 1000000UL,
        100000UL, 10000UL, 1000UL, 100UL, 10UL, 1UL
    };
    int started = 0;
    int i;
    for (i = 0; i < 10; i++) {
        unsigned long p = pow10[i];
        int d = 0;
        while (v >= p) { v -= p; d++; }
        if (d || started || i == 9) {
            lxa_emu_lputc((char)('0' + d));
            started = 1;
        }
    }
}

void kprintf(const char *fmt, ...)
{
    /* Minimal format support: %s, %d, %u, %x, %lx, %ld, %lu, %p, %c, %% */
    va_list ap;
    va_start(ap, fmt);

    lxa_emu_lputc('K');
    lxa_emu_lputc('P');
    lxa_emu_lputc(':');
    lxa_emu_lputc(' ');

    if (!fmt) { lxa_emu_lputc('\n'); va_end(ap); return; }

    const char *p = fmt;
    while (*p) {
        if (*p != '%') { lxa_emu_lputc(*p++); continue; }
        p++;
        /* skip flags/width/precision */
        while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') p++;
        while (*p >= '0' && *p <= '9') p++;
        if (*p == '.') { p++; while (*p >= '0' && *p <= '9') p++; }
        int is_long = 0;
        if (*p == 'l') { is_long = 1; p++; if (*p == 'l') p++; }
        if (*p == 'z' || *p == 'h') p++;
        char conv = *p ? *p++ : 0;
        switch (conv) {
            case 'd': case 'i': {
                long v = is_long ? va_arg(ap, long) : (long)va_arg(ap, int);
                if (v < 0) { lxa_emu_lputc('-'); v = -v; }
                s_kp_emit_dec((unsigned long)v);
                break;
            }
            case 'u': {
                unsigned long v = is_long ? va_arg(ap, unsigned long) : (unsigned long)va_arg(ap, unsigned int);
                s_kp_emit_dec(v);
                break;
            }
            case 'x': case 'X': case 'p': {
                unsigned long v;
                if (conv == 'p') v = (unsigned long)va_arg(ap, void*);
                else v = is_long ? va_arg(ap, unsigned long) : (unsigned long)va_arg(ap, unsigned int);
                char buf[16]; int n = 0;
                const char *digs = (conv == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                if (v == 0) buf[n++] = '0';
                while (v) { buf[n++] = digs[v & 0xf]; v >>= 4; }
                while (n--) lxa_emu_lputc(buf[n]);
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s) lxa_emu_lputc(*s++);
                break;
            }
            case 'c': {
                int v = va_arg(ap, int);
                lxa_emu_lputc((char)v);
                break;
            }
            case '%': lxa_emu_lputc('%'); break;
            default: lxa_emu_lputc('%'); if (conv) lxa_emu_lputc(conv); break;
        }
    }
    /* Ensure trailing newline */
    if (p > fmt && p[-1] != '\n') lxa_emu_lputc('\n');
    va_end(ap);
}
#if 0
void kprintf_full(const char *fmt, ...)
{
    va_list ap;
    UWORD   args[64];
    int     ai = 0;
    const char *p;

    /* PROBE: fire a known-working d0=1 LPUTC trap immediately on entry,
     * before touching SysBase or RawDoFmt.  If this shows up in lxa.log
     * but the d0=4 LPUTS trap below does not, the body is reached but
     * something in the LPUTS path is broken.  If neither shows up,
     * kprintf is never being called. */
    lxa_emu_lputc('K');
    lxa_emu_lputc('P');
    lxa_emu_lputc('\n');

    va_start(ap, fmt);

    /* Walk the format string and pack args. */
    for (p = fmt; *p && ai < (int)(sizeof(args)/sizeof(args[0])) - 4; p++) {
        if (*p != '%') continue;
        p++;
        /* Skip flags / width / precision. */
        while (*p && (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0')) p++;
        while (*p && isdigit((unsigned char)*p)) p++;
        if (*p == '.') {
            p++;
            while (*p && isdigit((unsigned char)*p)) p++;
        }
        /* Length modifier. */
        int is_long = 0;
        if (*p == 'l') { is_long = 1; p++; if (*p == 'l') p++; }
        /* Conversion. */
        switch (*p) {
            case 'd': case 'i': case 'u': case 'x': case 'X': case 'o':
                if (is_long) {
                    unsigned long v = va_arg(ap, unsigned long);
                    args[ai++] = (UWORD)((v >> 16) & 0xFFFF);
                    args[ai++] = (UWORD)(v & 0xFFFF);
                } else {
                    int v = va_arg(ap, int);
                    args[ai++] = (UWORD)(v & 0xFFFF);
                }
                break;
            case 's': case 'p': {
                void *v = va_arg(ap, void *);
                unsigned long uv = (unsigned long)v;
                args[ai++] = (UWORD)((uv >> 16) & 0xFFFF);
                args[ai++] = (UWORD)(uv & 0xFFFF);
                break;
            }
            case 'c': {
                int v = va_arg(ap, int);
                args[ai++] = (UWORD)(v & 0xFFFF);
                break;
            }
            case '%':
            default:
                break;
        }
    }
    va_end(ap);

    s_kp_emit_pos = 0;
    RawDoFmt((CONST_STRPTR)fmt, args, (void(*)())s_kp_putc_buffer, NULL);
    s_kp_emit_buf[s_kp_emit_pos] = '\0';

    /* Strip trailing newline so host log line stays single-line. */
    while (s_kp_emit_pos > 0 &&
           (s_kp_emit_buf[s_kp_emit_pos-1] == '\n' ||
            s_kp_emit_buf[s_kp_emit_pos-1] == '\r')) {
        s_kp_emit_buf[--s_kp_emit_pos] = '\0';
    }

    lxa_emu_lputs((const char *)s_kp_emit_buf);
    lxa_emu_lputs("\n");
}
#endif

/* ----- 3b. exit stub ---------------------------------------------------- *
 * Pulled in transitively by libnix's raise.o / __initstdio.o.  A real
 * library can never legitimately exit() — the host process owns process
 * termination — so we provide a hard halt that is unreachable in
 * normal operation but satisfies the linker.                              */
void exit(int code)
{
    (void)code;
    for (;;) { }
}

/* ----- 4. Asm*Pool wrappers --------------------------------------------- *
 * BGUI's task.c declared these as SAS/C-asm-style externs and uses them
 * unconditionally.  Under bebbo, just forward to the standard V39 Exec
 * pool calls.  SysBase is fetched the canonical way (absolute long 4)
 * inside <proto/exec.h> inlines, so we can ignore the trailing SysBase
 * register argument that BGUI passes by convention.                        */

APTR AsmCreatePool(ULONG memFlags __asm("d0"), ULONG puddleSize __asm("d1"),
                   ULONG threshSize __asm("d2"),
                   struct ExecBase *SysBase_unused __asm("a6"))
{
    (void)SysBase_unused;
    return CreatePool(memFlags, puddleSize, threshSize);
}

void AsmDeletePool(APTR pool __asm("a0"), struct ExecBase *SysBase_unused __asm("a6"))
{
    (void)SysBase_unused;
    DeletePool(pool);
}

APTR AsmAllocPooled(APTR pool __asm("a0"), ULONG size __asm("d0"),
                    struct ExecBase *SysBase_unused __asm("a6"))
{
    (void)SysBase_unused;
    return AllocPooled(pool, size);
}

void AsmFreePooled(APTR pool __asm("a0"), APTR mem __asm("a1"), ULONG size __asm("d0"),
                   struct ExecBase *SysBase_unused __asm("a6"))
{
    (void)SysBase_unused;
    FreePooled(pool, mem, size);
}
