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

/* ----- 3. kprintf stub -------------------------------------------------- */

void kprintf(const char *fmt, ...)
{
    (void)fmt;
}

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

APTR AsmCreatePool(ULONG memFlags, ULONG puddleSize, ULONG threshSize,
                   struct ExecBase *SysBase_unused)
{
    (void)SysBase_unused;
    return CreatePool(memFlags, puddleSize, threshSize);
}

void AsmDeletePool(APTR pool, struct ExecBase *SysBase_unused)
{
    (void)SysBase_unused;
    DeletePool(pool);
}

APTR AsmAllocPooled(APTR pool, ULONG size, struct ExecBase *SysBase_unused)
{
    (void)SysBase_unused;
    return AllocPooled(pool, size);
}

void AsmFreePooled(APTR pool, APTR mem, ULONG size,
                   struct ExecBase *SysBase_unused)
{
    (void)SysBase_unused;
    FreePooled(pool, mem, size);
}
