/*
 * ARexx - the ARexx interpreter process of lxa (Phase 250)
 *
 * RexxMast starts one of these for every RXCOMM or RXFUNC packet, with
 * the packet in tc_UserData, the client's streams and current directory.  The program runs on Regina (src/regina, LGPL) with ARexx
 * semantics: host commands go to public ports as RexxMsg packets (or to
 * the shell for ADDRESS COMMAND), functions are searched in the Library
 * List (function hosts and libraries), then as program files in the
 * current directory and REXX:.  The process replies the packet itself.
 *
 * The Regina side of the ARexx personality is src/regina/lxa_hooks.c.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <rexx/errors.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/rexxsyslib.h>

/* Regina's SAA API; LONG and ULONG come from exec/types.h */
#define LONG_TYPEDEFED
#define ULONG_TYPEDEFED
#define INCL_RXSHV
#define INCL_RXFUNC
#define INCL_RXSYSEXIT
#include "rexxsaa.h"

struct RxsLib *RexxSysBase = NULL;

#include "../Rexxc/rexx_common.h"
#include "lxa_arexx.h"

unsigned long __stack = LXA_REXX_STACK;

static const char version[] = "$VER: ARexx 36.21 (01.11.2026)";

static struct RexxMsg *job;          /* the packet we run */
static struct MsgPort *host_reply;   /* replies of our host packets */
static char msg_name[] = "REXX";     /* ln_Name of ARexx packets */

static int opt_results = 0;          /* OPTIONS RESULTS */
static long opt_failat = RC_ERROR;   /* OPTIONS FAILAT */
static char *opt_prompt = NULL;      /* OPTIONS PROMPT */

static char *src = NULL;             /* the program text */
static long src_len = 0;
static char prog_called[256] = "?";  /* PARSE SOURCE: called name */
static char prog_resolved[256] = "?";/* PARSE SOURCE: resolved name */
static char source_tail[600];

static const char *job_ext(void)
{
    return job->rm_FileExt ? (const char *)job->rm_FileExt : "REXX";
}

static const char *job_host(void)
{
    return job->rm_CommAddr ? (const char *)job->rm_CommAddr : "REXX";
}

/* ---- called by lxa_hooks.c ---------------------------------------------- */

const char *lxa_ax_line(int lineno, int *len)
{
    long i = 0, line = 1, start;

    if (!src || lineno < 1)
        return NULL;
    while (i < src_len && line < lineno) {
        if (src[i++] == '\n')
            line++;
    }
    if (line != lineno)
        return NULL;
    start = i;
    while (i < src_len && src[i] != '\n')
        i++;
    *len = (int)(i - start);
    return src + start;
}

void lxa_ax_set_results(int on)
{
    opt_results = on;
}

void lxa_ax_set_failat(long failat)
{
    opt_failat = failat;
}

void lxa_ax_set_prompt(const char *s, int len)
{
    free(opt_prompt);
    opt_prompt = NULL;
    if (len > 0 && (opt_prompt = malloc(len + 1)) != NULL) {
        memcpy(opt_prompt, s, len);
        opt_prompt[len] = 0;
    }
}

void lxa_ax_reset_options(void)
{
    opt_results = 0;
    opt_failat = RC_ERROR;
    lxa_ax_set_prompt(NULL, 0);
}

int lxa_ax_show(char opt, const char *name, int len)
{
    char buf[256];
    int found = 0;

    if (len >= (int)sizeof(buf))
        return 0;
    memcpy(buf, name, len);
    buf[len] = 0;
    switch (opt) {
    case 'P':
        Forbid();
        found = FindPort((STRPTR)buf) != NULL;
        Permit();
        break;
    case 'L':
        LockRexxBase(0);
        found = lxa_rexx_find(&RexxSysBase->rl_LibList, buf, len) != NULL;
        UnlockRexxBase(0);
        break;
    case 'C':
        LockRexxBase(0);
        found = lxa_rexx_find(&RexxSysBase->rl_ClipList, buf, len) != NULL;
        UnlockRexxBase(0);
        break;
    }
    return found;
}

