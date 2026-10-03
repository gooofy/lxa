/*
 * EXECUTE command - run an AmigaDOS command file (script)
 *
 * Usage: Execute <file> [arguments]
 *
 * Like AmigaOS 3.1 (verified on the reference, Phase 221): the command file
 * runs in the calling shell, which reads it through cli_CurrentInput and
 * then returns to its previous input.  Local variables, the current
 * directory and the fail limit are shared with the caller.
 *
 * Script directives (lines starting with '.'):
 *   .KEY/.K <template>   ReadArgs() template for the arguments
 *   .DEF <name> <value>  default value of an argument
 *   .BRA/.KET <c>        argument brackets (default < and >)
 *   .DOLLAR/.DOL <c>     default-value separator in <name$default> ($)
 *   .DOT <c>             directive character (.)
 * <name> is replaced by the argument's value; a /S switch gives the
 * keyword, /M arguments are joined with spaces, <$$> is the CLI number.
 * A file with directives is converted into T:Command-<cli>-T<nn>, which the
 * shell deletes when it is done.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define SHELL_MAGIC     0x4C584153UL    /* set by SYS:System/Shell */
#define MAX_KEYS        32
#define LINE_LEN        1024
#define TEXT_MAX        65536

struct Key {
    char name[64];
    char def[256];
    BOOL sw, num, multi;
};

static char bra = '<', ket = '>', dollar = '$', dot = '.';
static struct Key keys[MAX_KEYS];
static int nkeys = 0;
static LONG values[MAX_KEYS];
static BOOL have_values = FALSE;
static char key_template[512];

static void say(const char *fmt, LONG *args)
{
    VFPrintf(Output(), (STRPTR)fmt, args);
}

/* parse "name/a,count/n,flag/s" into keys[] */
static void parse_template(const char *t)
{
    nkeys = 0;
    while (*t && nkeys < MAX_KEYS) {
        struct Key *k = &keys[nkeys++];
        int n = 0;
        memset(k, 0, sizeof(*k));
        /* the first alias names the argument */
        while (*t && *t != ',' && *t != '/' && *t != '=') {
            if (n < (int)sizeof(k->name) - 1)
                k->name[n++] = *t;
            t++;
        }
        k->name[n] = '\0';
        while (*t && *t != ',') {
            if (*t == '/' && t[1]) {
                char c = t[1];
                if (c == 'S' || c == 's')
                    k->sw = TRUE;
                else if (c == 'N' || c == 'n')
                    k->num = TRUE;
                else if (c == 'M' || c == 'm')
                    k->multi = TRUE;
            }
            t++;
        }
        if (*t == ',')
            t++;
    }
}

static struct Key *find_key(const char *name, int len, int *index)
{
    int i;
    for (i = 0; i < nkeys; i++) {
        if ((int)strlen(keys[i].name) == len && !strnicmp(keys[i].name, name, len)) {
            *index = i;
            return &keys[i];
        }
    }
    return NULL;
}

/* append the value of <spec> (name or name$default) to out */
static int subst(const char *spec, int len, char *out, int room)
{
    const char *sep = memchr(spec, dollar, len);
    int nlen = sep ? sep - spec : len;
    const char *val = NULL;
    char numbuf[16];
    static char multibuf[LINE_LEN];
    struct Key *k;
    int idx, n;

    if (nlen == 2 && spec[0] == '$' && spec[1] == '$') {
        LONG a[1];
        a[0] = ((struct Process *)FindTask(NULL))->pr_TaskNum;
        RawDoFmt((STRPTR)"%ld", a, (void (*)())"\x16\xc0\x4e\x75", numbuf);
        val = numbuf;
    } else if ((k = find_key(spec, nlen, &idx))) {
        LONG v = have_values ? values[idx] : 0;
        if (v) {
            if (k->sw) {
                val = k->name;
            } else if (k->num) {
                /* AmigaOS 3.1 substitutes nothing for a given /N value */
                val = "";
            } else if (k->multi) {
                STRPTR *m;
                multibuf[0] = '\0';
                for (m = (STRPTR *)v; *m; m++) {
                    if (multibuf[0])
                        strncat(multibuf, " ", sizeof(multibuf) - strlen(multibuf) - 1);
                    strncat(multibuf, (char *)*m, sizeof(multibuf) - strlen(multibuf) - 1);
                }
                val = multibuf;
            } else {
                val = (const char *)v;
            }
        }
        if (!v || (val && !*val && !k->num)) {
            if (sep) {
                static char defbuf[256];
                int dl = len - nlen - 1;
                if (dl > (int)sizeof(defbuf) - 1)
                    dl = sizeof(defbuf) - 1;
                CopyMem((APTR)(sep + 1), defbuf, dl);
                defbuf[dl] = '\0';
                val = defbuf;
            } else {
                val = k->def;
            }
        }
    } else {
        /* not an argument: its default (or nothing) */
        static char defbuf2[256];
        int dl = sep ? len - nlen - 1 : 0;
        if (dl > (int)sizeof(defbuf2) - 1)
            dl = sizeof(defbuf2) - 1;
        if (dl > 0)
            CopyMem((APTR)(sep + 1), defbuf2, dl);
        defbuf2[dl] = '\0';
        val = defbuf2;
    }
    n = strlen(val);
    if (n > room)
        n = room;
    CopyMem((APTR)val, out, n);
    return n;
}

