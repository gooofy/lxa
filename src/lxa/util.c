#include "util.h"

#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>
#include <pthread.h>

#define LXA_LOG_FILENAME "lxa.log"

FILE    *g_logf                          = NULL;
bool     g_debug                         = false;

static pthread_mutex_t g_log_mutex;
static pthread_once_t g_log_mutex_once = PTHREAD_ONCE_INIT;
static __thread bool g_log_line_locked = false;

static void init_log_mutex(void)
{
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_log_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

static void lock_log_output(void)
{
    pthread_once(&g_log_mutex_once, init_log_mutex);
    pthread_mutex_lock(&g_log_mutex);
}

static void unlock_log_output(void)
{
    pthread_mutex_unlock(&g_log_mutex);
}

void lputc (int lvl, char ch)
{
    if (!g_log_line_locked)
    {
        lock_log_output();
        g_log_line_locked = true;
    }

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

    if (ch == '\n')
    {
        g_log_line_locked = false;
        unlock_log_output();
    }
}

void lputs (int lvl, const char *s)
{
    lock_log_output();

    if (g_logf)
    {
        fprintf (g_logf, "%s", s);
        fflush (g_logf);
    }

    if (lvl || g_debug)
    {
        printf ("%s", s); fflush (stdout);
    }

    unlock_log_output();
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

void util_shutdown(void)
{
    if (g_logf)
    {
        fclose(g_logf);
        g_logf = NULL;
    }

    g_debug = false;
}
