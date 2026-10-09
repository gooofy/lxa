/*
 * lxaprobe - RDD guest agent for the AmigaOS 3.1 reference machine
 * (lxa roadmap Phase 211).
 *
 * Started from S:Startup-Sequence when LXAREF: (the host exchange
 * directory) is mounted.  Speaks a line protocol on serial.device unit 0:
 * one command per line, zero or more reply lines, then "OK" or "ERR <why>".
 * Bulk data (trees, snapshots, logs) is written to files in LXAREF:.
 *
 *   PING                         -> PONG
 *   RUN [>file] [STACK n] <cmdline>  start a program asynchronously (CLI
 *                                process, current dir = the program's
 *                                directory, stack n bytes, default 32768);
 *                                >file sends its output to LXAREF:file
 *   ASSIGN <name> <path> [ADD]   create or extend a logical assign
 *   DELAY <ticks>                let <ticks> frames (1/50 s) pass
 *   WAIT_WINDOW <substr> [ms]    wait for a window whose title contains substr
 *   WAIT_IDLE [ms]               wait until the program's tasks all wait and
 *                                their window ports are empty
 *   DUMP_TREE <file>             screens/windows/gadgets/menus as JSON
 *                                (schema lxa-tree/1, doc/rdd-snapshot.md)
 *   SNAP <file> [WINDOW <n>]     pen-index snapshot (screen or window n)
 *   MOVE x y | CLICK x y [L|R|M] | PRESS x y [L|R|M] | RELEASE x y [L|R|M]
 *   KEY rawkey [qualifier]       raw key down+up
 *   TYPE <text>                  type text (keymap.library MapANSI)
 *   MENU <Menu/Item[/Sub]>       select a menu entry of the active window
 *   TEXT_START | TEXT_STOP | TEXT_DUMP <file>   Text() hook log
 *   TRACE <lib> <lvo,lvo,...> | TRACE_DUMP <file> | TRACE_STOP
 *   MODES <file>                 display mode database as JSON
 *   WAIT_EXIT [ms]               wait until the program started by RUN ends
 *                                -> RC <return code>; ERR held when a System
 *                                Request appeared (crash), ERR timeout otherwise
 *   QUIT [ms]                    close the program's windows, CTRL-C its
 *                                process, report survivors + memory delta
 *   SETCLOCK <unix-seconds>      set the system time (lxa's deterministic
 *                                epoch is 1704067200 = 2024-01-01 00:00)
 *   BYE                          stop the agent
 *
 * Built by tools/refsys/build_refsys.sh with m68k-amigaos-gcc (libnix).
 * This program runs only on the reference machine; it uses only public
 * AmigaOS APIs (clean-room: observation, not implementation).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <devices/serial.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/keymap_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include <inline/keymap.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *KeymapBase;

static const char version[] = "$VER: lxaprobe 1.0 (02.10.2026)";

/* ------------------------------------------------------------------ */
/* Serial line I/O                                                     */
/* ------------------------------------------------------------------ */

static struct MsgPort *ser_port;
static struct IOExtSer *ser_req;

static BOOL ser_open(void)
{
    ser_port = CreateMsgPort();
    if (!ser_port)
        return FALSE;
    ser_req = (struct IOExtSer *)CreateIORequest(ser_port, sizeof(struct IOExtSer));
    if (!ser_req)
        return FALSE;
    ser_req->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
    if (OpenDevice((STRPTR)SERIALNAME, 0, (struct IORequest *)ser_req, 0))
        return FALSE;
    ser_req->io_Baud = 115200;
    ser_req->io_ReadLen = 8;
    ser_req->io_WriteLen = 8;
    ser_req->io_StopBits = 1;
    ser_req->io_SerFlags = SERF_SHARED | SERF_XDISABLED;
    ser_req->IOSer.io_Command = SDCMD_SETPARAMS;
    DoIO((struct IORequest *)ser_req);
    return TRUE;
}

static void ser_write(const char *s, LONG len)
{
    ser_req->IOSer.io_Command = CMD_WRITE;
    ser_req->IOSer.io_Data = (APTR)s;
    ser_req->IOSer.io_Length = len;
    DoIO((struct IORequest *)ser_req);
}

static void reply(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n > (int)sizeof(buf) - 2)
        n = sizeof(buf) - 2;
    buf[n++] = '\n';
    ser_write(buf, n);
}

static int ser_readline(char *buf, int max)
{
    int n = 0;
    char c;
    for (;;)
    {
        ser_req->IOSer.io_Command = CMD_READ;
        ser_req->IOSer.io_Data = &c;
        ser_req->IOSer.io_Length = 1;
        if (DoIO((struct IORequest *)ser_req) || ser_req->IOSer.io_Actual != 1)
            continue;
        if (c == '\r')
            continue;
        if (c == '\n')
            break;
        if (n < max - 1)
            buf[n++] = c;
    }
    buf[n] = 0;
    return n;
}

/* ------------------------------------------------------------------ */
/* Buffered output to LXAREF: files                                    */
/* ------------------------------------------------------------------ */

static BPTR out_fh;
static char out_buf[4096];
static int out_len;

static BOOL out_open(const char *name)
{
    char path[256];
    snprintf(path, sizeof(path), "LXAREF:%s", name);
    out_fh = Open((STRPTR)path, MODE_NEWFILE);
    out_len = 0;
    return out_fh != 0;
}

static void out_flush(void)
{
    if (out_len && out_fh)
        Write(out_fh, out_buf, out_len);
    out_len = 0;
}

static void out_raw(const void *p, LONG len)
{
    const char *c = p;
    while (len > 0)
    {
        LONG n = sizeof(out_buf) - out_len;
        if (n > len)
            n = len;
        memcpy(out_buf + out_len, c, n);
        out_len += n;
        c += n;
        len -= n;
        if (out_len == sizeof(out_buf))
            out_flush();
    }
}

static void out_printf(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > (int)sizeof(buf) - 1)
        n = sizeof(buf) - 1;
    if (n > 0)
        out_raw(buf, n);
}

static void out_jstr(const char *s)
{
    out_raw("\"", 1);
    if (s)
    {
        for (; *s; s++)
        {
            unsigned char c = (unsigned char)*s;
            if (c == '"' || c == '\\')
            {
                out_raw("\\", 1);
                out_raw(s, 1);
            }
            else if (c < 0x20)
                out_printf("\\u%04x", c);
            else if (c >= 0x80)
            {
                /* Latin-1 -> UTF-8 */
                char u[2];
                u[0] = (char)(0xC0 | (c >> 6));
                u[1] = (char)(0x80 | (c & 0x3F));
                out_raw(u, 2);
            }
            else
                out_raw(s, 1);
        }
    }
    out_raw("\"", 1);
}

