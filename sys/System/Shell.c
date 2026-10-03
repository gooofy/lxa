/*
 * Shell - the lxa AmigaDOS shell (SYS:System/Shell)
 *
 * Behaviour follows the AmigaOS 3.1 shell, verified against the reference
 * machine with the shell parity scripts (tests/shell_parity, Phase 221):
 *
 *  - commands run in the shell's own process (RunCommand()), so they share
 *    its CLI structure, current directory and local variables;
 *  - command search: resident list, then the current directory, the path
 *    (cli_CommandDir, see Path) and C:;
 *  - local variables ($name, ${name}), RC and Result2 after every command,
 *    `command` substitution, aliases ([] marks where the arguments go),
 *    redirection (>file, >>file, <file);
 *  - the 3.1 internal commands (Alias Ask CD Echo Else EndCLI EndIf
 *    EndShell EndSkip FailAt Fault Get GetEnv If Lab NewCLI NewShell Path
 *    Prompt Quit Resident Run Set SetEnv Skip Stack Unalias Unset UnSetEnv
 *    Why and the .key/.bra/.ket script directives);
 *  - command files: C:Execute hands a script to the shell through
 *    cli_CurrentInput; the shell reads it and then returns to the previous
 *    input.  A return code at or above the fail limit stops all command
 *    files ("<cmd> failed returncode <rc>").
 *
 * Started with arguments (System(), dos Execute()) the shell runs that one
 * command line - and any command files it starts - and exits with the
 * command's return code.  Without arguments it reads commands from its
 * input; an interactive shell shows a banner, runs S:Startup-Sequence and
 * prompts.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <dos/var.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>
#include <inline/dos.h>
#include <inline/exec.h>

#include <string.h>
#include <stdarg.h>

#include "lxa_version.h"

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define LINE_MAX_LEN    1024
#define MAX_INPUTS      16
#define MAX_NEST        32
#define NAME_LEN        256

/* pr_ShellPrivate marker: "this process is an lxa shell" (C:Execute) */
#define SHELL_MAGIC     0x4C584153UL    /* 'LXAS' */

/* resident list use counts (dos/dosextens.h) */
#ifndef CMD_SYSTEM
#define CMD_SYSTEM      -1
#endif
#ifndef CMD_INTERNAL
#define CMD_INTERNAL    -2
#endif
#ifndef CMD_DISABLED
#define CMD_DISABLED    -999
#endif

/* -- state ------------------------------------------------------------- */

struct Input {
    BPTR fh;
    BOOL close;             /* opened by the shell or handed over by Execute */
    BOOL startup;           /* S:Startup-Sequence: failures do not stop it */
    char tmpname[NAME_LEN]; /* T:Command-... file to delete when done */
};

static struct Process *me;
static struct CommandLineInterface *cli;
static struct Input inputs[MAX_INPUTS];
static int ninputs = 0;
static BOOL running = TRUE;
static BOOL interactive = FALSE;
static LONG last_rc = 0;            /* RC of the previous command */
static LONG last_result2 = 0;
static BPTR shell_out = 0;          /* the shell's own output (not redirected) */

/* If/Else/EndIf */
enum { BLK_EXEC, BLK_SKIP_TO_ELSE, BLK_SKIP_TO_ENDIF };
static UBYTE blocks[MAX_NEST];
static int nblocks = 0;

/* -- output helpers ---------------------------------------------------- */

static void out_str(const char *s)
{
    FPuts(Output(), (STRPTR)s);
}

static void out_fmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    VFPrintf(Output(), (STRPTR)fmt, (LONG *)ap);
    va_end(ap);
}

static void shell_fmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    VFPrintf(shell_out, (STRPTR)fmt, (LONG *)ap);
    va_end(ap);
    Flush(shell_out);
}

/* "<header>: <fault text>" or just the fault text */
static void fault(LONG code, const char *header)
{
    PrintFault(code, (STRPTR)header);
}

static void out_padded(const char *s, int width)
{
    int n = strlen(s);
    out_str(s);
    while (n++ < width)
        out_str(" ");
}

/* -- small string helpers --------------------------------------------- */

static char *skip_ws(char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

static void trim_right(char *p)
{
    int n = strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\n' || p[n - 1] == '\r'))
        p[--n] = '\0';
}

/* Copy one (possibly quoted) word from *pp into buf; returns FALSE at the
 * end of the line.  Quotes are removed, *" and ** inside quotes kept as the
 * plain character. */
static BOOL next_word(char **pp, char *buf, int len)
{
    char *p = skip_ws(*pp);
    int n = 0;

    if (!*p)
        return FALSE;
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (*p == '*' && p[1]) {
                p++;
                if (*p == 'N' || *p == 'n')
                    *p = '\n';
            }
            if (n < len - 1)
                buf[n++] = *p;
            p++;
        }
        if (*p == '"')
            p++;
    } else {
        while (*p && *p != ' ' && *p != '\t') {
            if (n < len - 1)
                buf[n++] = *p;
            p++;
        }
    }
    buf[n] = '\0';
    *pp = p;
    return TRUE;
}

/* -- CLI fields -------------------------------------------------------- */

static void bstr_get(BSTR b, char *buf, int len)
{
    UBYTE *s = BADDR(b);
    int n = 0;
    if (s) {
        n = s[0];
        if (n > len - 1)
            n = len - 1;
        CopyMem(s + 1, buf, n);
    }
    buf[n] = '\0';
}

static void bstr_set(BSTR b, const char *str)
{
    UBYTE *s = BADDR(b);
    int n = strlen(str);
    if (!s)
        return;
    if (n > 255)
        n = 255;
    s[0] = n;
    CopyMem((APTR)str, s + 1, n);
    s[n + 1] = '\0';
}

static void set_local_num(const char *name, LONG value)
{
    char buf[16];
    LONG args[1];
    args[0] = value;
    RawDoFmt((STRPTR)"%ld", args, (void (*)())"\x16\xc0\x4e\x75", buf);
    SetVar((STRPTR)name, (STRPTR)buf, -1, GVF_LOCAL_ONLY | LV_VAR);
}

/* -- input stack ------------------------------------------------------- */

static void push_input(BPTR fh, BOOL close, BOOL startup)
{
    struct Input *in;

    if (ninputs >= MAX_INPUTS) {
        if (close)
            Close(fh);
        return;
    }
    in = &inputs[ninputs++];
    in->fh = fh;
    in->close = close;
    in->startup = startup;
    in->tmpname[0] = '\0';
    if (close) {
        /* command files C:Execute prepared in T: are removed when done */
        char name[NAME_LEN];
        if (NameFromFH(fh, (STRPTR)name, sizeof(name)) &&
            !strncmp((char *)FilePart((STRPTR)name), "Command-", 8))
            strcpy(in->tmpname, name);
    }
    cli->cli_CurrentInput = fh;
}

static void pop_input(void)
{
    struct Input *in;

    if (!ninputs)
        return;
    in = &inputs[--ninputs];
    if (in->close)
        Close(in->fh);
    if (in->tmpname[0])
        DeleteFile((STRPTR)in->tmpname);
    cli->cli_CurrentInput = ninputs ? inputs[ninputs - 1].fh : cli->cli_StandardInput;
}

