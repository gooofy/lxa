#include "util.h"

#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>

#define LXA_LOG_FILENAME "lxa.log"

FILE    *g_logf                          = NULL;
bool     g_debug                         = false;

void lputc (int lvl, char ch)
{
    if (g_logf)
    {
        fprintf (g_logf, "%c", ch);
        if (ch==10)
            fflush (g_logf);
    }

    if (lvl || g_debug)
    {
        printf ("%c", ch); fflush (stdout);
    }
}

void lputs (int lvl, const char *s)
{
    if (g_logf)
    {
        fprintf (g_logf, "%s", s);
        fflush (g_logf);
    }

    if (lvl || g_debug)
    {
        printf ("%s", s); fflush (stdout);
    }
}

void vlprintf (int level, const char *format, va_list ap)
{
    char buf[1024];
    vsnprintf (buf, 1024, format, ap);
    lputs (level, buf);
}

void lprintf (int level, const char *format, ...)
{
    va_list ap;

    va_start (ap, format);
    vlprintf (level, format, ap);
    va_end(ap);
}

void util_init(void)
{
    // logging
    g_logf = fopen (LXA_LOG_FILENAME, "w");
    assert (g_logf);
}