/* names of a list, separated by <sep> (malloc()ed) */
static char *list_names(struct List *list, const char *sep, int seplen)
{
    struct Node *n;
    long size = 1;
    char *s;

    for (n = list->lh_Head; n->ln_Succ; n = n->ln_Succ)
        size += lxa_rexx_strlen(n->ln_Name) + seplen;
    s = malloc(size);
    if (!s)
        return NULL;
    s[0] = 0;
    for (n = list->lh_Head; n->ln_Succ; n = n->ln_Succ) {
        if (s[0])
            strncat(s, sep, seplen);
        if (n->ln_Name)
            strcat(s, n->ln_Name);
    }
    return s;
}

char *lxa_ax_show_list(char opt, const char *sep, int seplen)
{
    char *s = NULL;

    switch (opt) {
    case 'P':
        Forbid();
        s = list_names(&SysBase->PortList, sep, seplen);
        Permit();
        break;
    case 'L':
        LockRexxBase(0);
        s = list_names(&RexxSysBase->rl_LibList, sep, seplen);
        UnlockRexxBase(0);
        break;
    case 'C':
        LockRexxBase(0);
        s = list_names(&RexxSysBase->rl_ClipList, sep, seplen);
        UnlockRexxBase(0);
        break;
    }
    return s;
}

const char *lxa_ax_source(const char *file, int len)
{
    const char *called = prog_called, *resolved = prog_resolved;
    char fbuf[256];

    /* an external function file of the program */
    if (file && len > 0 && len < (int)sizeof(fbuf) && strcmp(prog_called, "?") != 0) {
        memcpy(fbuf, file, len);
        fbuf[len] = 0;
        if (strcmp(fbuf, prog_called) != 0 && strcmp(fbuf, prog_resolved) != 0)
            called = resolved = fbuf;
    }
    snprintf(source_tail, sizeof(source_tail), "%d %s %s %s %s",
             (job->rm_Action & RXFF_RESULT) ? 1 : 0, called, resolved, job_ext(), job_host());
    return source_tail;
}

/* is <path> a program file (starting with a comment)? */
static int is_program(const char *path)
{
    char head[2];
    BPTR fh = Open((STRPTR)path, MODE_OLDFILE);
    LONG n;

    if (!fh)
        return -1;
    n = Read(fh, head, 2);
    Close(fh);
    return (n == 2 && head[0] == '/' && head[1] == '*') ? 1 : 0;
}

int lxa_ax_locate(const char *name, int len, char *buf, int bufsize)
{
    const char *dirs[2] = { "", "REXX:" };
    const char *exts[3];
    int ndirs, next = 0, d, e, i;
    const char *ext = job_ext();

    while (len > 0 && name[0] == ' ') {
        name++;
        len--;
    }
    if (len <= 0)
        return 0;
    ndirs = 2;
    for (i = 0; i < len; i++)
        if (name[i] == ':' || name[i] == '/')
            ndirs = 1;              /* a path: only there */
    if (strcasecmp(ext, "rexx") != 0)
        exts[next++] = ext;
    exts[next++] = "rexx";
    exts[next++] = NULL;            /* the name as given */

    for (d = 0; d < ndirs; d++) {
        for (e = 0; e < next; e++) {
            int r;
            if (exts[e])
                r = snprintf(buf, bufsize, "%s%.*s.%s", dirs[d], len, name, exts[e]);
            else
                r = snprintf(buf, bufsize, "%s%.*s", dirs[d], len, name);
            if (r < 0 || r >= bufsize)
                continue;
            r = is_program(buf);
            if (r == 1)
                return 1;
            if (r == 0)
                return 0;           /* a file without the leading comment */
        }
    }
    return 0;
}

/* ---- variables ----------------------------------------------------------- */

static void set_var(const char *name, const char *value, long len)
{
    SHVBLOCK shv;

    memset(&shv, 0, sizeof(shv));
    shv.shvname.strptr = (char *)name;
    shv.shvname.strlength = strlen(name);
    if (value) {
        shv.shvcode = RXSHV_SYSET;
        shv.shvvalue.strptr = (char *)value;
        shv.shvvalue.strlength = len;
    } else {
        shv.shvcode = RXSHV_SYDRO;
    }
    RexxVariablePool(&shv);
}

/* ---- packets to hosts ----------------------------------------------------- */

static struct RexxMsg *new_packet(LONG action)
{
    struct RexxMsg *rm = CreateRexxMsg(host_reply, job->rm_FileExt, job->rm_CommAddr);

