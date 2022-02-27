#ifndef HAVE_UTIL_H
#define HAVE_UTIL_H

#include <exec/types.h>


#define EMU_CALL_LPUTC         1
#define EMU_CALL_STOP          2
#define EMU_CALL_TRACE         3
#define EMU_CALL_LPUTS         4
#define EMU_CALL_EXCEPTION     5
#define EMU_CALL_WAIT          6
#define EMU_CALL_MONITOR       8
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
#define LPUTS(lvl, s) do { if (lvl >= LOG_LEVEL) lputs(lvl, s); } while (0)

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

#define NEWLIST(l) ((l)->lh_Head = (struct Node *)&(l)->lh_Tail, \
                    (l)->lh_Tail = NULL, \
                    (l)->lh_TailPred = (struct Node *)&(l)->lh_Head)

void            U_hexdump      (int lvl, void *mem, unsigned int len);

struct Task    *U_allocTask    (STRPTR name, LONG pri, APTR initpc, ULONG stacksize);
void            U_freeTask     (struct Task *task);
struct Task    *U_createTask   (STRPTR name, LONG pri, APTR initpc, ULONG stacksize);

struct Process *U_allocProcess (STRPTR name, LONG pri, APTR initpc, ULONG stacksize);

#endif
