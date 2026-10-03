/*
 * subtask.h - tiny exec task helper for probes (no amiga.lib needed).
 */
#ifndef PROBE_SUBTASK_H
#define PROBE_SUBTASK_H

#include <exec/tasks.h>
#include <exec/memory.h>

extern struct ExecBase *SysBase;

#define SUBTASK_STACK 4096

struct SubTask {
    struct Task tc;
    char name[16];
    UBYTE stack[SUBTASK_STACK];
};

static struct Task *start_task(const char *name, void (*fn)(void), BYTE pri)
{
    struct SubTask *st = AllocMem(sizeof(struct SubTask), MEMF_PUBLIC | MEMF_CLEAR);
    int i;
    if (!st)
        return NULL;
    for (i = 0; i < 15 && name[i]; i++)
        st->name[i] = name[i];
    st->tc.tc_Node.ln_Type = NT_TASK;
    st->tc.tc_Node.ln_Pri = pri;
    st->tc.tc_Node.ln_Name = st->name;
    st->tc.tc_SPLower = st->stack;
    st->tc.tc_SPUpper = st->stack + SUBTASK_STACK;
    st->tc.tc_SPReg = st->stack + SUBTASK_STACK;
    AddTask(&st->tc, (APTR)fn, NULL);
    return &st->tc;
}

/* call after the task has finished (signalled its last word and Wait(0)'d or returned) */
static void free_task(struct Task *t)
{
    if (t)
        FreeMem(t, sizeof(struct SubTask));
}

#endif