    if (!rm)
        return NULL;
    rm->rm_Node.mn_Node.ln_Name = msg_name;
    rm->rm_TaskBlock = (APTR)job;
    rm->rm_LibBase = (APTR)RexxSysBase;
    rm->rm_Action = action;
    rm->rm_Stdin = (LONG)Input();
    rm->rm_Stdout = (LONG)Output();
    return rm;
}

static void free_packet(struct RexxMsg *rm)
{
    ClearRexxMsg(rm, MAXRMARG + 1);
    DeleteRexxMsg(rm);
}

/* put a packet to a public port and wait for the reply; FALSE: no port */
static BOOL send_packet(const char *port_name, struct RexxMsg *rm)
{
    struct MsgPort *port;

    fflush(stdout);
    Forbid();
    port = FindPort((STRPTR)port_name);
    if (port)
        PutMsg(port, (struct Message *)rm);
    Permit();
    if (!port)
        return FALSE;
    while (!GetMsg(host_reply))
        WaitPort(host_reply);
    return TRUE;
}

static void set_retc(PRXSTRING retc, const char *s, long len)
{
    if (!s) {
        retc->strptr = NULL;
        retc->strlength = 0;
        return;
    }
    if (!retc->strptr || (long)retc->strlength < len) {
        retc->strptr = RexxAllocateMemory(len + 1);
        if (!retc->strptr) {
            retc->strlength = 0;
            return;
        }
    }
    memcpy(retc->strptr, s, len);
    retc->strlength = len;
}

/* ---- RXCMD: host commands -------------------------------------------- */

static LONG APIENTRY cmd_exit(LONG fn, LONG sub, PEXIT parm)
{
    RXCMDHST_PARM *p = (RXCMDHST_PARM *)parm;
    char env[256], num[16];
    char *cmd;
    long rc;
    int envlen = p->rxcmd_addressl;

    (void)fn;
    (void)sub;
    if (envlen >= (int)sizeof(env))
        envlen = sizeof(env) - 1;
    memcpy(env, p->rxcmd_address, envlen);
    env[envlen] = 0;
    cmd = malloc(p->rxcmd_command.strlength + 1);
    if (!cmd)
        lxa_arexx_raise(3);
    memcpy(cmd, p->rxcmd_command.strptr, p->rxcmd_command.strlength);
    cmd[p->rxcmd_command.strlength] = 0;

    if (strcasecmp(env, "COMMAND") == 0) {
        fflush(stdout);
        rc = SystemTags((STRPTR)cmd, SYS_Input, (ULONG)Input(), SYS_Output, (ULONG)Output(),
                        TAG_DONE);
        if (rc == -1)
            rc = RC_FATAL;
    } else {
        struct RexxMsg *rm = new_packet(RXCOMM | (opt_results ? RXFF_RESULT : 0));
        if (!rm || !(rm->rm_Args[0] = (STRPTR)CreateArgstring((STRPTR)cmd, p->rxcmd_command.strlength))) {
            if (rm)
                DeleteRexxMsg(rm);
            free(cmd);
            lxa_arexx_raise(3);
        }
        if (!send_packet(env, rm)) {
            free_packet(rm);
            free(cmd);
            lxa_arexx_raise(13);    /* host environment not found */
        }
        rc = rm->rm_Result1;
        if (opt_results) {
            if (rc == 0 && rm->rm_Result2) {
                UBYTE *r = (UBYTE *)rm->rm_Result2;
                set_var("RESULT", (char *)r, LengthArgstring(r));
                DeleteArgstring(r);
            } else {
                set_var("RESULT", NULL, 0);
            }
        }
        free_packet(rm);
    }
    free(cmd);

    sprintf(num, "%ld", rc);
    set_retc(&p->rxcmd_retc, num, strlen(num));
    p->rxcmd_flags.rxfcfail = (rc != 0 && rc >= opt_failat) ? 1 : 0;
    p->rxcmd_flags.rxfcerr = (rc != 0 && rc < opt_failat) ? 1 : 0;
    return RXEXIT_HANDLED;
}

/* ---- RXFNC: ARexx functions and the Library List ------------------------ */

static long arg_long(PRXSTRING a)
{
    char buf[16];
    long n = a->strptr ? (long)a->strlength : 0;

    if (n >= (long)sizeof(buf))
        n = sizeof(buf) - 1;
    memcpy(buf, a->strptr ? a->strptr : "", n);
    buf[n] = 0;
    return atol(buf);
}

