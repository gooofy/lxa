/*
 * Test Stubs for Unit Testing
 *
 * Provides minimal stub implementations for functions that unit tests
 * don't need full implementations of (logging, etc.)
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

/* === util.h stubs === */

FILE *g_logf = NULL;
bool g_debug = false;

void lputc(int level, char c)
{
    (void)level;
    (void)c;
    /* Stub: do nothing in unit tests */
}

void lputs(int level, const char *s)
{
    (void)level;
    (void)s;
    /* Stub: do nothing in unit tests */
}

void lprintf(int level, const char *format, ...)
{
    (void)level;
    (void)format;
    /* Stub: do nothing in unit tests */
    /* If debugging tests, uncomment:
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    */
}

void vlprintf(int level, const char *format, va_list ap)
{
    (void)level;
    (void)format;
    (void)ap;
    /* Stub: do nothing in unit tests */
}

void hexdump(int lvl, unsigned int offset, unsigned int len)
{
    (void)lvl;
    (void)offset;
    (void)len;
    /* Stub: do nothing in unit tests */
}

void util_init(void)
{
    /* Stub: do nothing in unit tests */
}
