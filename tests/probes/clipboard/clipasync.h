/*
 * clipasync.h - clipboard.device probe helpers.  On AmigaOS 3.1 some
 * clipboard requests block (an unfinished read holds off writes, a write
 * in progress holds off reads), so every request is issued by a helper
 * process with DoIO() and the probe waits for it with a bounded Delay()
 * loop: the probe never hangs on either system.
 */
#ifndef PROBE_CLIPASYNC_H
#define PROBE_CLIPASYNC_H

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <dos/dostags.h>
#include <devices/clipboard.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

#define NJOBS 6

struct Job {
    struct IOClipReq *req;
    struct MsgPort *home;       /* the request's own reply port */
    volatile LONG busy;         /* 1 while the helper process runs */
};

static struct Job jobs[NJOBS];
static LONG base;               /* clip IDs are printed relative to this */

static void job_body(struct Job *j)
{
    struct MsgPort *mp = CreateMsgPort();
    if (mp) {
        j->req->io_Message.mn_ReplyPort = mp;
        DoIO((struct IORequest *)j->req);
        j->req->io_Message.mn_ReplyPort = j->home;
        DeleteMsgPort(mp);
    }
    Forbid();
    j->busy = 0;
}

static void job0(void) { job_body(&jobs[0]); }
static void job1(void) { job_body(&jobs[1]); }
static void job2(void) { job_body(&jobs[2]); }
static void job3(void) { job_body(&jobs[3]); }
static void job4(void) { job_body(&jobs[4]); }
static void job5(void) { job_body(&jobs[5]); }
static void (*const job_fn[NJOBS])(void) = { job0, job1, job2, job3, job4, job5 };

/* start 'cmd' on request slot 'n' (asynchronously); FALSE if still busy */
static BOOL start(int n, UWORD cmd)
{
    struct Job *j = &jobs[n];
    if (j->busy)
        return FALSE;
    j->req->io_Command = cmd;
    j->req->io_Error = 0;
    j->req->io_Flags = 0;
    j->busy = 1;
    if (!CreateNewProcTags(NP_Entry, (ULONG)job_fn[n], NP_Name, (ULONG)"clipprobe-io",
                           NP_StackSize, 8192, NP_Output, 0, NP_Input, 0,
                           NP_CloseOutput, FALSE, NP_CloseInput, FALSE, TAG_END)) {
        j->busy = 0;
        return FALSE;
    }
    return TRUE;
}

/* wait at most 'ticks' ticks for slot 'n'; TRUE when its request is done */
static BOOL wait_done(int n, int ticks)
{
    int i;
    for (i = 0; i < ticks && jobs[n].busy; i++)
        Delay(1);
    if (jobs[n].busy)
        return FALSE;
    Delay(1);                   /* let the helper process finish exiting */
    return TRUE;
}

/* a request with a 2 s bound */
static BOOL run(int n, UWORD cmd)
{
    if (!start(n, cmd)) {
        probe_s("  (request busy)\n");
        return FALSE;
    }
    if (wait_done(n, 100))
        return TRUE;
    probe_s("  (request did not complete: command ");
    probe_dec(cmd);
    probe_s(")\n");
    return FALSE;
}

static void p_id(LONG id)
{
    if (id == -1 || id == 0) {
        probe_dec(id);
        return;
    }
    probe_s("base");
    if (id - base >= 0)
        probe_ch('+');
    probe_dec(id - base);
}

static void P_ID(const char *label, LONG id)
{
    probe_s(label);
    probe_s(" = ");
    p_id(id);
    probe_ch('\n');
}

static void show(const char *label, int n)
{
    struct IOClipReq *r = jobs[n].req;
    probe_s(label);
    if (jobs[n].busy) {
        probe_s(": pending\n");
        return;
    }
    probe_s(": err=");
    probe_dec(r->io_Error);
    probe_s(" actual=");
    probe_dec(r->io_Actual);
    probe_s(" offset=");
    probe_dec(r->io_Offset);
    probe_s(" clip=");
    p_id(r->io_ClipID);
    probe_ch('\n');
}

static void setup(int n, LONG clip, LONG offset, APTR data, LONG len)
{
    struct IOClipReq *r = jobs[n].req;
    r->io_ClipID = clip;
    r->io_Offset = offset;
    r->io_Data = (STRPTR)data;
    r->io_Length = len;
}

static BOOL open_job(int n, struct MsgPort *port, ULONG unit)
{
    struct IOClipReq *r = (struct IOClipReq *)CreateIORequest(port, sizeof(struct IOClipReq));
    if (!r)
        return FALSE;
    if (OpenDevice((STRPTR)"clipboard.device", unit, (struct IORequest *)r, 0) != 0) {
        DeleteIORequest((struct IORequest *)r);
        return FALSE;
    }
    jobs[n].req = r;
    jobs[n].home = port;
    jobs[n].busy = 0;
    return TRUE;
}

static void close_job(int n)
{
    if (jobs[n].req && !jobs[n].busy) {
        CloseDevice((struct IORequest *)jobs[n].req);
        DeleteIORequest((struct IORequest *)jobs[n].req);
        jobs[n].req = NULL;
    }
}

/* the current read (or write) ID, asked through slot n */
static LONG cur_id(int n, UWORD cmd)
{
    jobs[n].req->io_ClipID = 12345;
    if (!run(n, cmd))
        return 0;
    return jobs[n].req->io_ClipID;
}

static void ids(const char *label, int n)
{
    LONG r = cur_id(n, CBD_CURRENTREADID);
    LONG w = cur_id(n, CBD_CURRENTWRITEID);
    probe_s(label);
    probe_s(": readid=");
    p_id(r);
    probe_s(" writeid=");
    p_id(w);
    probe_ch('\n');
}

#endif