static int fn_is(RXFNCCAL_PARM *p, const char *name)
{
    return lxa_rexx_name_eq(name, (const char *)p->rxfnc_name, p->rxfnc_namel);
}

static LONG ret_bool(RXFNCCAL_PARM *p, int b)
{
    set_retc(&p->rxfnc_retc, b ? "1" : "0", 1);
    return RXEXIT_HANDLED;
}

/* call a function library's query entry: d0 = rc, a0 = result */
static LONG call_query(struct Library *base, LONG offset, struct RexxMsg *rm, UBYTE **result)
{
    register LONG d0 __asm("d0");
    register struct RexxMsg *a0 __asm("a0") = rm;
    register struct Library *a6 __asm("a6") = base;
    register APTR a1 __asm("a1") = (APTR)((UBYTE *)base + offset);

    __asm volatile ("jsr (%%a1)"
                    : "=r" (d0), "+r" (a0), "+r" (a1)
                    : "r" (a6)
                    : "d1", "cc", "memory");
    *result = (UBYTE *)a0;
    return d0;
}

struct lib_entry {
    char name[64];
    UBYTE type;
    LONG offset, version;
};

static LONG search_libraries(RXFNCCAL_PARM *p)
{
    struct lib_entry *libs = NULL;
    struct Node *n;
    int count = 0, i, j;

    LockRexxBase(0);
    for (n = RexxSysBase->rl_LibList.lh_Head; n->ln_Succ; n = n->ln_Succ)
        count++;
    if (count)
        libs = malloc(count * sizeof(*libs));
    count = 0;
    if (libs) {
        for (n = RexxSysBase->rl_LibList.lh_Head; n->ln_Succ; n = n->ln_Succ, count++) {
            struct RexxRsrc *rr = (struct RexxRsrc *)n;
            strncpy(libs[count].name, n->ln_Name ? n->ln_Name : "", sizeof(libs[count].name) - 1);
            libs[count].name[sizeof(libs[count].name) - 1] = 0;
            libs[count].type = n->ln_Type;
            libs[count].offset = rr->rr_Arg1;
            libs[count].version = rr->rr_Arg2;
        }
    }
    UnlockRexxBase(0);

    for (i = 0; i < count; i++) {
        struct RexxMsg *rm;
        int nargs = p->rxfnc_argc > MAXRMARG ? MAXRMARG : p->rxfnc_argc;
        LONG rc;
        UBYTE *result = NULL;

        rm = new_packet(RXFUNC | RXFF_RESULT | nargs);
        if (!rm)
            break;
        rm->rm_Args[0] = (STRPTR)CreateArgstring((STRPTR)p->rxfnc_name, p->rxfnc_namel);
        for (j = 0; j < nargs; j++)
            if (p->rxfnc_argv[j].strptr)
                rm->rm_Args[j + 1] = (STRPTR)CreateArgstring((STRPTR)p->rxfnc_argv[j].strptr,
                                                             p->rxfnc_argv[j].strlength);

        if (libs[i].type == RRT_LIB) {
            struct Library *base = OpenLibrary((STRPTR)libs[i].name, libs[i].version);
            if (!base) {
                free_packet(rm);
                free(libs);
                lxa_arexx_raise(14);    /* requested library not found */
            }
            rc = call_query(base, libs[i].offset, rm, &result);
            CloseLibrary(base);
            if (rc == 1) {              /* not in this library */
                free_packet(rm);
                continue;
            }
        } else {
            if (!send_packet(libs[i].name, rm)) {
                free_packet(rm);
                continue;               /* the host went away */
            }
            rc = rm->rm_Result1;
            result = rc == 0 ? (UBYTE *)rm->rm_Result2 : NULL;
        }
        free_packet(rm);
        free(libs);
        if (rc != 0)
            lxa_arexx_raise(12);        /* error return from function */
        if (result) {
            set_retc(&p->rxfnc_retc, (char *)result, LengthArgstring(result));
            DeleteArgstring(result);
        } else {
            set_retc(&p->rxfnc_retc, NULL, 0);
        }
        return RXEXIT_HANDLED;
    }
    free(libs);
    return RXEXIT_NOT_HANDLED;
}