static void out_close(void)
{
    out_flush();
    if (out_fh)
        Close(out_fh);
    out_fh = 0;
}

/* ------------------------------------------------------------------ */
/* Program tracking                                                    */
/* ------------------------------------------------------------------ */

static struct Process *app_proc;
static volatile LONG app_rc;
static volatile BOOL app_exited;

/* NP_ExitCode: called in the dying process with the return code in d0 */
static LONG exit_hook(LONG rc __asm("d0"), LONG data __asm("d1"))
{
    struct Process *me = (struct Process *)FindTask(NULL);

    (void)data;
    /* the program's segments are the CLI's cli_Module, as when a shell
     * runs it: a program that detaches (CreateProc() of its own code,
     * Maxon/SAS "detach" startup) clears cli_Module to keep them */
    if (me->pr_CLI)
    {
        struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(me->pr_CLI);
        if (cli->cli_Module)
            UnLoadSeg(cli->cli_Module);
        cli->cli_Module = 0;
    }
    app_rc = rc;
    app_exited = TRUE;
    return rc;
}
static ULONG mem_before;
static struct Window *known_windows[64];
static int known_count;

static BOOL task_alive(struct Task *t)
{
    struct Node *n;
    BOOL found = FALSE;
    if (!t)
        return FALSE;
    Forbid();
    if (t == SysBase->ThisTask)
        found = TRUE;
    for (n = SysBase->TaskReady.lh_Head; !found && n->ln_Succ; n = n->ln_Succ)
        if ((struct Task *)n == t)
            found = TRUE;
    for (n = SysBase->TaskWait.lh_Head; !found && n->ln_Succ; n = n->ln_Succ)
        if ((struct Task *)n == t)
            found = TRUE;
    Permit();
    return found;
}

/* windows that did not exist before RUN belong to the program */
static BOOL is_app_window(struct Window *w)
{
    int i;
    for (i = 0; i < known_count; i++)
        if (known_windows[i] == w)
            return FALSE;
    return TRUE;
}

static void snapshot_windows(void)
{
    struct Screen *s;
    struct Window *w;
    ULONG lock = LockIBase(0);
    known_count = 0;
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
        for (w = s->FirstWindow; w && known_count < 64; w = w->NextWindow)
            known_windows[known_count++] = w;
    UnlockIBase(lock);
}

/* Expunge unused libraries, devices and fonts so that AvailMem() deltas
 * measure the program, not caches (an impossible allocation does this). */
static ULONG avail_flushed(void)
{
    APTR p = AllocMem(0x7FFFFFF0, MEMF_PUBLIC);
    if (p)
        FreeMem(p, 0x7FFFFFF0);
    return AvailMem(MEMF_ANY);
}

static void delay_ticks(LONG ticks)
{
    Delay(ticks > 0 ? ticks : 1);
}

static void cmd_run(char *args)
{
    char prog[256], outname[160];
    char *p = args, *d = prog;
    BPTR seg, lock, dir, outfh;
    char *slash;
    LONG stack = 32768;

    while (*p == ' ')
        p++;
    outname[0] = 0;
    if (*p == '>')
    {
        int k = 0;
        p++;
        while (*p && *p != ' ' && k < (int)sizeof(outname) - 1)
            outname[k++] = *p++;
        outname[k] = 0;
        while (*p == ' ')
            p++;
    }
    if (!strncmp(p, "STACK ", 6))
    {
        p += 6;
        stack = strtol(p, &p, 10);
        if (stack < 4096)
            stack = 4096;
        while (*p == ' ')
            p++;
    }
    if (*p == '"')
    {
        p++;
        while (*p && *p != '"' && d < prog + sizeof(prog) - 1)
            *d++ = *p++;
        if (*p == '"')
            p++;
    }
    else
        while (*p && *p != ' ' && d < prog + sizeof(prog) - 1)
            *d++ = *p++;
    *d = 0;
    while (*p == ' ')
        p++;

    seg = LoadSeg((STRPTR)prog);
    if (!seg)
    {
        reply("ERR cannot load %s (%ld)", prog, IoErr());
        return;
    }

    /* current directory = the program's directory (as lxa does) */
    slash = strrchr(prog, '/');
    if (!slash)
        slash = strrchr(prog, ':');
    dir = 0;
    if (slash)
    {
        char c = slash[1];
        slash[1] = 0;
        if (*slash == '/')
            *slash = 0;
        lock = Lock((STRPTR)prog, SHARED_LOCK);
        dir = lock;
        if (*slash == 0)
            *slash = '/';
        slash[1] = c;
    }

    snapshot_windows();
    mem_before = avail_flushed();
    if (outname[0])
    {
        char path[200];
        snprintf(path, sizeof(path), "LXAREF:%s", outname);
        outfh = Open((STRPTR)path, MODE_NEWFILE);
    }
    else
        outfh = Open((STRPTR)"NIL:", MODE_NEWFILE);

    app_exited = FALSE;
    app_rc = -1;
    {
        static char argline[512];
        snprintf(argline, sizeof(argline), "%s\n", p);
        /* the child must not run before its CLI says how large its
         * stack is (cli_DefaultStack, as the STACK command sets it):
         * C startup codes (Maxon, SAS) derive their stack bound from it */
        Forbid();
        app_proc = CreateNewProcTags(NP_Seglist, (ULONG)seg,
                                     NP_FreeSeglist, FALSE,   /* cli_Module: see exit_hook */
                                     NP_Name, (ULONG)FilePart((STRPTR)prog),
                                     NP_CurrentDir, (ULONG)dir,
                                     NP_HomeDir, (ULONG)(dir ? DupLock(dir) : 0),
                                     NP_Input, (ULONG)Open((STRPTR)"NIL:", MODE_OLDFILE),
                                     NP_Output, (ULONG)outfh,
                                     NP_CloseInput, TRUE,
                                     NP_CloseOutput, TRUE,
                                     NP_Cli, TRUE,
                                     /* GetProgramName() must return the program, not the
                                      * agent (DPaint reads its own executable) */
                                     NP_CommandName, (ULONG)prog,
                                     NP_Arguments, (ULONG)argline,
                                     NP_StackSize, stack,
                                     NP_ExitCode, (ULONG)exit_hook,
                                     TAG_DONE);
        if (app_proc && app_proc->pr_CLI)
        {
            struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(app_proc->pr_CLI);
            cli->cli_DefaultStack = stack / 4;
            cli->cli_Module = seg;
        }
        Permit();
    }
    if (!app_proc)
    {
        UnLoadSeg(seg);
        if (dir)
            UnLock(dir);
        reply("ERR CreateNewProc failed");
        return;
    }
    reply("PROC %08lx", (ULONG)app_proc);
    reply("OK");
}