/* a failure ends every command file (back to the interactive input) */
static void abort_scripts(void)
{
    while (ninputs && (inputs[ninputs - 1].close || !interactive)) {
        if (ninputs == 1 && interactive)
            break;
        pop_input();
    }
    nblocks = 0;
}

static BOOL in_script(void)
{
    return ninputs > 0 && (inputs[ninputs - 1].close || !interactive);
}

/* read one line from the current input; FALSE at end of file */
static BOOL read_line(char *buf, int len)
{
    BPTR fh = inputs[ninputs - 1].fh;
    int n = 0;
    LONG c;

    for (;;) {
        c = FGetC(fh);
        if (c < 0) {
            if (n == 0)
                return FALSE;
            break;
        }
        if (c == '\n')
            break;
        if (n < len - 1)
            buf[n++] = c;
    }
    buf[n] = '\0';
    return TRUE;
}

/* -- variables --------------------------------------------------------- */

static BOOL is_var_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '.';
}

/* $name / ${name} -> value (unknown names stay as written), *$ -> $ */
static void substitute_vars(const char *src, char *dst, int len)
{
    int n = 0;
    char name[NAME_LEN];
    static char value[LINE_MAX_LEN];

    while (*src && n < len - 1) {
        if (src[0] == '*' && src[1] == '$') {
            dst[n++] = '$';
            src += 2;
            continue;
        }
        if (src[0] == '$' && (src[1] == '{' || is_var_char(src[1]))) {
            const char *p = src + 1;
            int k = 0;
            BOOL brace = (*p == '{');
            if (brace) {
                p++;
                while (*p && *p != '}' && k < NAME_LEN - 1)
                    name[k++] = *p++;
                if (*p != '}') {
                    dst[n++] = *src++;
                    continue;
                }
                p++;
            } else {
                while (is_var_char(*p) && k < NAME_LEN - 1)
                    name[k++] = *p++;
            }
            name[k] = '\0';
            if (GetVar((STRPTR)name, (STRPTR)value, sizeof(value), LV_VAR) >= 0) {
                char *v = value;
                while (*v && n < len - 1)
                    dst[n++] = *v++;
                src = p;
                continue;
            }
            /* unknown: keep "$name" */
            while (src < p && n < len - 1)
                dst[n++] = *src++;
            continue;
        }
        dst[n++] = *src++;
    }
    dst[n] = '\0';
}

/* -- command search ----------------------------------------------------- */

struct PathEntry {
    BPTR next;
    BPTR lock;
};

static BPTR load_from(BPTR dir, const char *name, BPTR *home)
{
    BPTR old = CurrentDir(dir);
    BPTR lock = Lock((STRPTR)name, SHARED_LOCK);
    BPTR seg = 0;

    if (lock) {
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        if (fib && Examine(lock, fib) && fib->fib_DirEntryType < 0)
            seg = LoadSeg((STRPTR)name);
        if (fib)
            FreeDosObject(DOS_FIB, fib);
        if (seg && home)
            *home = ParentDir(lock);
        UnLock(lock);
    }
    CurrentDir(old);
    return seg;
}

/* is 'name' (in the current directory) a script (protection bit s)? */
static BOOL is_script(BPTR dir, const char *name, char *full, int len)
{
    BPTR old = CurrentDir(dir);
    BPTR lock = Lock((STRPTR)name, SHARED_LOCK);
    BOOL script = FALSE;

    if (lock) {
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        if (fib && Examine(lock, fib) && fib->fib_DirEntryType < 0 &&
            (fib->fib_Protection & FIBF_SCRIPT))
            script = NameFromLock(lock, (STRPTR)full, len);
        if (fib)
            FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
    }
    CurrentDir(old);
    return script;
}

/* Find a command: resident list, current dir, path, C:.  Returns the
 * segment (resident: *res set), or 0; *script set for script files. */
static BPTR find_command(const char *name, struct Segment **res, BPTR *home,
                         char *script, int slen)
{
    BPTR seg;
    BPTR cdir;
    struct PathEntry *pe;

    *res = NULL;
    *home = 0;
    script[0] = '\0';

    if (!strchr(name, ':') && !strchr(name, '/')) {
        struct Segment *s;
        Forbid();
        s = FindSegment((STRPTR)name, NULL, FALSE);
        if (!s)
            s = FindSegment((STRPTR)name, NULL, TRUE);
        if (s && s->seg_UC >= 0) {
            s->seg_UC++;
            *res = s;
            Permit();
            return s->seg_Seg;
        }
        if (s && s->seg_UC == CMD_SYSTEM) {
            *res = s;
            Permit();
            return s->seg_Seg;
        }
        Permit();
    }

    cdir = me->pr_CurrentDir;
    if ((seg = load_from(cdir, name, home)))
        return seg;
    if (is_script(cdir, name, script, slen))
        return 0;
    if (strchr(name, ':'))
        return 0;

    for (pe = BADDR(cli->cli_CommandDir); pe; pe = BADDR(pe->next)) {
        if ((seg = load_from(pe->lock, name, home)))
            return seg;
        if (is_script(pe->lock, name, script, slen))
            return 0;
    }

    {
        BPTR c = Lock((STRPTR)"C:", SHARED_LOCK);
        if (c) {
            seg = load_from(c, name, home);
            if (!seg)
                is_script(c, name, script, slen);
            UnLock(c);
            if (seg)
                return seg;
        }
    }
    return 0;
}

/* -- built-in commands ------------------------------------------------- */

typedef LONG (*builtin_fn)(char *args, const char *name);

/* ReadArgs() over a string; the returned RDArgs must be freed */
static struct RDArgs *parse_args(const char *tmpl, char *args, LONG *array, int count)
{
    struct RDArgs *rda = AllocDosObject(DOS_RDARGS, NULL);
    static char buf[LINE_MAX_LEN + 2];
    int n;

    if (!rda)
        return NULL;
    memset(array, 0, count * sizeof(LONG));
    n = strlen(args);
    if (n > LINE_MAX_LEN)
        n = LINE_MAX_LEN;
    CopyMem(args, buf, n);
    buf[n++] = '\n';
    buf[n] = '\0';
    rda->RDA_Source.CS_Buffer = (UBYTE *)buf;
    rda->RDA_Source.CS_Length = n;
    rda->RDA_Source.CS_CurChr = 0;
    rda->RDA_Flags |= RDAF_NOPROMPT;
    if (!ReadArgs((STRPTR)tmpl, array, rda)) {
        FreeDosObject(DOS_RDARGS, rda);
        return NULL;
    }
    return rda;
}

static void free_args(struct RDArgs *rda)
{
    FreeArgs(rda);
    FreeDosObject(DOS_RDARGS, rda);
}

/* bad arguments of an internal command: "<fault text>", RC 10 */
static LONG bad_args(void)
{
    LONG err = IoErr();
    fault(err, NULL);
    SetIoErr(err);
    return RETURN_ERROR;
}