/* PRAGMA(option [, value]) */
static LONG pragma(RXFNCCAL_PARM *p)
{
    char buf[256], opt;
    struct Process *me = (struct Process *)FindTask(NULL);
    PRXSTRING v = p->rxfnc_argc > 1 ? &p->rxfnc_argv[1] : NULL;
    char val[256];
    long vlen = 0;

    if (p->rxfnc_argc < 1 || p->rxfnc_argc > 2 || !p->rxfnc_argv[0].strptr ||
        !p->rxfnc_argv[0].strlength)
        lxa_arexx_raise(17);
    if (v && v->strptr) {
        vlen = v->strlength < sizeof(val) - 1 ? (long)v->strlength : (long)sizeof(val) - 1;
        memcpy(val, v->strptr, vlen);
    }
    val[vlen] = 0;
    opt = lxa_rexx_upc(p->rxfnc_argv[0].strptr[0]);
    buf[0] = 0;
    switch (opt) {
    case 'D': {                         /* current directory */
        NameFromLock(me->pr_CurrentDir, (STRPTR)buf, sizeof(buf));
        if (v && v->strptr) {
            BPTR lock = Lock((STRPTR)val, ACCESS_READ);
            if (lock)
                UnLock(CurrentDir(lock));
            else
                buf[0] = 0;
        }
        break;
    }
    case 'P': {                         /* task priority */
        LONG old = me->pr_Task.tc_Node.ln_Pri;
        if (v && v->strptr)
            SetTaskPri(&me->pr_Task, atol(val));
        sprintf(buf, "%ld", (long)old);
        break;
    }
    case 'I':                           /* task id */
        sprintf(buf, "%08lx", (unsigned long)me);
        break;
    case 'S':                           /* stack size */
        sprintf(buf, "%lu", (unsigned long)__stack);
        break;
    case 'W':                           /* requester window */
        if (v && v->strptr)
            me->pr_WindowPtr = strcasecmp(val, "NULL") == 0 ? (APTR)-1 : NULL;
        break;
    default:
        lxa_arexx_raise(18);
    }
    set_retc(&p->rxfnc_retc, buf, strlen(buf));
    return RXEXIT_HANDLED;
}

static LONG APIENTRY fnc_exit(LONG fn, LONG sub, PEXIT parm)
{
    RXFNCCAL_PARM *p = (RXFNCCAL_PARM *)parm;
    PRXSTRING a = p->rxfnc_argv;
    int argc = p->rxfnc_argc;

    (void)fn;
    (void)sub;
    if (fn_is(p, "ADDLIB")) {
        if (argc != 2 && argc != 4)
            lxa_arexx_raise(17);
        if (!a[0].strptr)
            lxa_arexx_raise(18);
        return ret_bool(p, lxa_rexx_addlib(a[0].strptr, a[0].strlength, arg_long(&a[1]),
                                           argc == 4, argc == 4 ? arg_long(&a[2]) : -1,
                                           argc == 4 ? arg_long(&a[3]) : 0));
    }
    if (fn_is(p, "REMLIB")) {
        if (argc != 1 || !a[0].strptr)
            lxa_arexx_raise(17);
        return ret_bool(p, lxa_rexx_remlib(a[0].strptr, a[0].strlength));
    }
    if (fn_is(p, "SETCLIP")) {
        if (argc < 1 || argc > 2 || !a[0].strptr)
            lxa_arexx_raise(17);
        if (argc == 2 && a[1].strptr)
            return ret_bool(p, lxa_rexx_setclip(a[0].strptr, a[0].strlength,
                                                a[1].strptr, a[1].strlength));
        lxa_rexx_setclip(a[0].strptr, a[0].strlength, NULL, 0);
        return ret_bool(p, 1);
    }
    if (fn_is(p, "GETCLIP")) {
        struct RexxRsrc *rr;
        if (argc != 1 || !a[0].strptr)
            lxa_arexx_raise(17);
        LockRexxBase(0);
        rr = lxa_rexx_find(&RexxSysBase->rl_ClipList, a[0].strptr, a[0].strlength);
        if (rr && rr->rr_Arg1)
            set_retc(&p->rxfnc_retc, (char *)rr->rr_Arg1, LengthArgstring((UBYTE *)rr->rr_Arg1));
        else
            set_retc(&p->rxfnc_retc, "", 0);
        UnlockRexxBase(0);
        return RXEXIT_HANDLED;
    }
    if (fn_is(p, "PRAGMA"))
        return pragma(p);
    return search_libraries(p);
}