static struct Window *find_window(const char *substr)
{
    struct Screen *s;
    struct Window *w, *found = NULL;
    ULONG lock = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s && !found; s = s->NextScreen)
        for (w = s->FirstWindow; w; w = w->NextWindow)
            if (w->Title && strstr((char *)w->Title, substr))
            {
                found = w;
                break;
            }
    UnlockIBase(lock);
    return found;
}

static void cmd_wait_window(char *args)
{
    char sub[128];
    LONG ms = 10000, waited = 0;
    char *sp = strrchr(args, ' ');
    struct Window *w;

    snprintf(sub, sizeof(sub), "%s", args);
    if (sp && sp[1] >= '0' && sp[1] <= '9')
    {
        ms = atol(sp + 1);
        sub[sp - args] = 0;
    }
    while (!(w = find_window(sub)))
    {
        if (waited >= ms)
        {
            reply("ERR timeout waiting for window '%s'", sub);
            return;
        }
        delay_ticks(2);
        waited += 40;
    }
    reply("WINDOW %08lx %d %d %d %d", (ULONG)w, w->LeftEdge, w->TopEdge, w->Width, w->Height);
    reply("OK");
}

static BOOL app_idle(void)
{
    struct Screen *s;
    struct Window *w;
    BOOL idle = TRUE;
    struct Node *n;

    if (!app_proc || !task_alive(&app_proc->pr_Task))
        return TRUE;
    Forbid();
    for (n = SysBase->TaskReady.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if ((struct Task *)n == &app_proc->pr_Task)
            idle = FALSE;
    Permit();
    if (!idle)
        return FALSE;
    {
        ULONG lock = LockIBase(0);
        for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
            for (w = s->FirstWindow; w; w = w->NextWindow)
                if (is_app_window(w) && w->UserPort &&
                    w->UserPort->mp_MsgList.lh_Head->ln_Succ)
                    idle = FALSE;
        UnlockIBase(lock);
    }
    return idle;
}

static void cmd_wait_idle(char *args)
{
    LONG ms = (args && *args) ? atol(args) : 10000;
    LONG waited = 0;
    int stable = 0;
    while (stable < 5)
    {
        if (app_idle())
            stable++;
        else
            stable = 0;
        if (waited >= ms)
        {
            reply("ERR not idle after %ld ms", ms);
            return;
        }
        delay_ticks(1);
        waited += 20;
    }
    reply("OK");
}

/* ------------------------------------------------------------------ */
/* DUMP_TREE                                                           */
/* ------------------------------------------------------------------ */

static void dump_font(struct TextFont *f)
{
    if (!f)
    {
        out_printf("null");
        return;
    }
    out_printf("{\"name\":");
    out_jstr(f->tf_Message.mn_Node.ln_Name);
    out_printf(",\"ysize\":%d,\"xsize\":%d,\"baseline\":%d}", f->tf_YSize, f->tf_XSize, f->tf_Baseline);
}

static void dump_intuitext(struct IntuiText *it)
{
    int first = 1;
    out_printf("[");
    for (; it; it = it->NextText)
    {
        out_printf("%s{\"text\":", first ? "" : ",");
        out_jstr((char *)it->IText);
        out_printf(",\"left\":%d,\"top\":%d,\"fpen\":%d,\"bpen\":%d,\"drawmode\":%d}",
                   it->LeftEdge, it->TopEdge, it->FrontPen, it->BackPen, it->DrawMode);
        first = 0;
    }
    out_printf("]");
}

static void dump_gadgets(struct Gadget *g)
{
    int first = 1;
    out_printf("[");
    for (; g; g = g->NextGadget)
    {
        out_printf("%s{\"id\":%d,\"type\":%d,\"flags\":%d,\"activation\":%d,"
                   "\"left\":%d,\"top\":%d,\"width\":%d,\"height\":%d,\"text\":",
                   first ? "" : ",", g->GadgetID, g->GadgetType, g->Flags, g->Activation,
                   g->LeftEdge, g->TopEdge, g->Width, g->Height);
        if ((g->Flags & GFLG_LABELMASK) == GFLG_LABELITEXT)
            dump_intuitext(g->GadgetText);
        else
            out_printf("[]");
        out_printf("}");
        first = 0;
    }
    out_printf("]");
}

static void dump_items(struct MenuItem *it)
{
    int first = 1;
    out_printf("[");
    for (; it; it = it->NextItem)
    {
        out_printf("%s{\"left\":%d,\"top\":%d,\"width\":%d,\"height\":%d,\"flags\":%d,"
                   "\"mutualexclude\":%ld,\"command\":%d,\"text\":",
                   first ? "" : ",", it->LeftEdge, it->TopEdge, it->Width, it->Height,
                   it->Flags, it->MutualExclude, (it->Flags & COMMSEQ) ? it->Command : 0);
        if ((it->Flags & ITEMTEXT) && it->ItemFill)
            out_jstr((char *)((struct IntuiText *)it->ItemFill)->IText);
        else
            out_printf("null");
        out_printf(",\"sub\":");
        dump_items(it->SubItem);
        out_printf("}");
        first = 0;
    }
    out_printf("]");
}

static void dump_menus(struct Menu *m)
{
    int first = 1;
    out_printf("[");
    for (; m; m = m->NextMenu)
    {
        out_printf("%s{\"title\":", first ? "" : ",");
        out_jstr((char *)m->MenuName);
        out_printf(",\"left\":%d,\"top\":%d,\"width\":%d,\"height\":%d,\"flags\":%d,\"items\":",
                   m->LeftEdge, m->TopEdge, m->Width, m->Height, m->Flags);
        dump_items(m->FirstItem);
        out_printf("}");
        first = 0;
    }
    out_printf("]");
}

static void dump_window(struct Window *w)
{
    out_printf("{\"title\":");
    out_jstr((char *)w->Title);
    out_printf(",\"screen_title\":");
    out_jstr((char *)w->ScreenTitle);
    out_printf(",\"app\":%s,\"left\":%d,\"top\":%d,\"width\":%d,\"height\":%d,"
               "\"min_width\":%d,\"min_height\":%d,\"max_width\":%d,\"max_height\":%d,"
               "\"flags\":%lu,\"idcmp\":%lu,\"border\":[%d,%d,%d,%d],\"detail_pen\":%d,\"block_pen\":%d,"
               "\"font\":",
               is_app_window(w) ? "true" : "false",
               w->LeftEdge, w->TopEdge, w->Width, w->Height,
               w->MinWidth, w->MinHeight, (int)w->MaxWidth, (int)w->MaxHeight,
               w->Flags, w->IDCMPFlags,
               w->BorderLeft, w->BorderTop, w->BorderRight, w->BorderBottom,
               w->DetailPen, w->BlockPen);
    dump_font(w->RPort ? w->RPort->Font : NULL);
    out_printf(",\"gadgets\":");
    dump_gadgets(w->FirstGadget);
    out_printf(",\"menus\":");
    dump_menus(w->MenuStrip);
    out_printf("}");
}

static void cmd_dump_tree(char *file)
{
    struct Screen *s;
    struct Window *w;
    int fs = 1, fw;
    ULONG lock;

    if (!out_open(file))
    {
        reply("ERR cannot open LXAREF:%s", file);
        return;
    }
    lock = LockIBase(0);
    out_printf("{\"schema\":\"lxa-tree/1\",\"backend\":\"ref\",\"screens\":[");
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
    {
        ULONG modeid = GetVPModeID(&s->ViewPort);
        out_printf("%s{\"title\":", fs ? "" : ",");
        out_jstr((char *)s->Title);
        out_printf(",\"default_title\":");
        out_jstr((char *)s->DefaultTitle);
        out_printf(",\"left\":%d,\"top\":%d,\"width\":%d,\"height\":%d,\"depth\":%d,"
                   "\"display_id\":%lu,\"flags\":%d,\"bar_height\":%d,\"bar_vborder\":%d,"
                   "\"bar_hborder\":%d,\"menu_vborder\":%d,\"menu_hborder\":%d,"
                   "\"wbor\":[%d,%d,%d,%d],\"font\":",
                   s->LeftEdge, s->TopEdge, s->Width, s->Height, s->BitMap.Depth,
                   modeid, s->Flags, s->BarHeight, s->BarVBorder, s->BarHBorder,
                   s->MenuVBorder, s->MenuHBorder,
                   s->WBorLeft, s->WBorTop, s->WBorRight, s->WBorBottom);
        dump_font(s->RastPort.Font);
        out_printf(",\"windows\":[");
        fw = 1;
        for (w = s->FirstWindow; w; w = w->NextWindow)
        {
            if (!fw)
                out_printf(",");
            dump_window(w);
            fw = 0;
        }
        out_printf("]}");
        fs = 0;
    }
    out_printf("]}\n");
    UnlockIBase(lock);
    out_close();
    reply("OK");
}

/* ------------------------------------------------------------------ */
/* SNAP: pen-index snapshot                                            */
/*   "LXASNAP1 <w> <h> <depth> <ncolors>\n" + ncolors*3 RGB + w*h pens  */
/* ------------------------------------------------------------------ */

static void cmd_snap(char *args)
{
    char file[128];
    struct Screen *s = IntuitionBase->FirstScreen;   /* the front screen */
    struct RastPort *rp;
    WORD x0 = 0, y0 = 0, w, h;
    int ncolors, i, x, y;
    UBYTE *line;

    if (sscanf(args, "%127s", file) != 1)
    {
        reply("ERR usage: SNAP <file> [WINDOW <n>]");
        return;
    }
    rp = &s->RastPort;
    w = s->Width;
    h = s->Height;
    {
        char *wp = strstr(args, "WINDOW");
        if (wp)
        {
            int n = atoi(wp + 6), k = 0;
            struct Window *win;
            for (win = s->FirstWindow; win && k < n; win = win->NextWindow)
                k++;
            if (!win)
            {
                reply("ERR no window %d", n);
                return;
            }
            x0 = win->LeftEdge;
            y0 = win->TopEdge;
            w = win->Width;
            h = win->Height;
        }
    }
    if (!out_open(file))
    {
        reply("ERR cannot open LXAREF:%s", file);
        return;
    }
    ncolors = 1 << s->BitMap.Depth;
    if (ncolors > 256)
        ncolors = 256;
    out_printf("LXASNAP1 %d %d %d %d\n", w, h, s->BitMap.Depth, ncolors);
    for (i = 0; i < ncolors; i++)
    {
        ULONG rgb[3];
        UBYTE c[3];
        GetRGB32(s->ViewPort.ColorMap, i, 1, rgb);
        c[0] = rgb[0] >> 24;
        c[1] = rgb[1] >> 24;
        c[2] = rgb[2] >> 24;
        out_raw(c, 3);
    }
    line = AllocVec(w + 16, MEMF_ANY);
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
            line[x] = (UBYTE)ReadPixel(rp, x0 + x, y0 + y);
        out_raw(line, w);
    }
    FreeVec(line);
    out_close();
    reply("OK");
}

