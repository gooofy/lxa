#ifndef HAVE_UTIL_H
#define HAVE_UTIL_H

#include "emucalls.h"

#include <exec/types.h>

#include "dos/dos.h"
#include "dos/dosextens.h"

ULONG emucall0 (ULONG func);
ULONG emucall1 (ULONG func, ULONG param1);
ULONG emucall2 (ULONG func, ULONG param1, ULONG param2);
ULONG emucall3 (ULONG func, ULONG param1, ULONG param2, ULONG param3);
ULONG emucall4 (ULONG func, ULONG param1, ULONG param2, ULONG param3, ULONG param4);

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

/*
 * Helper macro for safe string printing in debug statements.
 * Handles NULL pointers and CONST_STRPTR (UBYTE*) vs char* type mismatches.
 */
#define STRORNULL(s) ((s) ? (const char*)(s) : "(null)")

//#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DPRINTF(lvl, ...) LPRINTF (lvl, __VA_ARGS__)
#define DPUTS(lvl, s) LPUTS (lvl, s)
#else
#define DPRINTF(lvl, ...)
#define DPUTS(lvl, s)
#endif

void  emu_stop (ULONG rv);
void  emu_trace (BOOL enable);
void  emu_monitor (void);

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
int strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);

#define ALIGN(x,a)              __ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))

#define NEWLIST(l) ((l)->lh_Head = (struct Node *)&(l)->lh_Tail, \
                    (l)->lh_Tail = NULL, \
                    (l)->lh_TailPred = (struct Node *)&(l)->lh_Head)

void            U_hexdump        (int lvl, void *mem, unsigned int len);

struct Task    *U_allocTask      (STRPTR name, LONG pri, ULONG stacksize, BOOL isProcess);
void            U_prepareTask    (struct Task *task, APTR initPC, APTR finalPC, char *args);
void            U_freeTask       (struct Task *task);
struct Task    *U_createTask     (STRPTR name, LONG pri, APTR initpc, ULONG stacksize);

void            U_prepareProcess (struct Process *process, APTR initPC, APTR finalPC, ULONG stacksize, char *args);

#endif