static LONG cmd_echo(char *args, const char *name)
{
    LONG a[4];
    struct RDArgs *rda = parse_args("/M,NOLINE/S,FIRST/K/N,LEN/K/N", args, a, 4);
    static char buf[LINE_MAX_LEN];
    STRPTR *m;
    int n = 0, len, first;

    if (!rda)
        return bad_args();
    if (!a[0]) {
        /* Echo without arguments prints nothing (AmigaOS 3.1) */
        free_args(rda);
        return 0;
    }
    buf[0] = '\0';
    for (m = (STRPTR *)a[0]; m && *m; m++) {
        int l = strlen((char *)*m);
        if (n + l + 2 >= (int)sizeof(buf))
            break;
        if (n)
            buf[n++] = ' ';
        CopyMem(*m, buf + n, l);
        n += l;
        buf[n] = '\0';
    }
    len = n;
    first = 0;
    if (a[2]) {
        first = *(LONG *)a[2] - 1;
        if (first < 0)
            first = 0;
        if (first > n)
            first = n;
        len = n - first;
        if (a[3] && *(LONG *)a[3] < len)
            len = *(LONG *)a[3];
    } else if (a[3]) {
        LONG l = *(LONG *)a[3];
        if (l < n) {
            first = n - l;
            len = l;
        }
    }
    if (len < 0)
        len = 0;
    Write(Output(), buf + first, len);
    if (!a[1])
        out_str("\n");
    free_args(rda);
    return 0;
}

static LONG cmd_failat(char *args, const char *name)
{
    LONG a[1];
    struct RDArgs *rda = parse_args("RCLIM/N", args, a, 1);

    if (!rda)
        return bad_args();
    if (a[0])
        cli->cli_FailLevel = *(LONG *)a[0];
    else
        out_fmt("Fail limit: %ld\n", cli->cli_FailLevel);
    free_args(rda);
    return 0;
}

static LONG cmd_fault(char *args, const char *name)
{
    LONG a[1];
    struct RDArgs *rda = parse_args("/N/M", args, a, 1);
    LONG **n;
    char buf[100];
    LONG rc = 0;

    if (!rda)
        return bad_args();
    for (n = (LONG **)a[0]; n && *n; n++) {
        /* AmigaOS 3.1: "Fault %3ld: text"; code 0 has no text (header
         * only, no newline); a code without a message gives RC 5 */
        if (**n == 0) {
            out_fmt("Fault %3ld", **n);
            rc = RETURN_WARN;
            continue;
        }
        Fault(**n, NULL, (STRPTR)buf, sizeof(buf));
        out_fmt("Fault %3ld: %s\n", **n, buf);
        if (!strncmp(buf, "Error ", 6))
            rc = RETURN_WARN;
    }
    free_args(rda);
    return rc;
}

static LONG cmd_why(char *args, const char *name)
{
    char buf[100];

    if (cli->cli_ReturnCode == 0 || cli->cli_Result2 == 0) {
        out_str("The last command did not set a return code\n");
        return 0;
    }
    Fault(cli->cli_Result2, NULL, (STRPTR)buf, sizeof(buf));
    out_fmt("Last command failed because %s\n", buf);
    return 0;
}

static LONG cmd_cd(char *args, const char *name)
{
    LONG a[1];
    struct RDArgs *rda = parse_args("DIR", args, a, 1);
    char buf[NAME_LEN];
    LONG rc = 0;

    if (!rda)
        return bad_args();
    if (!a[0]) {
        if (NameFromLock(me->pr_CurrentDir, (STRPTR)buf, sizeof(buf)))
            out_fmt("%s\n", buf);
    } else {
        BPTR lock = Lock((STRPTR)a[0], SHARED_LOCK);
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        if (!lock) {
            LONG err = IoErr();
            fault(err, NULL);
            SetIoErr(err);
            rc = RETURN_FAIL;
        } else if (fib && Examine(lock, fib) && fib->fib_DirEntryType < 0) {
            fault(ERROR_OBJECT_WRONG_TYPE, NULL);
            UnLock(lock);
            SetIoErr(ERROR_OBJECT_WRONG_TYPE);
            rc = RETURN_FAIL;
        } else {
            UnLock(CurrentDir(lock));
            if (NameFromLock(lock, (STRPTR)buf, sizeof(buf)))
                SetCurrentDirName((STRPTR)buf);
        }
        if (fib)
            FreeDosObject(DOS_FIB, fib);
    }
    free_args(rda);
    return rc;
}

/* local variables (LV_VAR) or aliases (LV_ALIAS), sorted by name */
static void list_locals(UBYTE type)
{
    struct LocalVar *v, *best;
    char last[NAME_LEN];
    BOOL have_last = FALSE;

    for (;;) {
        best = NULL;
        for (v = (struct LocalVar *)me->pr_LocalVars.mlh_Head; v->lv_Node.ln_Succ;
             v = (struct LocalVar *)v->lv_Node.ln_Succ) {
            if ((v->lv_Node.ln_Type & 0x7F) != type || !v->lv_Node.ln_Name)
                continue;
            if (have_last && stricmp((const char *)v->lv_Node.ln_Name, last) <= 0)
                continue;
            if (!best || stricmp((const char *)v->lv_Node.ln_Name, best->lv_Node.ln_Name) < 0)
                best = v;
        }
        if (!best)
            break;
        out_padded((char *)best->lv_Node.ln_Name, 18);
        {
            /* lxa keeps the NUL terminator in lv_Len */
            LONG l = best->lv_Len;
            while (l > 0 && best->lv_Value[l - 1] == '\0')
                l--;
            Write(Output(), best->lv_Value, l);
        }
        out_str("\n");
        strncpy(last, best->lv_Node.ln_Name, sizeof(last) - 1);
        last[sizeof(last) - 1] = '\0';
        have_last = TRUE;
    }
}

/* Set/SetEnv/Alias: NAME,STRING/F */
static LONG set_common(char *args, ULONG flags)
{
    LONG a[2];
    struct RDArgs *rda = parse_args("NAME,STRING/F", args, a, 2);
    static char buf[LINE_MAX_LEN];
    LONG rc = 0;

    if (!rda)
        return bad_args();
    if (!a[0]) {
        if (flags & GVF_GLOBAL_ONLY) {
            /* SetEnv without arguments lists ENV: */
            BPTR lock = Lock((STRPTR)"ENV:", SHARED_LOCK);
            struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
            if (lock && fib && Examine(lock, fib)) {
                while (ExNext(lock, fib)) {
                    if (fib->fib_DirEntryType >= 0)
                        continue;
                    out_padded(fib->fib_FileName, 18);
                    if (GetVar((STRPTR)fib->fib_FileName, (STRPTR)buf, sizeof(buf),
                               GVF_GLOBAL_ONLY | GVF_BINARY_VAR) >= 0)
                        out_str(buf);
                    out_str("\n");
                }
            }
            if (fib)
                FreeDosObject(DOS_FIB, fib);
            if (lock)
                UnLock(lock);
        } else {
            list_locals(flags & 0xFF);
        }
    } else if (!a[1]) {
        if (flags & GVF_GLOBAL_ONLY) {
            SetVar((STRPTR)a[0], (STRPTR)"", 0, flags);
        } else if (GetVar((STRPTR)a[0], (STRPTR)buf, sizeof(buf), flags) >= 0) {
            out_fmt("%s\n", buf);
        }
    } else {
        char *v = (char *)a[1];
        int l = strlen(v);
        /* a quoted value is stored without its quotes */
        if (l >= 2 && v[0] == '"' && v[l - 1] == '"') {
            v[l - 1] = '\0';
            v++;
        }
        if (!SetVar((STRPTR)a[0], (STRPTR)v, -1, flags)) {
            fault(IoErr(), NULL);
            rc = RETURN_ERROR;
        }
    }
    free_args(rda);
    return rc;
}