/* ------------------------------------------------------------------ */
/* Input injection                                                     */
/* ------------------------------------------------------------------ */

static struct MsgPort *in_port;
static struct IOStdReq *in_req;

static BOOL input_open(void)
{
    in_port = CreateMsgPort();
    in_req = (struct IOStdReq *)CreateIORequest(in_port, sizeof(struct IOStdReq));
    return in_req && !OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)in_req, 0);
}

static void send_event(struct InputEvent *ie)
{
    in_req->io_Command = IND_WRITEEVENT;
    in_req->io_Data = ie;
    in_req->io_Length = sizeof(struct InputEvent);
    DoIO((struct IORequest *)in_req);
}

static void move_to(WORD x, WORD y, UWORD qual)
{
    struct InputEvent ie;
    struct IEPointerPixel pp;
    memset(&ie, 0, sizeof(ie));
    pp.iepp_Screen = IntuitionBase->FirstScreen;   /* coordinates on the front screen */
    pp.iepp_Position.X = x;
    pp.iepp_Position.Y = y;
    ie.ie_Class = IECLASS_NEWPOINTERPOS;
    ie.ie_SubClass = IESUBCLASS_PIXEL;
    ie.ie_Code = IECODE_NOBUTTON;
    ie.ie_Qualifier = qual;
    ie.ie_EventAddress = &pp;
    send_event(&ie);
}

static UWORD button_code(char b)
{
    return b == 'R' ? IECODE_RBUTTON : b == 'M' ? IECODE_MBUTTON : IECODE_LBUTTON;
}

static UWORD button_qual(char b)
{
    return b == 'R' ? IEQUALIFIER_RBUTTON : b == 'M' ? IEQUALIFIER_MIDBUTTON : IEQUALIFIER_LEFTBUTTON;
}

static void button(char b, BOOL down)
{
    struct InputEvent ie;
    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWMOUSE;
    ie.ie_Code = button_code(b) | (down ? 0 : IECODE_UP_PREFIX);
    ie.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE | (down ? button_qual(b) : 0);
    send_event(&ie);
}

/* the keyboard's key-down history: real RAWKEY events carry the previous
 * two key downs (ie_Prev1Down*, ie_Prev2Down*; IntuiMessage IAddress
 * points at them) - injected events must too */
