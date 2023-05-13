#ifndef HAVE_UTIL_H
#define HAVE_UTIL_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define ENDIAN_SWAP_16(data) ( (((data) >> 8) & 0x00FF) | (((data) << 8) & 0xFF00) )
#define ENDIAN_SWAP_32(data) ( (((data) >> 24) & 0x000000FF) | (((data) >>  8) & 0x0000FF00) | \
                               (((data) <<  8) & 0x00FF0000) | (((data) << 24) & 0xFF000000) )
#define ENDIAN_SWAP_64(data) ( (((data) & 0x00000000000000ffLL) << 56) | \
                               (((data) & 0x000000000000ff00LL) << 40) | \
                               (((data) & 0x0000000000ff0000LL) << 24) | \
                               (((data) & 0x00000000ff000000LL) << 8)  | \
                               (((data) & 0x000000ff00000000LL) >> 8)  | \
                               (((data) & 0x0000ff0000000000LL) >> 24) | \
                               (((data) & 0x00ff000000000000LL) >> 40) | \
                               (((data) & 0xff00000000000000LL) >> 56))

/*
 * debugging / logging
 */

#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARNING 2
#define LOG_ERROR   3

#define LOG_LEVEL LOG_DEBUG
//#define LOG_LEVEL LOG_INFO

void  lputc    (int level, char c);
void  lputs    (int level, const char *s);
void  lprintf  (int level, const char *format, ...);
void  vlprintf (int level, const char *format, va_list ap);

#define LPRINTF(lvl, ...) do { if (lvl >= LOG_LEVEL) lprintf(lvl, __VA_ARGS__); } while (0)
#define LPUTS(lvl, s) do { if (lvl >= LOG_LEVEL) lputs(lvl, s); } while (0)

#define CPRINTF(lvl, ...) do { lprintf (LOG_INFO, __VA_ARGS__); } while (0)

//#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DPRINTF(lvl, ...) LPRINTF (lvl, __VA_ARGS__)
#define DPUTS(lvl, s) LPUTS (lvl, s)
#else
#define DPRINTF(lvl, ...)
#define DPUTS(lvl, s)
#endif

extern FILE    *g_logf;
extern bool     g_debug;

void hexdump (int lvl, uint32_t offset, uint32_t len);

void util_init(void);

#endif