static LONG cmd_set(char *args, const char *name)
{
    return set_common(args, GVF_LOCAL_ONLY | LV_VAR);
}

static LONG cmd_setenv(char *args, const char *name)
{
    return set_common(args, GVF_GLOBAL_ONLY | LV_VAR);
}

static LONG cmd_alias(char *args, const char *name)
{
    return set_common(args, GVF_LOCAL_ONLY | LV_ALIAS);
}

static LONG get_common(char *args, ULONG flags)
{
    LONG a[1];
    struct RDArgs *rda = parse_args("NAME/A", args, a, 1);
    static char buf[LINE_MAX_LEN];
    LONG rc = 0;

    if (!rda)
        return bad_args();
    if (GetVar((STRPTR)a[0], (STRPTR)buf, sizeof(buf), flags) >= 0) {
        out_fmt("%s\n", buf);
    } else {
        LONG err = IoErr();
        fault(err, NULL);
        SetIoErr(err);
        rc = RETURN_WARN;
    }
    free_args(rda);
    return rc;
}

static LONG cmd_get(char *args, const char *name)
{
    return get_common(args, GVF_LOCAL_ONLY | LV_VAR);
}

static LONG cmd_getenv(char *args, const char *name)
{
    return get_common(args, GVF_GLOBAL_ONLY | LV_VAR);
}

static LONG unset_common(char *args, ULONG flags)
{
    LONG a[1];
    struct RDArgs *rda = parse_args("NAME", args, a, 1);

    if (!rda)
        return bad_args();
    if (!a[0]) {
        if (!(flags & GVF_GLOBAL_ONLY))
            list_locals(flags & 0xFF);
    } else {
        DeleteVar((STRPTR)a[0], flags);
    }
    free_args(rda);
    return 0;
}

static LONG cmd_unset(char *args, const char *name)
{
    return unset_common(args, GVF_LOCAL_ONLY | LV_VAR);
}

static LONG cmd_unsetenv(char *args, const char *name)
{
    return unset_common(args, GVF_GLOBAL_ONLY | LV_VAR);
}

static LONG cmd_unalias(char *args, const char *name)
{
    return unset_common(args, GVF_LOCAL_ONLY | LV_ALIAS);
}

static LONG cmd_stack(char *args, const char *name)
{
    LONG a[1];
    struct RDArgs *rda = parse_args("SIZE/N", args, a, 1);

    if (!rda)
        return bad_args();
    if (a[0]) {
        LONG size = *(LONG *)a[0];
        if (size < 1600) {
            out_str("Requested size too small\n");
            free_args(rda);
            return RETURN_ERROR;
        }
        cli->cli_DefaultStack = size / 4;
    } else {
        out_fmt("Current stack size is %ld bytes\n", cli->cli_DefaultStack * 4);
    }
    free_args(rda);
    return 0;
}

static LONG cmd_prompt(char *args, const char *name)
{
    LONG a[1];
    struct RDArgs *rda = parse_args("PROMPT", args, a, 1);

    if (!rda)
        return bad_args();
    SetPrompt(a[0] ? (STRPTR)a[0] : (STRPTR)"%N> ");
    free_args(rda);
    return 0;
}

static LONG cmd_quit(char *args, const char *name)
{
    LONG a[1];
    struct RDArgs *rda = parse_args("RC/N", args, a, 1);
    LONG rc = 0;

    if (!rda)
        return bad_args();
    if (a[0])
        rc = *(LONG *)a[0];
    free_args(rda);
    /* ends the command file(s); a non-interactive shell ends */
    while (ninputs && in_script())
        pop_input();
    nblocks = 0;
    if (!ninputs)
        running = FALSE;
    return rc;
}

static LONG cmd_endcli(char *args, const char *name)
{
    running = FALSE;
    return 0;
}

static LONG cmd_nop(char *args, const char *name)
{
    return 0;
}

static LONG cmd_ask(char *args, const char *name)
{
    LONG a[1];
    struct RDArgs *rda = parse_args("PROMPT/A", args, a, 1);
    char buf[64];
    LONG n;

    if (!rda)
        return bad_args();
    for (;;) {
        out_fmt("%s ", a[0]);
        Flush(Output());
        n = Read(Input(), buf, sizeof(buf) - 1);
        if (n <= 0) {
            free_args(rda);
            return 0;
        }
        buf[n] = '\0';
        trim_right(buf);
        if (buf[0] == 'y' || buf[0] == 'Y') {
            free_args(rda);
            return RETURN_WARN;
        }
        if (buf[0] == 'n' || buf[0] == 'N' || buf[0] == '\0') {
            free_args(rda);
            return 0;
        }
    }
}

static LONG cmd_path(char *args, const char *name)
{
    LONG a[6];
    struct RDArgs *rda = parse_args("PATH/M,ADD/S,SHOW/S,RESET/S,REMOVE/S,QUIET/S", args, a, 6);
    struct PathEntry *pe, *prev;
    STRPTR *m;
    char buf[NAME_LEN];
    LONG rc = 0;

    if (!rda)
        return bad_args();

    if (a[3]) {
        /* RESET: forget the path */
        pe = BADDR(cli->cli_CommandDir);
        while (pe) {
            struct PathEntry *next = BADDR(pe->next);
            UnLock(pe->lock);
            FreeVec(pe);
            pe = next;
        }
        cli->cli_CommandDir = 0;
    }

    for (m = (STRPTR *)a[0]; m && *m; m++) {
        BPTR lock = Lock(*m, SHARED_LOCK);
        if (!lock) {
            LONG err = IoErr();
            fault(err, (char *)*m);
            SetIoErr(err);
            rc = RETURN_ERROR;
            continue;
        }
        if (a[4]) {
            /* REMOVE */
            BPTR *link = &cli->cli_CommandDir;
            while ((pe = BADDR(*link))) {
                if (SameLock(pe->lock, lock) == LOCK_SAME) {
                    *link = pe->next;
                    UnLock(pe->lock);
                    FreeVec(pe);
                    break;
                }
                link = &pe->next;
            }
            UnLock(lock);
            continue;
        }
        /* ADD (also the default): append, no duplicates */
        prev = NULL;
        for (pe = BADDR(cli->cli_CommandDir); pe; pe = BADDR(pe->next)) {
            if (SameLock(pe->lock, lock) == LOCK_SAME)
                break;
            prev = pe;
        }
        if (pe) {
            UnLock(lock);
            continue;
        }
        pe = AllocVec(sizeof(*pe), MEMF_PUBLIC | MEMF_CLEAR);
        if (!pe) {
            UnLock(lock);
            continue;
        }
        pe->lock = lock;
        if (prev)
            prev->next = MKBADDR(pe);
        else
            cli->cli_CommandDir = MKBADDR(pe);
    }

    if (!a[0] || a[2]) {
        if (!a[3] || a[2]) {
            out_str("Current_directory\n");
            for (pe = BADDR(cli->cli_CommandDir); pe; pe = BADDR(pe->next)) {
                if (NameFromLock(pe->lock, (STRPTR)buf, sizeof(buf)))
                    out_fmt("%s\n", buf);
            }
            out_str("C:\n");
        }
    }
    free_args(rda);
    return rc;
}