static UBYTE kb_p1c, kb_p1q, kb_p2c, kb_p2q;

static void rawkey(UWORD code, UWORD qual)
{
    struct InputEvent ie;
    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = code;
    ie.ie_Qualifier = qual;
    ie.ie_Prev1DownCode = kb_p1c;
    ie.ie_Prev1DownQual = kb_p1q;
    ie.ie_Prev2DownCode = kb_p2c;
    ie.ie_Prev2DownQual = kb_p2q;
    send_event(&ie);
    kb_p2c = kb_p1c;
    kb_p2q = kb_p1q;
    kb_p1c = (UBYTE)code;
    kb_p1q = (UBYTE)qual;
    delay_ticks(1);
    ie.ie_Code = code | IECODE_UP_PREFIX;
    ie.ie_Prev1DownCode = kb_p1c;
    ie.ie_Prev1DownQual = kb_p1q;
    ie.ie_Prev2DownCode = kb_p2c;
    ie.ie_Prev2DownQual = kb_p2q;
    send_event(&ie);
    delay_ticks(1);
}

static void cmd_mouse(const char *cmd, char *args)
{
    int x = 0, y = 0;
    char b = 'L';
    if (sscanf(args, "%d %d %c", &x, &y, &b) < 2)
    {
        reply("ERR usage: %s x y [L|R|M]", cmd);
        return;
    }
    if (!strcmp(cmd, "MOVE"))
        move_to(x, y, 0);
    else if (!strcmp(cmd, "PRESS"))
    {
        move_to(x, y, 0);
        delay_ticks(1);
        button(b, TRUE);
    }
    else if (!strcmp(cmd, "RELEASE"))
    {
        move_to(x, y, button_qual(b));
        delay_ticks(1);
        button(b, FALSE);
    }
    else
    {
        move_to(x, y, 0);
        delay_ticks(1);
        button(b, TRUE);
        delay_ticks(2);
        button(b, FALSE);
    }
    delay_ticks(2);
    reply("OK");
}

static void cmd_key(char *args)
{
    unsigned int code = 0, qual = 0;
    if (sscanf(args, "%x %x", &code, &qual) < 1)
    {
        reply("ERR usage: KEY <rawkey-hex> [qualifier-hex]");
        return;
    }
    rawkey(code, qual);
    reply("OK");
}

static void cmd_type(char *args)
{
    if (!KeymapBase)
    {
        reply("ERR no keymap.library");
        return;
    }
    for (; *args; args++)
    {
        UBYTE ev[8];
        LONG n = MapANSI((STRPTR)args, 1, (STRPTR)ev, 4, NULL);
        if (n >= 1)
            rawkey(ev[0], ev[1]);
    }
    reply("OK");
}

/* menu geometry: the menu bar is BarHeight+1 high, items hang below it at
 * menu->LeftEdge (+BarHBorder), sub-items to the right of their parent */
static BOOL label_eq(const char *a, const char *b, int blen)
{
    int alen = strlen(a);
    while (alen && a[alen - 1] == ' ')
        alen--;
    if (alen >= 3 && !strncmp(a + alen - 3, "...", 3))
        alen -= 3;
    if (blen >= 3 && !strncmp(b + blen - 3, "...", 3))
        blen -= 3;
    return alen == blen && !strnicmp(a, b, blen);
}

static void cmd_menu(char *path)
{
    struct Window *w = IntuitionBase->ActiveWindow;
    struct Screen *s;
    struct Menu *m;
    struct MenuItem *it = NULL, *sub = NULL, *first;
    char *seg[3];
    int len[3], nseg = 0;
    char *p = path;
    WORD mx, my, ix, iy, sx = 0, sy = 0;

    if (!w || !w->MenuStrip)
    {
        reply("ERR active window has no menus");
        return;
    }
    s = w->WScreen;
    while (nseg < 3)
    {
        char *sl = strchr(p, '/');
        seg[nseg] = p;
        len[nseg] = sl ? sl - p : (int)strlen(p);
        nseg++;
        if (!sl)
            break;
        p = sl + 1;
    }
    for (m = w->MenuStrip; m; m = m->NextMenu)
        if (label_eq((char *)m->MenuName, seg[0], len[0]))
            break;
    if (!m || nseg < 2)
    {
        reply("ERR menu not found");
        return;
    }
    for (it = m->FirstItem; it; it = it->NextItem)
        if ((it->Flags & ITEMTEXT) && it->ItemFill &&
            label_eq((char *)((struct IntuiText *)it->ItemFill)->IText, seg[1], len[1]))
            break;
    if (!it)
    {
        reply("ERR item not found");
        return;
    }
    if (nseg == 3)
    {
        for (sub = it->SubItem; sub; sub = sub->NextItem)
            if ((sub->Flags & ITEMTEXT) && sub->ItemFill &&
                label_eq((char *)((struct IntuiText *)sub->ItemFill)->IText, seg[2], len[2]))
                break;
        if (!sub)
        {
            reply("ERR sub-item not found");
            return;
        }
    }
    first = m->FirstItem;
    mx = s->BarHBorder + m->LeftEdge + m->Width / 2;
    my = s->BarHeight / 2;
    ix = s->BarHBorder + m->LeftEdge + (it->LeftEdge - first->LeftEdge) + it->Width / 2;
    iy = s->BarHeight + 1 + it->TopEdge + it->Height / 2;
    if (sub)
    {
        sx = s->BarHBorder + m->LeftEdge + it->LeftEdge + it->Width +
             (sub->LeftEdge - it->SubItem->LeftEdge) + sub->Width / 2;
        sy = iy - it->Height / 2 + sub->TopEdge + sub->Height / 2;
    }
    move_to(mx, my, 0);
    delay_ticks(1);
    button('R', TRUE);
    delay_ticks(3);
    move_to(ix, iy, IEQUALIFIER_RBUTTON);
    delay_ticks(3);
    if (sub)
    {
        move_to(sx, iy, IEQUALIFIER_RBUTTON);
        delay_ticks(3);
        move_to(sx, sy, IEQUALIFIER_RBUTTON);
        delay_ticks(3);
    }
    button('R', FALSE);
    delay_ticks(3);
    reply("OK");
}

/* ------------------------------------------------------------------ */
/* Text() hook                                                          */
/* ------------------------------------------------------------------ */

#define TEXTLOG_MAX 4096
struct text_rec { WORD x, y; UBYTE len; char s[61]; };
static struct text_rec *textlog;
static volatile ULONG text_count;
static APTR orig_text;
static BOOL text_on;

