#include <exec/types.h>

void lputc(int level, char c)
{
    (void)level;
    (void)c;
}

void lputs(int level, const char *s)
{
    (void)level;
    (void)s;
}

void lprintf(int level, const char *format, ...)
{
    (void)level;
    (void)format;
}

__stdargs void __assert_func(const char *file_name, int line_number, const char *e)
{
    (void)file_name;
    (void)line_number;
    (void)e;

    for (;;)
    {
    }
}

/* Phase 203: disk libraries report stubs through the same EMU_CALL as the
 * ROM (LXA_UNIMPLEMENTED in util.h), so stub telemetry covers them too. */
ULONG emucall3(ULONG func, ULONG param1, ULONG param2, ULONG param3)
{
    ULONG res;
    asm volatile( "move.l    %1, d0\n\t"
         "move.l    %2, d1\n\t"
         "move.l    %3, d2\n\t"
         "move.l    %4, d3\n\t"
         "illegal\n\t"
         "move.l    d0, %0\n"
        : "=r" (res)
        : "r" (func), "r" (param1), "r" (param2), "r" (param3)
        : "cc", "d0", "d1", "d2", "d3"
        );
    return res;
}