static LONG cmd_resident(char *args, const char *name);
static LONG cmd_if(char *args, const char *name);
static LONG cmd_else(char *args, const char *name);
static LONG cmd_endif(char *args, const char *name);
static LONG cmd_skip(char *args, const char *name);
static LONG run_external(const char *cmdname, char *args);

/* Run/NewShell/NewCLI are internal on AmigaOS; lxa runs them from C: */
static LONG cmd_delegate(char *args, const char *name)
{
    return run_external(name, args);
}

struct Builtin {
    const char *name;
    builtin_fn fn;
};

/* AmigaOS 3.1 internal commands, in the order Resident lists them */
static const struct Builtin builtins[] = {
    { "Alias",    cmd_alias },
    { "Ask",      cmd_ask },
    { "CD",       cmd_cd },
    { "Echo",     cmd_echo },
    { "Else",     cmd_else },
    { "EndCLI",   cmd_endcli },
    { "EndIf",    cmd_endif },
    { "EndShell", cmd_endcli },
    { "EndSkip",  cmd_nop },
    { "Failat",   cmd_failat },
    { "Fault",    cmd_fault },
    { "Get",      cmd_get },
    { "Getenv",   cmd_getenv },
    { "If",       cmd_if },
    { "Lab",      cmd_nop },
    { "NewCLI",   cmd_delegate },
    { "NewShell", cmd_delegate },
    { "Path",     cmd_path },
    { "Prompt",   cmd_prompt },
    { "Quit",     cmd_quit },
    { "Resident", cmd_resident },
    { "Run",      cmd_delegate },
    { "Set",      cmd_set },
    { "Setenv",   cmd_setenv },
    { "Skip",     cmd_skip },
    { "Stack",    cmd_stack },
    { "Unalias",  cmd_unalias },
    { "Unset",    cmd_unset },
    { "Unsetenv", cmd_unsetenv },
    { "Why",      cmd_why },
    { ".ket",     cmd_nop },
    { ".bra",     cmd_nop },
    { ".key",     cmd_nop },
    { NULL, NULL }
};

static const struct Builtin *find_builtin(const char *name)
{
    const struct Builtin *b;

    for (b = builtins; b->name; b++)
        if (!stricmp((const char *)b->name, name))
            return b;
    /* the remaining script directives */
    if (name[0] == '.')
        return &builtins[30];
    return NULL;
}

/* The internal commands are on the resident list as CMD_INTERNAL, as on
 * AmigaOS (Resident lists them, Which reports them as INTERNAL). */
static void register_internals(void)
{
    int i, n;

    for (n = 0; builtins[n].name; n++)
        ;
    Forbid();
    if (!FindSegment((STRPTR)"Echo", NULL, TRUE)) {
        /* AddSegment() prepends: add in reverse order */
        for (i = n - 1; i >= 0; i--) {
            /* the segment of an internal command is never run */
            ULONG *dummy = AllocVec(8, MEMF_PUBLIC | MEMF_CLEAR);
            if (dummy)
                AddSegment((STRPTR)builtins[i].name, MKBADDR(dummy), CMD_INTERNAL);
        }
    }
    Permit();
}

static LONG cmd_resident(char *args, const char *name)
{
    LONG a[7];
    struct RDArgs *rda = parse_args("NAME,FILE,REMOVE/S,ADD/S,REPLACE/S,PURE=FORCE/S,SYSTEM/S",
                                    args, a, 7);
    struct DosInfo *di = BADDR(((struct RootNode *)DOSBase->dl_Root)->rn_Info);
    struct Segment *s;
    LONG rc = 0;

    if (!rda)
        return bad_args();

    if (!a[0]) {
        out_str("NAME              USE COUNT\n\n");
        Forbid();
        for (s = BADDR((BPTR)di->di_NetHand); s; s = BADDR(s->seg_Next)) {
            char nm[64];
            int l = s->seg_Name[0];
            if (l > 63)
                l = 63;
            CopyMem(&s->seg_Name[1], nm, l);
            nm[l] = '\0';
            out_padded(nm, 18);
            if (s->seg_UC == CMD_INTERNAL)
                out_str("INTERNAL\n");
            else if (s->seg_UC == CMD_SYSTEM)
                out_str("SYSTEM\n");
            else if (s->seg_UC == CMD_DISABLED)
                out_str("DISABLED\n");
            else
                out_fmt("%ld\n", s->seg_UC);
        }
        Permit();
        free_args(rda);
        return 0;
    }

    if (a[2]) {
        /* REMOVE */
        Forbid();
        s = FindSegment((STRPTR)a[0], NULL, FALSE);
        if (!s) {
            Permit();
            fault(ERROR_OBJECT_NOT_FOUND, NULL);
            SetIoErr(ERROR_OBJECT_NOT_FOUND);
            rc = RETURN_WARN;
        } else if (s->seg_UC > 1 || !RemSegment(s)) {
            Permit();
            out_fmt("%s is in use\n", a[0]);
            rc = RETURN_WARN;
        } else {
            Permit();
        }
        free_args(rda);
        return rc;
    }

    {
        const char *file = a[1] ? (char *)a[1] : (char *)a[0];
        const char *rname = a[1] ? (char *)a[0] : (char *)FilePart((STRPTR)a[0]);
        BPTR seg = LoadSeg((STRPTR)file);

        if (!seg && !a[1] && !strchr(file, ':') && !strchr(file, '/')) {
            char cpath[NAME_LEN];
            strcpy(cpath, "C:");
            strncat(cpath, file, sizeof(cpath) - 3);
            seg = LoadSeg((STRPTR)cpath);
        }
        if (!seg) {
            LONG err = IoErr();
            fault(err, NULL);
            SetIoErr(err);
            free_args(rda);
            return RETURN_WARN;
        }
        Forbid();
        s = FindSegment((STRPTR)rname, NULL, FALSE);
        if (s && s->seg_UC > 1) {
            Permit();
            UnLoadSeg(seg);
            out_fmt("%s is in use\n", rname);
            free_args(rda);
            return RETURN_WARN;
        }
        /* an existing entry is replaced (AmigaOS 3.1) */
        if (s) {
            UnLoadSeg(s->seg_Seg);
            s->seg_Seg = seg;
            Permit();
        } else {
            Permit();
            AddSegment((STRPTR)rname, seg, a[6] ? CMD_SYSTEM : 1);
        }
    }
    free_args(rda);
    return 0;
}