static LONG call_orig_text(struct RastPort *rp, CONST_STRPTR str, ULONG count)
{
    register LONG d0 __asm("d0") = count;
    register struct RastPort *a1 __asm("a1") = rp;
    register CONST_STRPTR a0 __asm("a0") = str;
    register struct GfxBase *a6 __asm("a6") = GfxBase;
    register APTR a2 __asm("a2") = orig_text;
    __asm volatile ("jsr (%%a2)"
                    : "+r"(d0), "+r"(a1), "+r"(a0)
                    : "r"(a6), "r"(a2)
                    : "d1", "cc", "memory");
    return d0;
}

static LONG text_patch(register struct RastPort *rp __asm("a1"),
                       register CONST_STRPTR str __asm("a0"),
                       register ULONG count __asm("d0"),
                       register struct GfxBase *gb __asm("a6"))
{
    (void)gb;
    if (text_on && textlog && str && count)
    {
        ULONG i;
        Disable();
        i = text_count;
        if (i < TEXTLOG_MAX)
            text_count = i + 1;
        Enable();
        if (i < TEXTLOG_MAX)
        {
            struct text_rec *r = &textlog[i];
            UWORD n = (UWORD)count;
            if (n > 60)
                n = 60;
            r->x = rp->cp_x;
            r->y = rp->cp_y;
            r->len = n;
            memcpy(r->s, str, n);
            r->s[n] = 0;
        }
    }
    return call_orig_text(rp, str, count & 0xFFFF);
}

static void cmd_text(const char *cmd, char *args)
{
    if (!strcmp(cmd, "TEXT_START"))
    {
        if (!textlog)
            textlog = AllocVec(sizeof(struct text_rec) * TEXTLOG_MAX, MEMF_PUBLIC | MEMF_CLEAR);
        if (!orig_text)
        {
            Forbid();
            orig_text = SetFunction((struct Library *)GfxBase, -60, (APTR)text_patch);
            CacheClearU();
            Permit();
        }
        text_count = 0;
        text_on = TRUE;
    }
    else if (!strcmp(cmd, "TEXT_STOP"))
        text_on = FALSE;
    else
    {
        ULONG i;
        if (!out_open(args))
        {
            reply("ERR cannot open LXAREF:%s", args);
            return;
        }
        for (i = 0; textlog && i < text_count; i++)
        {
            out_printf("{\"text\":");
            out_jstr(textlog[i].s);
            out_printf(",\"x\":%d,\"y\":%d}\n", textlog[i].x, textlog[i].y);
        }
        out_close();
    }
    reply("OK");
}

/* ------------------------------------------------------------------ */
/* TRACE: SetFunction relay that logs arguments and return values      */
/* ------------------------------------------------------------------ */

#define TRACE_MAX_FN   64
#define TRACE_LOG_MAX  4096
struct trace_fn { struct Library *lib; WORD lvo; APTR orig; UWORD *stub; char libname[24]; };
/* str[i]: printable C string that d1, d2, a0, a1 point to (if any) */
struct trace_rec { UBYTE fn; UBYTE ret; struct Task *task; ULONG regs[15]; char str[4][40]; };

static void trace_str(char *dst, ULONG addr)
{
    const UBYTE *p = (const UBYTE *)addr;
    int i;
    dst[0] = 0;
    if (addr < 0x400 || !TypeOfMem((APTR)addr))
        return;
    for (i = 0; i < 39; i++)
    {
        if (!p[i])
            break;
        if (p[i] < 0x20 || (p[i] >= 0x7F && p[i] < 0xA0))
        {
            dst[0] = 0;
            return;
        }
        dst[i] = p[i];
    }
    dst[i] = 0;          /* longer strings are truncated to 39 chars */
}
static struct trace_fn trace_fns[TRACE_MAX_FN];
static int trace_nfn;
static struct trace_rec *tracelog;
static volatile ULONG trace_count;
static BOOL trace_on;

/* called from the relay stubs with the saved d0-d7/a0-a6 */
static void trace_entry(register ULONG idx __asm("d0"), register ULONG *regs __asm("a0"))
{
    ULONG i;
    if (!trace_on || !tracelog)
        return;
    Disable();
    i = trace_count;
    if (i < TRACE_LOG_MAX)
        trace_count = i + 1;
    Enable();
    if (i < TRACE_LOG_MAX)
    {
        tracelog[i].fn = (UBYTE)idx;
        tracelog[i].ret = 0;
        tracelog[i].task = SysBase->ThisTask;
        memcpy(tracelog[i].regs, regs, 15 * 4);
        trace_str(tracelog[i].str[0], regs[1]);   /* d1 */
        trace_str(tracelog[i].str[1], regs[2]);   /* d2 */
        trace_str(tracelog[i].str[2], regs[8]);   /* a0 */
        trace_str(tracelog[i].str[3], regs[9]);   /* a1 */
    }
}

static void trace_exit(register ULONG idx __asm("d1"), register ULONG d0 __asm("d0"))
{
    ULONG i;
    if (!trace_on || !tracelog)
        return;
    Disable();
    i = trace_count;
    if (i < TRACE_LOG_MAX)
        trace_count = i + 1;
    Enable();
    if (i < TRACE_LOG_MAX)
    {
        tracelog[i].fn = (UBYTE)idx;
        tracelog[i].ret = 1;
        tracelog[i].task = SysBase->ThisTask;
        tracelog[i].regs[0] = d0;
    }
}

/*
 * Relay stub (built at runtime, 68000 code):
 *   movem.l d0-d7/a0-a6,-(sp)      48E7 FFFE
 *   move.l  sp,a0                  204F
 *   moveq   #idx,d0  (move.l #idx,d0: 203C xxxx xxxx)
 *   jsr     trace_entry            4EB9 xxxx xxxx
 *   movem.l (sp)+,d0-d7/a0-a6      4CDF 7FFF
 *   jsr     orig                   4EB9 xxxx xxxx
 *   movem.l d0-d1/a0-a1,-(sp)      48E7 C0C0
 *   move.l  #idx,d1                223C xxxx xxxx
 *   jsr     trace_exit             4EB9 xxxx xxxx
 *   movem.l (sp)+,d0-d1/a0-a1      4CDF 0303
 *   rts                            4E75
 */
static UWORD *make_stub(int idx, APTR orig)
{
    UWORD *c = AllocVec(64, MEMF_PUBLIC | MEMF_CLEAR), *p = c;
    if (!c)
        return NULL;
#define L(v) do { ULONG _v = (ULONG)(v); *p++ = _v >> 16; *p++ = _v & 0xFFFF; } while (0)
    *p++ = 0x48E7; *p++ = 0xFFFE;
    *p++ = 0x204F;
    *p++ = 0x203C; L(idx);
    *p++ = 0x4EB9; L(trace_entry);
    *p++ = 0x4CDF; *p++ = 0x7FFF;
    *p++ = 0x4EB9; L(orig);
    *p++ = 0x48E7; *p++ = 0xC0C0;
    *p++ = 0x223C; L(idx);
    *p++ = 0x4EB9; L(trace_exit);
    *p++ = 0x4CDF; *p++ = 0x0303;
    *p++ = 0x4E75;
#undef L
    return c;
}

