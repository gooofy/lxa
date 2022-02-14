#ifndef HAVE_UTIL_H
#define HAVE_UTIL_H

#include <exec/types.h>


#define EMU_CALL_LPUTC         1
#define EMU_CALL_STOP          2
#define EMU_CALL_TRACE         3
#define EMU_CALL_DOS_OPEN   1000
#define EMU_CALL_DOS_READ   1001
#define EMU_CALL_DOS_SEEK   1002
#define EMU_CALL_DOS_CLOSE  1003

ULONG emucall1 (ULONG func, ULONG param1);
ULONG emucall3 (ULONG func, ULONG param1, ULONG param2, ULONG param3);

/*
 * debugging / logging
 */

#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARNING 2
#define LOG_ERROR   3

#define LOG_LEVEL LOG_DEBUG
//#define LOG_LEVEL LOG_INFO

int   isdigit  (int c);
void  lputc    (int level, char c);
void  lputs    (int level, const char *s);
void  lprintf  (int level, const char *format, ...);

#define LPRINTF(lvl, ...) do { if (lvl >= LOG_LEVEL) lprintf(lvl, __VA_ARGS__); } while (0)

void  emu_stop (void);

#undef assert

__stdargs void __assert_func (const char *file_name, int line_number, const char *e);

#ifdef NDEBUG
# define assert(__e) ((void)0)
#else
# define assert(__e) ((__e) ? (void)0 : __assert_func (__FILE__, __LINE__, #__e))
#endif

struct InitTable
{
    ULONG  LibBaseSize;
    APTR  *FunctionTable;
    APTR  *DataTable;
    APTR   InitLibFn;
};

void *memset(void *dst, int c, ULONG n);
int strcmp(const char* s1, const char* s2);

#define ALIGN(x,a)              __ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))

void hexdump(int lvl, void *mem, unsigned int len);

#endif
