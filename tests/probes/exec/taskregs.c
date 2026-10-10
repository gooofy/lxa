/*
 * Probe (Phase 237d): the registers a task created with AddTask() starts
 * with.  Fred Fish Password "loop" (disk 365) starts its VTBeep task with
 * amiga.lib CreateTask(); the task's Lattice stack check reads 744(A4)
 * before it sets A4.  The new task stores D0-D7/A0-A6 and the word at the
 * top of its stack (the return address AddTask() pushed: finalPC), then
 * signals the probe and waits.
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static ULONG regs[16];      /* d0-d7, a0-a6, (sp) */
static struct Task *parent;

__asm__(
    "    .text\n"
    "    .globl _tr_entry\n"
    "_tr_entry:\n"
    "    movem.l d0-d7/a0-a6,_regs\n"
    "    move.l (sp),_regs+60\n"
    "    move.l 4,a6\n"
    "    move.l _parent,a1\n"
    "    move.l #0x80000000,d0\n"
    "    jsr -324(a6)\n"                    /* Signal() */
    "    moveq #0,d0\n"
    "    jsr -318(a6)\n"                    /* Wait(0): forever */
    "    rts\n");
extern void tr_entry(void);

static const char *rn[] = { "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
                            "a0", "a1", "a2", "a3", "a4", "a5", "a6" };

int main(void)
{
    struct Task *t = AllocMem(sizeof(struct Task), MEMF_PUBLIC | MEMF_CLEAR);
    UBYTE *stack = AllocMem(2048, MEMF_PUBLIC | MEMF_CLEAR);
    int i;

    if (!t || !stack)
        return 20;
    parent = FindTask(NULL);
    SetSignal(0, 0x80000000);
    t->tc_Node.ln_Type = NT_TASK;
    t->tc_Node.ln_Name = (char *)"probe.taskregs";
    t->tc_Node.ln_Pri = 1;
    t->tc_SPLower = stack;
    t->tc_SPUpper = stack + 2048;
    t->tc_SPReg = stack + 2048;
    NewList(&t->tc_MemEntry);
    AddTask(t, (APTR)tr_entry, NULL);
    Wait(0x80000000);
    Forbid();
    RemTask(t);
    Permit();
    for (i = 0; i < 15; i++) {
        ULONG v = regs[i];
        probe_s(rn[i]);
        probe_s(" = ");
        if (v == (ULONG)t)
            probe_s("the task");
        else if (v == (ULONG)SysBase)
            probe_s("SysBase");
        else if (v >= (ULONG)stack && v <= (ULONG)stack + 2048) {
            probe_s("stack+");
            probe_dec((LONG)(v - (ULONG)stack));
        } else if (v == (ULONG)tr_entry)
            probe_s("initPC");
        else if (v < 0x100)
            probe_hex(v, 8);
        else
            probe_s(v >= 0xf80000 && v < 0x1000000 ? "(ROM)" : "(other)");
        probe_ch('\n');
    }
    probe_s("(sp) = ");
    probe_s(regs[15] >= 0xf80000 && regs[15] < 0x1000000 ? "(ROM)" :
            regs[15] == 0 ? "0" : "(other)");
    probe_ch('\n');
    FreeMem(stack, 2048);
    FreeMem(t, sizeof(struct Task));
    return 0;
}