static void cmd_trace(char *args)
{
    char lib[40], list[256], *p;
    struct Library *base;
    if (sscanf(args, "%39s %255s", lib, list) != 2)
    {
        reply("ERR usage: TRACE <lib> <lvo,lvo,...>");
        return;
    }
    base = OpenLibrary((STRPTR)lib, 0);
    if (!base)
    {
        reply("ERR cannot open %s", lib);
        return;
    }
    if (!tracelog)
        tracelog = AllocVec(sizeof(struct trace_rec) * TRACE_LOG_MAX, MEMF_PUBLIC | MEMF_CLEAR);
    for (p = strtok(list, ","); p && trace_nfn < TRACE_MAX_FN; p = strtok(NULL, ","))
    {
        LONG lvo = atol(p);
        struct trace_fn *f = &trace_fns[trace_nfn];
        if (lvo > 0)
            lvo = -lvo;
        f->lib = base;
        f->lvo = (WORD)lvo;
        snprintf(f->libname, sizeof(f->libname), "%s", lib);
        f->stub = make_stub(trace_nfn, NULL);
        Forbid();
        f->orig = SetFunction(base, lvo, (APTR)f->stub);
        /* patch the jsr orig target now that it is known */
        f->stub[12] = (ULONG)f->orig >> 16;
        f->stub[13] = (ULONG)f->orig & 0xFFFF;
        CacheClearU();
        Permit();
        trace_nfn++;
    }
    trace_count = 0;
    trace_on = TRUE;
    reply("OK");
}

static void cmd_trace_dump(char *file)
{
    ULONG i;
    if (!out_open(file))
    {
        reply("ERR cannot open LXAREF:%s", file);
        return;
    }
    for (i = 0; i < trace_count; i++)
    {
        struct trace_rec *r = &tracelog[i];
        struct trace_fn *f = &trace_fns[r->fn];
        out_printf("{\"lib\":\"%s\",\"lvo\":%d,\"task\":", f->libname, f->lvo);
        out_jstr(r->task ? r->task->tc_Node.ln_Name : "");
        if (r->ret)
            out_printf(",\"ret\":%lu}\n", r->regs[0]);
        else
        {
            static const char *names[4] = {"d1", "d2", "a0", "a1"};
            int k;
            out_printf(",\"d\":[%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu],\"a\":[%lu,%lu,%lu,%lu],\"str\":{",
                       r->regs[0], r->regs[1], r->regs[2], r->regs[3], r->regs[4], r->regs[5],
                       r->regs[6], r->regs[7], r->regs[8], r->regs[9], r->regs[10], r->regs[11]);
            for (k = 0; k < 4; k++)
            {
                if (k)
                    out_printf(",");
                out_printf("\"%s\":", names[k]);
                if (r->str[k][0])
                    out_jstr(r->str[k]);
                else
                    out_printf("null");
            }
            out_printf("}}\n");
        }
    }
    out_close();
    reply("OK");
}

static void cmd_trace_stop(void)
{
    int i;
    trace_on = FALSE;
    Forbid();
    for (i = trace_nfn - 1; i >= 0; i--)
        SetFunction(trace_fns[i].lib, trace_fns[i].lvo, trace_fns[i].orig);
    CacheClearU();
    Permit();
    /* stubs are kept: a task may still be inside one */
    trace_nfn = 0;
    reply("OK");
}

/* ------------------------------------------------------------------ */
/* MODES                                                               */
/* ------------------------------------------------------------------ */

static void cmd_modes(char *file)
{
    ULONG id = INVALID_ID;
    int first = 1;
    if (!out_open(file))
    {
        reply("ERR cannot open LXAREF:%s", file);
        return;
    }
    out_printf("{\"schema\":\"lxa-modes/1\",\"modes\":[");
    while ((id = NextDisplayInfo(id)) != INVALID_ID)
    {
        DisplayInfoHandle h = FindDisplayInfo(id);
        struct NameInfo ni;
        struct DimensionInfo di;
        struct DisplayInfo dinfo;
        if (!h)
            continue;
        memset(&ni, 0, sizeof(ni));
        memset(&di, 0, sizeof(di));
        memset(&dinfo, 0, sizeof(dinfo));
        GetDisplayInfoData(h, (UBYTE *)&ni, sizeof(ni), DTAG_NAME, 0);
        GetDisplayInfoData(h, (UBYTE *)&di, sizeof(di), DTAG_DIMS, 0);
        GetDisplayInfoData(h, (UBYTE *)&dinfo, sizeof(dinfo), DTAG_DISP, 0);
        out_printf("%s{\"id\":%lu,\"name\":", first ? "" : ",", id);
        out_jstr((char *)ni.Name);
        out_printf(",\"max_depth\":%d,\"nominal\":[%d,%d,%d,%d],\"min_raster\":[%d,%d],"
                   "\"max_raster\":[%d,%d],\"property_flags\":%lu,\"not_available\":%d}",
                   di.MaxDepth, di.Nominal.MinX, di.Nominal.MinY, di.Nominal.MaxX, di.Nominal.MaxY,
                   di.MinRasterWidth, di.MinRasterHeight, di.MaxRasterWidth, di.MaxRasterHeight,
                   dinfo.PropertyFlags, dinfo.NotAvailable);
        first = 0;
    }
    out_printf("]}\n");
    out_close();
    reply("OK");
}

/* ------------------------------------------------------------------ */
/* QUIT                                                                */
/* ------------------------------------------------------------------ */

static void cmd_wait_exit(char *args)
{
    LONG ms = (args && *args) ? atol(args) : 10000, waited = 0;

    if (!app_proc)
    {
        reply("ERR nothing running");
        return;
    }
    while (!app_exited && waited < ms)
    {
        delay_ticks(1);
        waited += 20;
    }
    if (!app_exited)
    {
        /* a crashed task is held by the "Software Failure" system requester */
        struct Screen *s;
        struct Window *w;
        BOOL held = FALSE;
        ULONG lock = LockIBase(0);
        for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
            for (w = s->FirstWindow; w; w = w->NextWindow)
                if (w->Title && !strcmp((char *)w->Title, "System Request") && is_app_window(w))
                    held = TRUE;
        UnlockIBase(lock);
        reply(held ? "ERR held" : "ERR timeout");
        return;
    }
    while (task_alive(&app_proc->pr_Task))
        delay_ticks(1);
    app_proc = NULL;
    reply("RC %ld", app_rc);
    reply("OK");
}