/* -- If / Else / EndIf / Skip ------------------------------------------ */

static BOOL executing(void)
{
    return nblocks == 0 || blocks[nblocks - 1] == BLK_EXEC;
}

static void push_block(UBYTE state)
{
    if (nblocks < MAX_NEST)
        blocks[nblocks++] = state;
}

static LONG cmd_if(char *args, const char *name)
{
    LONG a[10];
    struct RDArgs *rda;
    BOOL cond = FALSE;

    rda = parse_args("NOT/S,WARN/S,ERROR/S,FAIL/S,A,EQ/K,GT/K,GE/K,VAL/S,EXISTS/K", args, a, 10);
    if (!rda) {
        LONG rc = bad_args();
        push_block(BLK_SKIP_TO_ELSE);
        return rc;
    }
    if (a[1])
        cond = last_rc >= RETURN_WARN;
    else if (a[2])
        cond = last_rc >= RETURN_ERROR;
    else if (a[3])
        cond = last_rc >= RETURN_FAIL;
    else if (a[9]) {
        BPTR lock = Lock((STRPTR)a[9], SHARED_LOCK);
        if (lock) {
            cond = TRUE;
            UnLock(lock);
        }
    } else if (a[5] || a[6] || a[7]) {
        const char *l = a[4] ? (char *)a[4] : "";
        const char *r = (char *)(a[5] ? a[5] : a[6] ? a[6] : a[7]);
        LONG c;
        if (a[8]) {
            LONG lv = 0, rv = 0;
            StrToLong((STRPTR)l, &lv);
            StrToLong((STRPTR)r, &rv);
            c = lv < rv ? -1 : lv > rv ? 1 : 0;
        } else {
            c = stricmp((const char *)l, r);
        }
        if (a[5])
            cond = (c == 0);
        else if (a[6])
            cond = (c > 0);
        else
            cond = (c >= 0);
    }
    if (a[0])
        cond = !cond;
    free_args(rda);
    push_block(cond ? BLK_EXEC : BLK_SKIP_TO_ELSE);
    return 0;
}

static LONG cmd_else(char *args, const char *name)
{
    if (nblocks == 0) {
        /* Else without If: skip to the next EndIf */
        push_block(BLK_SKIP_TO_ENDIF);
        return 0;
    }
    if (blocks[nblocks - 1] == BLK_EXEC)
        blocks[nblocks - 1] = BLK_SKIP_TO_ENDIF;
    else if (blocks[nblocks - 1] == BLK_SKIP_TO_ELSE)
        blocks[nblocks - 1] = BLK_EXEC;
    return 0;
}

static LONG cmd_endif(char *args, const char *name)
{
    if (nblocks)
        nblocks--;
    return 0;
}

/* first word of a line, lower case compare helper */
static BOOL first_word_is(const char *line, const char *word, const char **rest)
{
    const char *p = line;
    int n = strlen(word);

    while (*p == ' ' || *p == '\t')
        p++;
    if (strnicmp((const char *)p, word, n))
        return FALSE;
    if (p[n] && p[n] != ' ' && p[n] != '\t')
        return FALSE;
    if (rest)
        *rest = p + n;
    return TRUE;
}

static LONG cmd_skip(char *args, const char *name)
{
    LONG a[2];
    struct RDArgs *rda = parse_args("LABEL,BACK/S", args, a, 2);
    static char line[LINE_MAX_LEN];
    char label[NAME_LEN];

    if (!rda)
        return bad_args();
    label[0] = '\0';
    if (a[0]) {
        strncpy(label, (char *)a[0], sizeof(label) - 1);
        label[sizeof(label) - 1] = '\0';
    }
    free_args(rda);

    if (!in_script()) {
        out_str("Skip must be in a command file\n");
        return RETURN_FAIL;
    }
    if (a[1])
        Seek(inputs[ninputs - 1].fh, 0, OFFSET_BEGINNING);

    nblocks = 0;
    while (read_line(line, sizeof(line))) {
        const char *rest;
        if (first_word_is(line, "Lab", &rest)) {
            char word[NAME_LEN];
            char *r = (char *)rest;
            /* Skip without a label stops at a Lab without one */
            BOOL has = next_word(&r, word, sizeof(word));
            if (!label[0] ? !has : (has && !stricmp((const char *)word, label)))
                return 0;
        }
    }
    fault(ERROR_OBJECT_NOT_FOUND, NULL);
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return RETURN_FAIL;
}

/* -- running commands -------------------------------------------------- */

static LONG run_line(char *line);

static LONG run_external(const char *cmdname, char *args)
{
    struct Segment *res;
    BPTR home = 0, seg;
    char script[NAME_LEN];
    static char argbuf[LINE_MAX_LEN + 2];
    LONG rc, n;
    BPTR old_home, old_seg;
    BPTR *segarray;

    seg = find_command(cmdname, &res, &home, script, sizeof(script));
    if (!seg && script[0]) {
        /* a script file (protection bit s) runs through Execute */
        static char line[LINE_MAX_LEN];
        strcpy(line, "Execute \"");
        strncat(line, script, sizeof(line) - 20);
        strcat(line, "\" ");
        strncat(line, args, sizeof(line) - strlen(line) - 1);
        return run_line(line);
    }
    if (!seg) {
        out_fmt("%s: Unknown command\n", cmdname);
        last_result2 = ERROR_OBJECT_NOT_FOUND;
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return RETURN_ERROR;
    }

    n = strlen(args);
    if (n > LINE_MAX_LEN)
        n = LINE_MAX_LEN;
    CopyMem(args, argbuf, n);
    argbuf[n++] = '\n';
    argbuf[n] = '\0';

    bstr_set(cli->cli_CommandName, cmdname);
    cli->cli_Module = seg;
    segarray = (BPTR *)BADDR(me->pr_SegList);
    old_seg = segarray ? segarray[3] : 0;
    if (segarray)
        segarray[3] = seg;
    old_home = me->pr_HomeDir;
    me->pr_HomeDir = home;
    SetIoErr(0);

    rc = RunCommand(seg, cli->cli_DefaultStack * 4, (STRPTR)argbuf, n);

    last_result2 = IoErr();
    me->pr_HomeDir = old_home;
    if (home)
        UnLock(home);
    cli->cli_Module = 0;
    if (segarray)
        segarray[3] = old_seg;
    if (res) {
        Forbid();
        if (res->seg_UC > 0)
            res->seg_UC--;
        Permit();
    } else {
        UnLoadSeg(seg);
    }
    SetIoErr(last_result2);
    if (rc == -1) {
        fault(last_result2, cmdname);
        return RETURN_FAIL;
    }
    return rc;
}

