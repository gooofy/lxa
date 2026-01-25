#ifndef TEST_ASSERT_H
#define TEST_ASSERT_H

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#define EMU_CALL_STOP 6

static inline void emu_stop(LONG rv)
{
    __asm volatile (
        "move.l %0, d1\n"
        "move.w #6, d0\n"
        "illegal\n"
        : /* no outputs */
        : "r" (rv)
        : "d0", "d1"
    );
}

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            Write(Output(), (CONST APTR)"[TEST FAIL] ", 12); \
            Write(Output(), (CONST APTR)__FILE__, sizeof(__FILE__) - 1); \
            Write(Output(), (CONST APTR)":", 1); \
            Write(Output(), (CONST APTR)_LINE_STR(__LINE__), _LINE_STR_LEN(__LINE__)); \
            Write(Output(), (CONST APTR)": assert failed: " #cond "\n", 20 + sizeof(#cond) - 1); \
            emu_stop(1); \
        } \
    } while (0)

#define _LINE_STR_LEN(line) _LINE_STR_LEN_HELPER(line)
#define _LINE_STR_LEN_HELPER(line) (sizeof(#line) - 1)
#define _LINE_STR(line) #line

#define TEST_REPORT_PASS() \
    do { \
        Write(Output(), (CONST APTR)"[TEST PASS]\n", 12); \
    } while (0)

#endif /* TEST_ASSERT_H */