static void cmd_quit(char *args)
{
    LONG ms = (args && *args) ? atol(args) : 5000, waited = 0;
    struct Screen *s;
    struct Window *w;

    if (!app_proc)
    {
        reply("ERR nothing running");
        return;
    }
    /* close gadget on every window of the program, like a user would */
    {
        ULONG lock = LockIBase(0);
        for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
            for (w = s->FirstWindow; w; w = w->NextWindow)
                if (is_app_window(w) && (w->Flags & WFLG_CLOSEGADGET))
                {
                    WORD x = w->LeftEdge + w->BorderLeft / 2 + 5;
                    WORD y = w->TopEdge + w->BorderTop / 2;
                    UnlockIBase(lock);
                    move_to(x, y, 0);
                    delay_ticks(1);
                    button('L', TRUE);
                    delay_ticks(1);
                    button('L', FALSE);
                    delay_ticks(5);
                    lock = LockIBase(0);
                    break;
                }
        UnlockIBase(lock);
    }
    while (task_alive(&app_proc->pr_Task) && waited < ms / 2)
    {
        delay_ticks(5);
        waited += 100;
    }
    if (task_alive(&app_proc->pr_Task))
        Signal(&app_proc->pr_Task, SIGBREAKF_CTRL_C);
    while (task_alive(&app_proc->pr_Task) && waited < ms)
    {
        delay_ticks(5);
        waited += 100;
    }
    reply("SURVIVOR %s", task_alive(&app_proc->pr_Task) ? "yes" : "no");
    delay_ticks(10);
    reply("MEMDELTA %ld", (LONG)mem_before - (LONG)avail_flushed());
    if (!task_alive(&app_proc->pr_Task))
        app_proc = NULL;
    reply("OK");
}

/* ------------------------------------------------------------------ */
/* ASSIGN                                                              */
/* ------------------------------------------------------------------ */

static void cmd_assign(char *args)
{
    char name[64], path[256], mode[8];
    BPTR lock;
    int n = sscanf(args, "%63s %255s %7s", name, path, mode);
    BOOL ok;

    if (n < 2)
    {
        reply("ERR usage: ASSIGN <name> <path> [ADD]");
        return;
    }
    if (name[strlen(name) - 1] == ':')
        name[strlen(name) - 1] = 0;
    lock = Lock((STRPTR)path, SHARED_LOCK);
    if (!lock)
    {
        reply("ERR no such directory %s", path);
        return;
    }
    ok = (n == 3 && !stricmp(mode, "ADD")) ? AssignAdd((STRPTR)name, lock)
                                             : AssignLock((STRPTR)name, lock);
    if (!ok)
    {
        UnLock(lock);
        reply("ERR assign %s: failed", name);
        return;
    }
    reply("OK");
}

/* ------------------------------------------------------------------ */
/* SETCLOCK                                                            */
/* ------------------------------------------------------------------ */

static void cmd_setclock(char *args)
{
    struct MsgPort *port = CreateMsgPort();
    struct timerequest *tr = (struct timerequest *)CreateIORequest(port, sizeof(*tr));
    ULONG unix_secs = strtoul(args, NULL, 10);
    /* Amiga time starts 1978-01-01, 252460800 s after the Unix epoch */
    if (!tr || OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0) || unix_secs < 252460800UL)
    {
        reply("ERR cannot set clock");
    }
    else
    {
        tr->tr_node.io_Command = TR_SETSYSTIME;
        tr->tr_time.tv_secs = unix_secs - 252460800UL;
        tr->tr_time.tv_micro = 0;
        DoIO((struct IORequest *)tr);
        CloseDevice((struct IORequest *)tr);
        reply("OK");
    }
    if (tr)
        DeleteIORequest((struct IORequest *)tr);
    DeleteMsgPort(port);
}

/* ------------------------------------------------------------------ */
/* main loop                                                           */
/* ------------------------------------------------------------------ */

int main(void)
{
    char line[600];
    (void)version;

    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    KeymapBase = OpenLibrary((STRPTR)"keymap.library", 37);
    if (!GfxBase || !IntuitionBase || !ser_open() || !input_open())
        return 20;

    SetTaskPri(SysBase->ThisTask, 5);
    reply("LXAPROBE READY");

    for (;;)
    {
        char *cmd = line, *args;
        ser_readline(line, sizeof(line));
        if (!line[0])
            continue;
        args = strchr(line, ' ');
        if (args)
            *args++ = 0;
        else
            args = line + strlen(line);

        if (!strcmp(cmd, "PING"))
        {
            reply("PONG");
            reply("OK");
        }
        else if (!strcmp(cmd, "RUN"))
            cmd_run(args);
        else if (!strcmp(cmd, "WAIT_EXIT"))
            cmd_wait_exit(args);
        else if (!strcmp(cmd, "WAIT_WINDOW"))
            cmd_wait_window(args);
        else if (!strcmp(cmd, "WAIT_IDLE"))
            cmd_wait_idle(args);
        else if (!strcmp(cmd, "DUMP_TREE"))
            cmd_dump_tree(args);
        else if (!strcmp(cmd, "SNAP"))
            cmd_snap(args);
        else if (!strcmp(cmd, "MOVE") || !strcmp(cmd, "CLICK") ||
                 !strcmp(cmd, "PRESS") || !strcmp(cmd, "RELEASE"))
            cmd_mouse(cmd, args);
        else if (!strcmp(cmd, "KEY"))
            cmd_key(args);
        else if (!strcmp(cmd, "TYPE"))
            cmd_type(args);
        else if (!strcmp(cmd, "MENU"))
            cmd_menu(args);
        else if (!strncmp(cmd, "TEXT_", 5))
            cmd_text(cmd, args);
        else if (!strcmp(cmd, "TRACE"))
            cmd_trace(args);
        else if (!strcmp(cmd, "TRACE_DUMP"))
            cmd_trace_dump(args);
        else if (!strcmp(cmd, "TRACE_STOP"))
            cmd_trace_stop();
        else if (!strcmp(cmd, "MODES"))
            cmd_modes(args);
        else if (!strcmp(cmd, "QUIT"))
            cmd_quit(args);
        else if (!strcmp(cmd, "SETCLOCK"))
            cmd_setclock(args);
        else if (!strcmp(cmd, "ASSIGN"))
            cmd_assign(args);
        else if (!strcmp(cmd, "DELAY"))
        {
            delay_ticks(atol(args));
            reply("OK");
        }
        else if (!strcmp(cmd, "BYE"))
        {
            reply("OK");
            break;
        }
        else
            reply("ERR unknown command %s", cmd);
    }
    return 0;
}
