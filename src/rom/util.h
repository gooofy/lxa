#ifndef HAVE_UTIL_H
#define HAVE_UTIL_H

#include <exec/types.h>

#define EMU_CALL_DOS_OPEN   1000
#define EMU_CALL_DOS_READ   1001

ULONG emucall3 (ULONG func, ULONG param1, ULONG param2, ULONG param3);

int   isdigit  (int c);
void  lputc    (char c);
void  lputs    (const char *s);
void  lprintf  (const char *format, ...);

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

void hexdump(void *mem, unsigned int len);

#endif
