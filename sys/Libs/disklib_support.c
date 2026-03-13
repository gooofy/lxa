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