/* ---- halt, trace, RXSIO ------------------------------------------------ */

/* once per clause (Regina's RXHLT/RXTRC exits are not available) */
int lxa_ax_poll(void)
{
    static int traced = 0;
    int ev = 0, now;

    if ((SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) ||
        (RexxSysBase->rl_Flags & (1 << RLFB_HALT)))
        ev |= LXA_AX_HALT;
    now = (RexxSysBase->rl_Flags & (1 << RLFB_TRACE)) ? 1 : 0;
    if (now != traced)
        ev |= now ? LXA_AX_TRACE_ON : LXA_AX_TRACE_OFF;
    traced = now;
    return ev;
}

static LONG APIENTRY sio_exit(LONG fn, LONG sub, PEXIT parm)
{
    (void)fn;
    if (sub == RXSIOTRC && RexxSysBase->rl_TraceFH) {
        RXSIOTRC_PARM *p = (RXSIOTRC_PARM *)parm;
        BPTR fh = (BPTR)RexxSysBase->rl_TraceFH;
        Write(fh, p->rxsio_string.strptr, p->rxsio_string.strlength);
        Write(fh, (APTR)"\n", 1);
        return RXEXIT_HANDLED;
    }
    if ((sub == RXSIOTRD || sub == RXSIODTR) && opt_prompt) {
        fputs(opt_prompt, stdout);
        fflush(stdout);
    }
    return RXEXIT_NOT_HANDLED;
}

/* ---- running the packet -------------------------------------------------- */

static BOOL read_program(const char *path)
{
    BPTR fh = Open((STRPTR)path, MODE_OLDFILE);
    BPTR lock;
    LONG size, n;

    if (!fh)
        return FALSE;
    Seek(fh, 0, OFFSET_END);
    size = Seek(fh, 0, OFFSET_BEGINNING);
    src = malloc(size > 0 ? size + 1 : 1);
    if (!src) {
        Close(fh);
        return FALSE;
    }
    n = Read(fh, src, size);
    Close(fh);
    src_len = n > 0 ? n : 0;
    src[src_len] = 0;
    lock = Lock((STRPTR)path, ACCESS_READ);
    if (!lock || !NameFromLock(lock, (STRPTR)prog_resolved, sizeof(prog_resolved)))
        strncpy(prog_resolved, path, sizeof(prog_resolved) - 1);
    if (lock)
        UnLock(lock);
    return TRUE;
}

static int is_whole_number(const char *s, long len, long *value)
{
    long i = 0, v = 0, sign = 1;

    while (i < len && s[i] == ' ')
        i++;
    while (len > i && s[len - 1] == ' ')
        len--;
    if (i < len && (s[i] == '-' || s[i] == '+'))
        sign = s[i++] == '-' ? -1 : 1;
    if (i >= len)
        return 0;
    for (; i < len; i++) {
        if (s[i] < '0' || s[i] > '9')
            return 0;
        v = v * 10 + (s[i] - '0');
    }
    *value = sign * v;
    return 1;
}