static BOOL is_directive(const char *line, const char *a, const char *b, const char **rest)
{
    const char *p = line + 1;
    int la = strlen(a), lb = b ? strlen(b) : 0;

    if (!strnicmp(p, a, la) && (!p[la] || p[la] == ' ' || p[la] == '\t')) {
        *rest = p + la;
        return TRUE;
    }
    if (b && !strnicmp(p, b, lb) && (!p[lb] || p[lb] == ' ' || p[lb] == '\t')) {
        *rest = p + lb;
        return TRUE;
    }
    return FALSE;
}

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct CommandLineInterface *cli = Cli();
    char *argstr = (char *)GetArgStr();
    static char file[256];
    char *rest;
    char *text = NULL, *outbuf = NULL;
    LONG textlen = 0, outlen = 0;
    BPTR fh;
    struct RDArgs *rda = NULL;
    BOOL directives = FALSE;
    LONG rc = 0;
    int k = 0;

    /* file name (possibly quoted), then the arguments for .KEY */
    rest = argstr ? argstr : "";
    while (*rest == ' ' || *rest == '\t')
        rest++;
    if (*rest == '"') {
        rest++;
        while (*rest && *rest != '"' && k < (int)sizeof(file) - 1)
            file[k++] = *rest++;
        if (*rest == '"')
            rest++;
    } else {
        while (*rest && *rest != ' ' && *rest != '\t' && *rest != '\n' && k < (int)sizeof(file) - 1)
            file[k++] = *rest++;
    }
    file[k] = '\0';
    while (*rest == ' ' || *rest == '\t')
        rest++;

    if (!file[0]) {
        /* AmigaOS 3.1: silently fails */
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return RETURN_FAIL;
    }

    fh = Open((STRPTR)file, MODE_OLDFILE);
    if (!fh) {
        LONG err = IoErr();
        LONG a[1];
        a[0] = (LONG)file;
        say("EXECUTE: Can't open %s\n", a);
        PrintFault(err, NULL);
        SetIoErr(err);
        return RETURN_ERROR;
    }

    /* read the whole file */
    text = AllocVec(TEXT_MAX + 1, MEMF_PUBLIC);
    if (!text) {
        Close(fh);
        PrintFault(ERROR_NO_FREE_STORE, (STRPTR)"EXECUTE");
        return RETURN_FAIL;
    }
    textlen = Read(fh, text, TEXT_MAX);
    if (textlen < 0)
        textlen = 0;
    text[textlen] = '\0';

    {
        const char *p = skip_ws(text);
        directives = (*p == '.');
        if (!directives) {
            /* directives may also follow other lines */
            const char *l = text;
            while (l && *l) {
                if (*skip_ws(l) == '.' && skip_ws(l)[1] && skip_ws(l)[1] != ' ') {
                    directives = TRUE;
                    break;
                }
                l = strchr(l, '\n');
                if (l)
                    l++;
            }
        }
    }

    if (!directives) {
        /* hand the file itself to the shell */
        Seek(fh, 0, OFFSET_BEGINNING);
        FreeVec(text);
        text = NULL;
    } else {
        Close(fh);
        fh = 0;
    }

    if (directives) {
        char *line = text, *next;
        BOOL key_seen = FALSE;
        outbuf = AllocVec(TEXT_MAX * 2 + 1, MEMF_PUBLIC);
        if (!outbuf) {
            FreeVec(text);
            PrintFault(ERROR_NO_FREE_STORE, (STRPTR)"EXECUTE");
            return RETURN_FAIL;
        }

        /* first pass: .KEY and .DEF */
        for (line = text; line && *line; line = next) {
            static char l[LINE_LEN];
            const char *r;
            int n;
            next = strchr(line, '\n');
            n = next ? next - line : (int)strlen(line);
            if (next)
                next++;
            if (n > LINE_LEN - 1)
                n = LINE_LEN - 1;
            CopyMem(line, l, n);
            l[n] = '\0';
            if (l[0] != dot)
                continue;
            if (!key_seen && (is_directive(l, "KEY", "K", &r))) {
                r = skip_ws(r);
                strncpy(key_template, r, sizeof(key_template) - 1);
                key_seen = TRUE;
            } else if (is_directive(l, "DOT", NULL, &r)) {
                r = skip_ws(r);
                if (*r)
                    dot = *r;
            }
        }
        parse_template(key_template);

        if (key_seen) {
            static char argbuf[LINE_LEN + 2];
            int n = strlen(rest);
            if (n > LINE_LEN - 1)
                n = LINE_LEN - 1;
            CopyMem(rest, argbuf, n);
            while (n > 0 && (argbuf[n - 1] == '\n' || argbuf[n - 1] == ' '))
                n--;
            argbuf[n++] = '\n';
            argbuf[n] = '\0';
            rda = AllocDosObject(DOS_RDARGS, NULL);
            if (rda) {
                rda->RDA_Source.CS_Buffer = (UBYTE *)argbuf;
                rda->RDA_Source.CS_Length = n;
                rda->RDA_Source.CS_CurChr = 0;
                rda->RDA_Flags |= RDAF_NOPROMPT;
                memset(values, 0, sizeof(values));
                if (!ReadArgs((STRPTR)key_template, values, rda)) {
                    LONG err = IoErr();
                    LONG a[1];
                    a[0] = (LONG)key_template;
                    say("EXECUTE: Parameters unsuitable for key \"%s\"\n", a);
                    PrintFault(err, NULL);
                    FreeDosObject(DOS_RDARGS, rda);
                    FreeVec(text);
                    FreeVec(outbuf);
                    SetIoErr(err);
                    return RETURN_ERROR;
                }
                have_values = TRUE;
            }
        }

        /* second pass: directives and substitution */
        for (line = text; line && *line; line = next) {
            static char l[LINE_LEN];
            const char *r;
            int n, i;
            next = strchr(line, '\n');
            n = next ? next - line : (int)strlen(line);
            if (next)
                next++;
            if (n > LINE_LEN - 1)
                n = LINE_LEN - 1;
            CopyMem(line, l, n);
            l[n] = '\0';

            if (l[0] == dot && l[1] && l[1] != ' ' && l[1] != '\t') {
                if (is_directive(l, "DEF", NULL, &r)) {
                    char nm[64];
                    int j = 0, idx;
                    struct Key *kk;
                    r = skip_ws(r);
                    while (*r && *r != ' ' && *r != '\t' && j < 63)
                        nm[j++] = *r++;
                    nm[j] = '\0';
                    r = skip_ws(r);
                    if ((kk = find_key(nm, j, &idx))) {
                        int dl = strlen(r);
                        if (dl >= 2 && r[0] == '"' && r[dl - 1] == '"') {
                            r++;
                            dl -= 2;
                        }
                        if (dl > (int)sizeof(kk->def) - 1)
                            dl = sizeof(kk->def) - 1;
                        CopyMem((APTR)r, kk->def, dl);
                        kk->def[dl] = '\0';
                    }
                } else if (is_directive(l, "BRA", NULL, &r)) {
                    r = skip_ws(r);
                    if (*r)
                        bra = *r;
                } else if (is_directive(l, "KET", NULL, &r)) {
                    r = skip_ws(r);
                    if (*r)
                        ket = *r;
                } else if (is_directive(l, "DOLLAR", "DOL", &r)) {
                    r = skip_ws(r);
                    if (*r)
                        dollar = *r;
                }
                /* .KEY, .DOT and comments ('. text') produce no line */
                continue;
            }

            for (i = 0; l[i] && outlen < TEXT_MAX * 2 - 1; i++) {
                if (l[i] == bra) {
                    char *end = strchr(l + i + 1, ket);
                    if (end) {
                        int got = subst(l + i + 1, end - (l + i + 1), outbuf + outlen,
                                        TEXT_MAX * 2 - 1 - outlen);
                        if (got >= 0) {
                            outlen += got;
                            i = end - l;
                            continue;
                        }
                    }
                }
                outbuf[outlen++] = l[i];
            }
            if (next && outlen < TEXT_MAX * 2)
                outbuf[outlen++] = '\n';
        }

        /* T:Command-<cli>-T<nn> */
        {
            static int counter = 0;
            char tname[64];
            LONG a[2];
            BPTR wfh;
            do {
                BPTR l;
                a[0] = me->pr_TaskNum;
                a[1] = counter++;
                RawDoFmt((STRPTR)"T:Command-%ld-T%02ld", a, (void (*)())"\x16\xc0\x4e\x75", tname);
                l = Lock((STRPTR)tname, SHARED_LOCK);
                if (!l)
                    break;
                UnLock(l);
            } while (counter < 100);
            wfh = Open((STRPTR)tname, MODE_NEWFILE);
            if (wfh) {
                Write(wfh, outbuf, outlen);
                Close(wfh);
                fh = Open((STRPTR)tname, MODE_OLDFILE);
            }
            if (!fh) {
                LONG err = IoErr();
                PrintFault(err, (STRPTR)"EXECUTE");
                rc = RETURN_ERROR;
            }
        }
        if (rda) {
            FreeArgs(rda);
            FreeDosObject(DOS_RDARGS, rda);
        }
        FreeVec(text);
        FreeVec(outbuf);
        if (!fh)
            return rc;
    }

    if (cli && me->pr_ShellPrivate == SHELL_MAGIC) {
        /* the shell picks the command file up after we return */
        cli->cli_CurrentInput = fh;
        return 0;
    }

    /* not started by a shell: run one that reads the command file */
    {
        struct TagItem tags[3];
        tags[0].ti_Tag = SYS_Input;
        tags[0].ti_Data = (ULONG)fh;
        tags[1].ti_Tag = SYS_Output;
        tags[1].ti_Data = (ULONG)Output();
        tags[2].ti_Tag = TAG_DONE;
        rc = SystemTagList((STRPTR)"", tags);
        Close(fh);
        return rc < 0 ? RETURN_FAIL : rc;
    }
}
