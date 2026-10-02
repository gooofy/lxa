#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <hardware/blit.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;

static volatile LONG g_events[8];
static volatile LONG g_event_count;
static volatile LONG g_child_done;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + 15;
    BOOL neg = FALSE;

    *p = '\0';

    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }

    do
    {
        *(--p) = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    if (neg)
        *(--p) = '-';

    print(p);
}

static void reset_events(void)
{
    int i;
    g_event_count = 0;
    g_child_done = 0;
    for (i = 0; i < 8; i++)
        g_events[i] = 0;
}

static int qblit_func_a(void)
{
    g_events[g_event_count++] = 1;
    return 0;
}

static int qblit_func_b(void)
{
    g_events[g_event_count++] = 2;
    return 0;
}

static int cleanup_func(void)
{
    g_events[g_event_count++] = 3;
    return 0;
}

static void __attribute__((noinline)) waitblit_child_task(void)
{
    WaitBlit();
    g_events[g_event_count++] = 4;
    g_child_done = 1;
    Wait(0);
}

static struct Task *create_task(const char *name, APTR entry, APTR *stack_out)
{
    struct Task *task;
    APTR stack;

    stack = AllocMem(4096, MEMF_CLEAR);
    if (!stack)
        return NULL;

    task = (struct Task *)AllocMem(sizeof(struct Task), MEMF_CLEAR | MEMF_PUBLIC);
    if (!task)
    {
        FreeMem(stack, 4096);
        return NULL;
    }

    task->tc_Node.ln_Type = NT_TASK;
    task->tc_Node.ln_Pri = 0;
    task->tc_Node.ln_Name = (char *)name;
    task->tc_SPLower = stack;
    task->tc_SPUpper = (APTR)((UBYTE *)stack + 4096);
    task->tc_SPReg = task->tc_SPUpper;

    AddTask(task, entry, NULL);

    if (stack_out)
        *stack_out = stack;

    return task;
}

int main(void)
{
    int errors = 0;
    struct Task *me;

    print("Testing QBlit/QBSBlit/WaitBlit/OwnBlitter/DisownBlitter...\n");

    me = FindTask(NULL);
    if (!me || !GfxBase)
    {
        print("FAIL: Missing current task or GfxBase\n");
        return 20;
    }

    /* Note: OwnBlitter() does not nest on AmigaOS (a second call by the
     * owner deadlocks), and the GfxBase blitter bookkeeping fields
     * (BlitOwner, BlitNest, BlitLock, blthd, bsblthd) are private, so only
     * the observable behaviour is tested. */
    print("Test 1: OwnBlitter/DisownBlitter...\n");
    OwnBlitter();
    DisownBlitter();
    print("OK: OwnBlitter()/DisownBlitter() returned\n");
    print("\nTest 2: QBlit immediate execution and cleanup...\n");
    {
        struct bltnode node;

        reset_events();
        node.n = NULL;
        node.function = qblit_func_a;
        node.stat = CLEANUP;
        node.blitsize = 0;
        node.beamsync = 0;
        node.cleanup = cleanup_func;

        QBlit(&node);
        WaitBlit();

        if (g_event_count != 2 || g_events[0] != 1 || g_events[1] != 3)
        {
            print("FAIL: QBlit() immediate execution/cleanup order incorrect\n");
            errors++;
        }
        else
        {
            print("OK: QBlit() executed function and cleanup\n");
        }
    }

    print("\nTest 3: QBlit/QBSBlit queued while the blitter is owned...\n");
    {
        struct bltnode q_node;
        struct bltnode qbs_node;

        reset_events();
        OwnBlitter();

        q_node.n = NULL;
        q_node.function = qblit_func_a;
        q_node.stat = 0;
        q_node.blitsize = 0;
        q_node.beamsync = 0;
        q_node.cleanup = NULL;

        qbs_node.n = NULL;
        qbs_node.function = qblit_func_b;
        qbs_node.stat = 0;
        qbs_node.blitsize = 0;
        qbs_node.beamsync = 0;
        qbs_node.cleanup = NULL;

        QBlit(&q_node);
        QBSBlit(&qbs_node);

        Delay(2);
        if (g_event_count != 0)
        {
            print("FAIL: blits should stay queued while blitter is owned\n");
            errors++;
        }
        else
        {
            print("OK: queued blits stayed pending while owned\n");
        }

        DisownBlitter();
        WaitBlit();

        Delay(2);
        if (g_event_count != 2 || g_events[0] != 1 || g_events[1] != 2)
        {
            print("FAIL: queued QBlit()/QBSBlit() did not run in order after DisownBlitter()\n");
            errors++;
        }
        else
        {
            print("OK: queued QBlit() and QBSBlit() ran in order after DisownBlitter()\n");
        }
    }

    print("\nTest 4: WaitBlit does not wait for the blitter owner...\n");
    {
        struct Task *child;
        APTR child_stack = NULL;
        struct bltnode node;

        reset_events();
        OwnBlitter();

        child = create_task("WaitBlitChild", (APTR)waitblit_child_task, &child_stack);
        if (!child)
        {
            print("FAIL: Could not create WaitBlit child task\n");
            errors++;
            DisownBlitter();
        }
        else
        {
            Delay(3);

            /* WaitBlit() only waits for the running blit to finish; it does
             * not wait for another task's OwnBlitter() to end */
            if (g_child_done != 1)
            {
                print("FAIL: WaitBlit() child blocked while blitter was owned\n");
                errors++;
            }
            else
            {
                print("OK: WaitBlit() child returned while blitter was owned\n");
            }

            node.n = NULL;
            node.function = qblit_func_a;
            node.stat = 0;
            node.blitsize = 0;
            node.beamsync = 0;
            node.cleanup = NULL;
            QBlit(&node);

            DisownBlitter();
            Delay(3);

            if (g_event_count != 2 || g_events[0] != 4 || g_events[1] != 1)
            {
                print("FAIL: queued blit did not run after DisownBlitter()\n");
                errors++;
            }
            else
            {
                print("OK: queued blit ran after DisownBlitter()\n");
            }

            Forbid();
            RemTask(child);
            Permit();
            FreeMem(child_stack, 4096);
            FreeMem(child, sizeof(struct Task));
        }
    }

    if (errors == 0)
    {
        print("PASS: Blitter queue tests passed\n");
        return 0;
    }

    print("FAIL: Blitter queue tests had ");
    print_num(errors);
    print(" error(s)\n");
    return 20;
}