static void run_job(void)
{
    static RXSYSEXIT exits[] = {
        { (PSZ)"LXACMD", RXCMD },
        { (PSZ)"LXAFNC", RXFNC },
        { (PSZ)"LXASIO", RXSIO },
        { NULL, RXENDLST }
    };
    LONG code = job->rm_Action & RXCODEMASK;
    LONG flags = job->rm_Action;
    const char *a0 = (const char *)job->rm_Args[0];
    RXSTRING args[MAXRMARG], instore[2], result;
    char path[512];
    LONG argc = 0, ret, i;
    short rc = 0;
    long value;

    job->rm_Result1 = 0;
    job->rm_Result2 = 0;
    if (!a0) {
        job->rm_Result1 = RC_FATAL;
        job->rm_Result2 = ERR10_010;
        return;
    }

    if (flags & RXFF_STRING) {
        src_len = LengthArgstring((UBYTE *)a0);
        src = malloc(src_len + 1);
        if (!src) {
            job->rm_Result1 = RC_FATAL;
            job->rm_Result2 = ERR10_003;
            return;
        }
        memcpy(src, a0, src_len);
        src[src_len] = 0;
    } else {
        /* the program name, then (RXCOMM) one argument string */
        const char *s = a0, *name;
        long nlen;

        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == '"') {
            name = ++s;
            while (*s && *s != '"')
                s++;
            nlen = s - name;
            if (*s)
                s++;
        } else {
            name = s;
            while (*s && *s != ' ' && *s != '\t')
                s++;
            nlen = s - name;
        }
        while (*s == ' ' || *s == '\t')
            s++;
        if (nlen >= (long)sizeof(prog_called))
            nlen = sizeof(prog_called) - 1;
        memcpy(prog_called, name, nlen);
        prog_called[nlen] = 0;
        if (!lxa_ax_locate(name, nlen, path, sizeof(path)) || !read_program(path)) {
            job->rm_Result1 = RC_WARN;
            job->rm_Result2 = ERR10_001;   /* program not found */
            return;
        }
        if (code == RXCOMM && *s) {
            args[0].strptr = (char *)s;
            args[0].strlength = strlen(s);
            argc = 1;
        }
    }
    if (code == RXFUNC) {
        argc = job->rm_Action & RXARGMASK;
        for (i = 0; i < argc; i++) {
            const char *a = (const char *)job->rm_Args[i + 1];
            args[i].strptr = (char *)a;
            args[i].strlength = a ? LengthArgstring((UBYTE *)a) : 0;
        }
    }

    RexxRegisterExitExe((PSZ)"LXACMD", (RexxExitHandler *)cmd_exit, NULL);
    RexxRegisterExitExe((PSZ)"LXAFNC", (RexxExitHandler *)fnc_exit, NULL);
    RexxRegisterExitExe((PSZ)"LXASIO", (RexxExitHandler *)sio_exit, NULL);

    instore[0].strptr = src;
    instore[0].strlength = src_len;
    instore[1].strptr = NULL;
    instore[1].strlength = 0;
    result.strptr = NULL;
    result.strlength = 0;
    ret = RexxStart(argc, args, (PSZ)prog_called, instore, (PSZ)job_host(),
                    code == RXFUNC ? RXFUNCTION : RXCOMMAND, exits, &rc, &result);
    fflush(stdout);

    if (lxa_arexx_errcode > 0) {
        job->rm_Result1 = RC_ERROR;
        job->rm_Result2 = lxa_arexx_errcode;
    } else if (lxa_arexx_errcode < 0) {
        /* ended by a condition ARexx does not treat as an error */
    } else if (ret < 0) {
        job->rm_Result1 = RC_ERROR;
        job->rm_Result2 = lxa_arexx_error_code(-ret, 0, 0);
    } else if (flags & RXFF_RESULT) {
        if (result.strptr)
            job->rm_Result2 = (LONG)CreateArgstring((STRPTR)result.strptr, result.strlength);
    } else if (result.strptr) {
        if (is_whole_number(result.strptr, result.strlength, &value)) {
            job->rm_Result1 = value;
        } else {
            job->rm_Result1 = RC_ERROR;
            job->rm_Result2 = 47;       /* arithmetic conversion error */
        }
    }
    if (result.strptr)
        RexxFreeMemory(result.strptr);
}

int main(int argc, char **argv)
{
    (void)version;
    (void)argc;
    (void)argv;
    /* RexxMast hands us the packet in tc_UserData */
    job = (struct RexxMsg *)FindTask(NULL)->tc_UserData;
    if (!job) {
        fputs("ARexx: the interpreter is started by RexxMast\n", stderr);
        return RETURN_FAIL;
    }
    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 36);
    host_reply = CreateMsgPort();
    if (!RexxSysBase || !host_reply || !lxa_lex_buffers()) {
        job->rm_Result1 = RC_FATAL;
        job->rm_Result2 = ERR10_003;
    } else {
        run_job();
    }
    fflush(stdout);

    Forbid();
    if (RexxSysBase) {
        if (--RexxSysBase->rl_NumTask <= 0) {
            RexxSysBase->rl_NumTask = 0;
            RexxSysBase->rl_Flags &= ~(1 << RLFB_HALT);   /* HI ends with the last program */
        }
    }
    ReplyMsg((struct Message *)job);
    Permit();

    if (host_reply)
        DeleteMsgPort(host_reply);
    if (RexxSysBase)
        CloseLibrary((struct Library *)RexxSysBase);
    return RETURN_OK;
}
