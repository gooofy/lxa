/*
 * CPU exceptions and tc_TrapCode (AmigaOS convention, RKRM Exec "Traps").
 *
 *  1. A task-installed tc_TrapCode receives the exception number on top of
 *     the supervisor stack and resumes the task (divide by zero, TRAP #5,
 *     CHK).
 *  2. A process without its own handler that raises a CPU exception is held
 *     (Software Failure): it never continues, the system goes on.  (A plain
 *     Task that faults ends in a dead-end alert on AmigaOS 3.1, so the test
 *     uses a process.)
 *
 * Reference-validated (Phase 232): output identical on AmigaOS 3.1.
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static volatile ULONG trap_count;
static volatile ULONG last_trap;

static void print(const char *s) { PutStr((STRPTR)s); Flush(Output()); }
static void print_num(ULONG v)
{
    char b[12];
    int i = 11;
    b[i] = 0;
    do { b[--i] = '0' + v % 10; v /= 10; } while (v);
    PutStr((STRPTR)&b[i]);
}

/* Supervisor mode, stack = [exception number][frame].  Records the number
 * and returns: for divide by zero, CHK and TRAP the stacked PC already
 * points to the next instruction. */
asm(
"   .text\n"
"_trap_handler:\n"
"   move.l  d0, -(sp)\n"
"   move.l  4(sp), d0\n"
"   move.l  d0, _last_trap\n"
"   addq.l  #1, _trap_count\n"
"   move.l  (sp)+, d0\n"
"   addq.l  #4, sp\n"
"   rte\n"
);
extern void trap_handler(void);

static ULONG do_div(ULONG a, UWORD b)
{
    register ULONG d0 asm("d0") = a;
    register ULONG d1 asm("d1") = b;
    asm volatile("divu.w %1, %0" : "+d"(d0) : "d"(d1));
    return d0;
}

static volatile LONG child_reached_end;

static void child(void)
{
    /* no handler: exec holds this task */
    volatile UWORD zero = 0;
    do_div(10, zero);
    child_reached_end = 1;
    Wait(0);
}

int main(void)
{
    struct Task *me = FindTask(NULL);
    APTR old_code = me->tc_TrapCode;
    int errors = 0;

    print("Test 1: tc_TrapCode catches divide by zero\n");
    me->tc_TrapCode = (APTR)trap_handler;
    {
        volatile UWORD zero = 0;
        do_div(100, zero);
    }
    print("  traps: "); print_num(trap_count); print(", number: "); print_num(last_trap); print("\n");
    if (trap_count != 1 || last_trap != 5) { print("  FAIL\n"); errors++; }

    print("Test 2: TRAP #5\n");
    asm volatile("trap #5");
    print("  traps: "); print_num(trap_count); print(", number: "); print_num(last_trap); print("\n");
    if (trap_count != 2 || last_trap != 37) { print("  FAIL\n"); errors++; }

    print("Test 3: CHK out of bounds\n");
    {
        register LONG v asm("d0") = 10;
        register LONG lim asm("d1") = 5;
        asm volatile("chk.w %1, %0" : : "d"(v), "d"(lim));
    }
    print("  traps: "); print_num(trap_count); print(", number: "); print_num(last_trap); print("\n");
    if (trap_count != 3 || last_trap != 6) { print("  FAIL\n"); errors++; }
    me->tc_TrapCode = old_code;

    print("Test 4: a faulting process without handler is held\n");
    {
        struct Process *pr = CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"traps-child",
                                               NP_StackSize, 4096, TAG_DONE);
        Delay(25);
        print("  child started: ");
        print(pr ? "yes\n" : "no\n");
        print("  child continued after the fault: ");
        print(child_reached_end ? "yes\n" : "no\n");
        print("  parent still runs: yes\n");
        if (!pr || child_reached_end) { print("  FAIL\n"); errors++; }
        /* the held process is left alone, as on AmigaOS */
    }

    print(errors ? "FAIL: traps\n" : "PASS: traps\n");
    return errors ? 20 : 0;
}