/* `command` -> its output (newlines become spaces) */
static void substitute_backticks(char *line)
{
    char *open, *close;
    char *tmp, *result, *cmd;
    char tname[64];
    LONG args[2];

    if (!strchr(line, '`'))
        return;
    tmp = AllocVec(3 * LINE_MAX_LEN, MEMF_PUBLIC);
    if (!tmp)
        return;
    result = tmp + LINE_MAX_LEN;
    cmd = result + LINE_MAX_LEN;

    while ((open = strchr(line, '`')) && (close = strchr(open + 1, '`'))) {
        BPTR fh, old;
        int n = close - open - 1, k = 0;

        CopyMem(open + 1, cmd, n);
        cmd[n] = '\0';
        args[0] = me->pr_TaskNum;
        args[1] = (LONG)FindTask(NULL);
        RawDoFmt((STRPTR)"T:Shell-bq-%ld-%lx", args, (void (*)())"\x16\xc0\x4e\x75", tname);
        result[0] = '\0';
        fh = Open((STRPTR)tname, MODE_NEWFILE);
        if (fh) {
            old = SelectOutput(fh);
            run_line(cmd);
            SelectOutput(old);
            Close(fh);
            fh = Open((STRPTR)tname, MODE_OLDFILE);
            if (fh) {
                k = Read(fh, result, LINE_MAX_LEN - 1);
                if (k < 0)
                    k = 0;
                result[k] = '\0';
                Close(fh);
            }
            DeleteFile((STRPTR)tname);
        }
        trim_right(result);
        for (n = 0; result[n]; n++)
            if (result[n] == '\n')
                result[n] = ' ';
        *open = '\0';
        strcpy(tmp, line);
        strncat(tmp, result, LINE_MAX_LEN - strlen(tmp) - 1);
        strncat(tmp, close + 1, LINE_MAX_LEN - strlen(tmp) - 1);
        strcpy(line, tmp);
    }
    FreeVec(tmp);
}

/* truncate at an unquoted ';' */
static void strip_comment(char *line)
{
    BOOL q = FALSE;
    char *p;

    for (p = line; *p; p++) {
        if (q && *p == '*' && p[1]) {
            p++;
            continue;
        }
        if (*p == '"')
            q = !q;
        else if (*p == ';' && !q) {
            *p = '\0';
            break;
        }
    }
}

/* Remove unquoted >file, >>file and <file from args. */
static void take_redirections(char *args, char *out, BOOL *append, char *in)
{
    char *p = args, *d = args;
    BOOL q = FALSE;

    out[0] = in[0] = '\0';
    *append = FALSE;
    while (*p) {
        if (!q && (*p == '>' || *p == '<') && (p == args || p[-1] == ' ' || p[-1] == '\t')) {
            char *target = (*p == '<') ? in : out;
            int k = 0;
            if (*p == '>' && p[1] == '>') {
                *append = TRUE;
                p++;
            }
            p++;
            p = skip_ws(p);
            if (*p == '"') {
                p++;
                while (*p && *p != '"' && k < NAME_LEN - 1)
                    target[k++] = *p++;
                if (*p == '"')
                    p++;
            } else {
                while (*p && *p != ' ' && *p != '\t' && k < NAME_LEN - 1)
                    target[k++] = *p++;
            }
            target[k] = '\0';
            /* drop the separator before the redirection */
            while (d > args && (d[-1] == ' ' || d[-1] == '\t'))
                d--;
            if (d > args && *p)
                *d++ = ' ';
            p = skip_ws(p);
            continue;
        }
        if (q && *p == '*' && p[1]) {
            *d++ = *p++;
            *d++ = *p++;
            continue;
        }
        if (*p == '"')
            q = !q;
        *d++ = *p++;
    }
    *d = '\0';
    trim_right(args);
}

/* alias expansion: [] marks the place of the arguments */
static BOOL expand_alias(const char *name, const char *args, char *out, int len)
{
    static char value[LINE_MAX_LEN];
    char *br;

    if (GetVar((STRPTR)name, (STRPTR)value, sizeof(value), GVF_LOCAL_ONLY | LV_ALIAS) < 0)
        return FALSE;
    br = strstr(value, "[]");
    if (br) {
        *br = '\0';
        strncpy(out, value, len - 1);
        out[len - 1] = '\0';
        strncat(out, args, len - strlen(out) - 1);
        strncat(out, br + 2, len - strlen(out) - 1);
    } else {
        strncpy(out, value, len - 1);
        out[len - 1] = '\0';
        if (*args) {
            strncat(out, " ", len - strlen(out) - 1);
            strncat(out, args, len - strlen(out) - 1);
        }
    }
    return TRUE;
}

/* After a command: RC/Result2 and the fail limit. */
static void command_done(const char *name, LONG rc)
{
    /* AmigaOS 3.1: Result2 is 0 after a successful command */
    if (rc == 0)
        last_result2 = 0;
    last_rc = rc;
    cli->cli_ReturnCode = rc;
    cli->cli_Result2 = last_result2;
    set_local_num("RC", rc);
    set_local_num("Result2", last_result2);

    if (rc >= cli->cli_FailLevel) {
        BOOL startup = ninputs && inputs[ninputs - 1].startup;
        if (in_script() && !startup) {
            shell_fmt("%s failed returncode %ld\n", name, rc);
            abort_scripts();
            if (!interactive && !ninputs)
                running = FALSE;
        } else if (!startup) {
            shell_fmt("%s failed returncode %ld\n", name, rc);
        }
    }
}

/* Run one command line; returns its return code. */
struct LineWork {
    char expanded[LINE_MAX_LEN];
    char aliased[LINE_MAX_LEN];
    char name[NAME_LEN];
    char outname[NAME_LEN];
    char inname[NAME_LEN];
};

static LONG run_line_work(char *line, struct LineWork *w);

static LONG run_line(char *line)
{
    /* the work buffers live on the heap: run_line() recurses (scripts,
     * `command`) and the shell runs on small stacks */
    struct LineWork *w = AllocVec(sizeof(*w), MEMF_PUBLIC);
    LONG rc;

    if (!w)
        return RETURN_FAIL;
    rc = run_line_work(line, w);
    FreeVec(w);
    return rc;
}

static LONG run_line_work(char *line, struct LineWork *w)
{
    char *expanded = w->expanded, *aliased = w->aliased;
    char *name = w->name, *outname = w->outname, *inname = w->inname;
    char *p, *args;
    BOOL append;
    BPTR outfh = 0, infh = 0, oldout = 0, oldin = 0;
    const struct Builtin *b;
    LONG rc;

    strip_comment(line);
    trim_right(line);
    p = skip_ws(line);
    if (!*p)
        return -1;

    /* while an If block is skipped only If/Else/EndIf count */
    if (!executing()) {
        if (first_word_is(p, "If", NULL))
            push_block(BLK_SKIP_TO_ENDIF);
        else if (first_word_is(p, "Else", NULL)) {
            if (blocks[nblocks - 1] == BLK_SKIP_TO_ELSE &&
                (nblocks == 1 || blocks[nblocks - 2] == BLK_EXEC))
                blocks[nblocks - 1] = BLK_EXEC;
        } else if (first_word_is(p, "EndIf", NULL))
            nblocks--;
        return -1;
    }

    substitute_vars(p, expanded, LINE_MAX_LEN);
    substitute_backticks(expanded);

    p = expanded;
    if (!next_word(&p, name, NAME_LEN))
        return -1;
    args = skip_ws(p);

    if (expand_alias(name, args, aliased, LINE_MAX_LEN)) {
        p = aliased;
        if (!next_word(&p, name, NAME_LEN))
            return -1;
        args = skip_ws(p);
    }

    take_redirections(args, outname, &append, inname);
    if (outname[0]) {
        if (append) {
            outfh = Open((STRPTR)outname, MODE_READWRITE);
            if (outfh)
                Seek(outfh, 0, OFFSET_END);
        } else {
            outfh = Open((STRPTR)outname, MODE_NEWFILE);
        }
        if (!outfh) {
            LONG err = IoErr();
            out_fmt("Unable to open redirection file\n");
            last_result2 = err;
            command_done(name, RETURN_FAIL);
            return RETURN_FAIL;
        }
        oldout = SelectOutput(outfh);
    }
    if (inname[0]) {
        infh = Open((STRPTR)inname, MODE_OLDFILE);
        if (!infh) {
            LONG err = IoErr();
            if (outfh) {
                SelectOutput(oldout);
                Close(outfh);
            }
            out_fmt("Unable to open redirection file\n");
            last_result2 = err;
            command_done(name, RETURN_FAIL);
            return RETURN_FAIL;
        }
        oldin = SelectInput(infh);
    }

    last_result2 = 0;
    b = find_builtin(name);
    if (b && b->fn != cmd_delegate) {
        SetIoErr(0);
        rc = b->fn(args, b->name);
        if (rc)
            last_result2 = IoErr();
    } else {
        /* AmigaOS 3.1: a command run from a command file reads its input
         * from the command file (Eval, ReadArgs "?" prompts consume the
         * following lines) */
        BPTR oldcis = 0;
        BOOL from_script = !infh && in_script() && ninputs;
        if (from_script)
            oldcis = SelectInput(inputs[ninputs - 1].fh);
        rc = run_external(b ? b->name : name, args);
        if (from_script)
            SelectInput(oldcis);
    }
    Flush(Output());

    if (infh) {
        SelectInput(oldin);
        Close(infh);
    }
    if (outfh) {
        SelectOutput(oldout);
        Close(outfh);
    }

    /* C:Execute handed a command file over */
    if (cli->cli_CurrentInput && ninputs && cli->cli_CurrentInput != inputs[ninputs - 1].fh)
        push_input(cli->cli_CurrentInput, TRUE, FALSE);
    else if (cli->cli_CurrentInput && !ninputs && cli->cli_CurrentInput != cli->cli_StandardInput)
        push_input(cli->cli_CurrentInput, TRUE, FALSE);

    command_done(name, rc);
    return rc;
}

/* -- prompt ------------------------------------------------------------ */

static void show_prompt(void)
{
    char prompt[128], path[NAME_LEN];
    int i;

    bstr_get(cli->cli_Prompt, prompt, sizeof(prompt));
    for (i = 0; prompt[i]; i++) {
        if (prompt[i] == '%' && prompt[i + 1]) {
            i++;
            if (prompt[i] == 'N' || prompt[i] == 'n') {
                out_fmt("%ld", me->pr_TaskNum);
            } else if (prompt[i] == 'S' || prompt[i] == 's') {
                if (NameFromLock(me->pr_CurrentDir, (STRPTR)path, sizeof(path)))
                    out_str(path);
            } else if (prompt[i] == 'R' || prompt[i] == 'r') {
                out_fmt("%ld", last_rc);
            } else {
                char c[2];
                c[0] = prompt[i];
                c[1] = '\0';
                out_str(c);
            }
        } else {
            char c[2];
            c[0] = prompt[i];
            c[1] = '\0';
            out_str(c);
        }
    }
    Flush(Output());
}

/* -- main -------------------------------------------------------------- */

int main(void)
{
    static char line[LINE_MAX_LEN];
    char *argstr;
    BOOL command_mode;

    me = (struct Process *)FindTask(NULL);
    cli = Cli();
    if (!cli)
        return RETURN_FAIL;

    me->pr_ShellPrivate = SHELL_MAGIC;
    shell_out = Output();
    register_internals();

    if (cli->cli_DefaultStack < 1024)
        cli->cli_DefaultStack = 4096 / 4;
    if (cli->cli_FailLevel <= 0)
        cli->cli_FailLevel = RETURN_ERROR;
    cli->cli_StandardInput = Input();
    cli->cli_StandardOutput = Output();
    cli->cli_CurrentInput = Input();
    cli->cli_CurrentOutput = Output();
    set_local_num("process", me->pr_TaskNum);
    set_local_num("RC", 0);
    set_local_num("Result2", 0);

    if (me->pr_CurrentDir) {
        char dir[NAME_LEN];
        if (NameFromLock(me->pr_CurrentDir, (STRPTR)dir, sizeof(dir)))
            SetCurrentDirName((STRPTR)dir);
    }

    argstr = (char *)GetArgStr();
    if (argstr) {
        strncpy(line, argstr, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
        trim_right(line);
    } else {
        line[0] = '\0';
    }
    command_mode = (*skip_ws(line) != '\0');

    if (command_mode) {
        /* System(): one command line (and the command files it starts) */
        cli->cli_Interactive = DOSFALSE;
        cli->cli_Background = DOSTRUE;
        cli->cli_CurrentInput = 0;
        cli->cli_StandardInput = 0;
        run_line(line);
    } else {
        interactive = IsInteractive(Input()) ? TRUE : FALSE;
        cli->cli_Interactive = interactive ? DOSTRUE : DOSFALSE;
        cli->cli_Background = interactive ? DOSFALSE : DOSTRUE;
        push_input(Input(), FALSE, FALSE);
        if (interactive) {
            BPTR startup;
            out_fmt("lxa Shell v%s (%s)\n", (LONG)LXA_VERSION_STRING, (LONG)LXA_BUILD_DATE);
            Flush(Output());
            {
                char prompt[64];
                bstr_get(cli->cli_Prompt, prompt, sizeof(prompt));
                if (!prompt[0])
                    SetPrompt((STRPTR)"%N.%S> ");
            }
            startup = Open((STRPTR)"S:Startup-Sequence", MODE_OLDFILE);
            if (startup)
                push_input(startup, TRUE, TRUE);
        }
    }

    while (running && ninputs) {
        if (interactive && ninputs == 1)
            show_prompt();
        if (!read_line(line, sizeof(line))) {
            if (ninputs == 1 && !inputs[0].close)
                break;      /* end of the shell's own input */
            pop_input();
            nblocks = 0;
            continue;
        }
        run_line(line);
        if (SetSignal(0, SIGBREAKF_CTRL_D) & SIGBREAKF_CTRL_D) {
            if (in_script()) {
                shell_fmt("***Break\n");
                abort_scripts();
            }
        }
    }

    while (ninputs)
        pop_input();
    me->pr_ShellPrivate = 0;
    return last_rc;
}
