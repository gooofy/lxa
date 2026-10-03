/*
 * LIST - detailed directory listing
 *
 * Template: DIR/M,P=PAT/K,KEYS/S,DATES/S,NODATES/S,TO/K,SUB/K,SINCE/K,
 *           UPTO/K,QUICK/S,BLOCK/S,NOHEAD/S,FILES/S,DIRS/S,LFORMAT/K,ALL/S
 *
 * Output format and messages follow AmigaOS 3.1's List (verified on the
 * reference with tests/shell_parity/list.script, Phase 221):
 *
 *   Directory "dir" on Monday 01-Jan-24
 *   name                    size protbits date      time
 *   : comment
 *   2 files - 1 directory - 5 blocks used
 *
 * The header appears for a named directory (also the directory part of a
 * pattern or file argument); dates use Today/Yesterday/weekday names unless
 * DATES is given.  Blocks: fib_NumBlocks + 1 per file, 1 per directory.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/datetime.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

/* fib_FileName/fib_Comment are UBYTE arrays */
#pragma GCC diagnostic ignored "-Wpointer-sign"

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "DIR/M,P=PAT/K,KEYS/S,DATES/S,NODATES/S,TO/K,SUB/K,SINCE/K,UPTO/K,QUICK/S,BLOCK/S,NOHEAD/S,FILES/S,DIRS/S,LFORMAT/K,ALL/S"

enum { A_DIR, A_PAT, A_KEYS, A_DATES, A_NODATES, A_TO, A_SUB, A_SINCE, A_UPTO, A_QUICK,
       A_BLOCK, A_NOHEAD, A_FILES, A_DIRS, A_LFORMAT, A_ALL, A_COUNT };

#define PATH_LEN 512

static LONG args[A_COUNT];
static BPTR out;
static char pat_parsed[PATH_LEN * 2 + 2];
static BOOL have_pat = FALSE;
static struct DateStamp since, upto;
static BOOL have_since = FALSE, have_upto = FALSE;
static LONG tot_files, tot_dirs, tot_blocks;
static BOOL broken = FALSE;

static void say(const char *fmt, LONG *a)
{
    VFPrintf(out, (STRPTR)fmt, a);
}

static void puts_out(const char *s)
{
    FPuts(out, (STRPTR)s);
}

static void summary(LONG files, LONG dirs, LONG blocks)
{
    LONG a[2];
    BOOL first = TRUE;

    if (files) {
        a[0] = files;
        a[1] = (LONG)(files == 1 ? "" : "s");
        say("%ld file%s", a);
        first = FALSE;
    }
    if (dirs) {
        if (!first)
            puts_out(" - ");
        a[0] = dirs;
        a[1] = (LONG)(dirs == 1 ? "y" : "ies");
        say("%ld director%s", a);
        first = FALSE;
    }
    if (!first)
        puts_out(" - ");
    a[0] = blocks;
    a[1] = (LONG)(blocks == 1 ? "" : "s");
    say("%ld block%s used\n", a);
}

static void protbits(LONG prot, char *b)
{
    static const char flags[] = "hsparwed";
    int i;

    for (i = 0; i < 8; i++) {
        LONG bit = 1L << (7 - i);
        BOOL set = (prot & bit) != 0;
        if (i >= 4)
            set = !set;     /* rwed are "denied" bits */
        b[i] = set ? flags[i] : '-';
    }
    b[8] = '\0';
}

static void datestr(struct DateStamp *ds, BOOL subst, char *date, char *time)
{
    struct DateTime dt;

    memset(&dt, 0, sizeof(dt));
    dt.dat_Stamp = *ds;
    dt.dat_Format = FORMAT_DOS;
    dt.dat_Flags = subst ? DTF_SUBST : 0;
    dt.dat_StrDate = (STRPTR)date;
    dt.dat_StrTime = (STRPTR)time;
    date[0] = time[0] = '\0';
    DateToStr(&dt);
}

static LONG cmpstamp(struct DateStamp *a, struct DateStamp *b)
{
    if (a->ds_Days != b->ds_Days)
        return a->ds_Days < b->ds_Days ? -1 : 1;
    if (a->ds_Minute != b->ds_Minute)
        return a->ds_Minute < b->ds_Minute ? -1 : 1;
    if (a->ds_Tick != b->ds_Tick)
        return a->ds_Tick < b->ds_Tick ? -1 : 1;
    return 0;
}

static BOOL contains_nocase(const char *hay, const char *needle)
{
    int n = strlen(needle);
    for (; *hay; hay++)
        if (!strnicmp(hay, needle, n))
            return TRUE;
    return n == 0;
}

static void join(char *dst, const char *dir, const char *name)
{
    int n;
    strcpy(dst, dir);
    n = strlen(dst);
    if (n && dst[n - 1] != ':' && dst[n - 1] != '/')
        strcat(dst, "/");
    strcat(dst, name);
}

/* %p: the path prefix of the listed directory ("" or "dir/") */
static void path_prefix(const char *dir, char *buf)
{
    int n;
    strcpy(buf, dir);
    n = strlen(buf);
    if (n && buf[n - 1] != ':' && buf[n - 1] != '/')
        strcat(buf, "/");
}

static void lformat_entry(const char *fmt, struct FileInfoBlock *fib, const char *dir, BPTR dirlock)
{
    char date[16], time[16], prot[10], path[PATH_LEN], num[16];
    int nstr = 0, sidx = 0;
    const char *p;
    LONG a[1];

    for (p = fmt; *p; p++)
        if (p[0] == '%' && (p[1] == 's' || p[1] == 'S')) {
            nstr++;
            p++;
        }
    datestr(&fib->fib_Date, !args[A_DATES], date, time);
    protbits(fib->fib_Protection, prot);
    path_prefix(dir, path);

    for (p = fmt; *p; p++) {
        if (*p != '%' || !p[1]) {
            char c[2];
            c[0] = *p;
            c[1] = '\0';
            puts_out(c);
            continue;
        }
        p++;
        switch (*p) {
        case 'a': case 'A': puts_out(prot); break;
        case 'b': case 'B':
            if (fib->fib_DirEntryType >= 0) {
                puts_out("Dir");
            } else {
                a[0] = fib->fib_NumBlocks;
                say("%ld", a);
            }
            break;
        case 'c': case 'C': puts_out(fib->fib_Comment); break;
        case 'd': case 'D': puts_out(date); break;
        case 't': case 'T': puts_out(time); break;
        case 'f': case 'F': {
            char full[PATH_LEN];
            if (NameFromLock(dirlock, (STRPTR)full, sizeof(full))) {
                int n = strlen(full);
                if (n && full[n - 1] != ':')
                    strcat(full, "/");
                puts_out(full);
            }
            break;
        }
        case 'k': case 'K':
            a[0] = fib->fib_DiskKey;
            say("%ld", a);
            break;
        case 'l': case 'L':
            if (fib->fib_DirEntryType >= 0) {
                puts_out("Dir");
            } else {
                a[0] = fib->fib_Size;
                say("%ld", a);
            }
            break;
        case 'n': case 'N': puts_out(fib->fib_FileName); break;
        case 'p': case 'P': puts_out(path); break;
        case 's': case 'S':
            /* with two or more %s the first is the path, the rest the name */
            if (nstr >= 2 && sidx == 0)
                puts_out(path);
            else
                puts_out(fib->fib_FileName);
            sidx++;
            break;
        default:
            num[0] = '%';
            num[1] = *p;
            num[2] = '\0';
            puts_out(num);
            break;
        }
    }
    puts_out("\n");
}

static void print_entry(struct FileInfoBlock *fib, const char *dir, BPTR dirlock)
{
    char date[16], time[16], prot[10], size[16];
    LONG a[4];

    if (args[A_LFORMAT]) {
        lformat_entry((char *)args[A_LFORMAT], fib, dir, dirlock);
        return;
    }
    if (args[A_QUICK]) {
        a[0] = (LONG)fib->fib_FileName;
        say("%s\n", a);
        return;
    }
    if (fib->fib_DirEntryType >= 0)
        strcpy(size, "Dir");
    else if (fib->fib_Size == 0)
        strcpy(size, "empty");
    else {
        a[0] = args[A_BLOCK] ? fib->fib_NumBlocks : fib->fib_Size;
        RawDoFmt((STRPTR)"%ld", a, (void (*)())"\x16\xc0\x4e\x75", size);
    }
    protbits(fib->fib_Protection, prot);
    if (args[A_KEYS]) {
        a[0] = (LONG)fib->fib_FileName;
        a[1] = fib->fib_DiskKey;
        a[2] = (LONG)size;
        a[3] = (LONG)prot;
        say("%-17s [%5ld] %7s %s", a);
    } else {
        a[0] = (LONG)fib->fib_FileName;
        a[1] = (LONG)size;
        a[2] = (LONG)prot;
        say("%-24s %7s %s", a);
    }
    if (!args[A_NODATES]) {
        datestr(&fib->fib_Date, !args[A_DATES], date, time);
        a[0] = (LONG)date;
        a[1] = (LONG)time;
        say(" %-9s %s", a);
    }
    puts_out("\n");
    if (fib->fib_Comment[0]) {
        a[0] = (LONG)fib->fib_Comment;
        say(": %s\n", a);
    }
}

static BOOL wanted(struct FileInfoBlock *fib, const char *namepat)
{
    BOOL isdir = fib->fib_DirEntryType >= 0;

    if (args[A_FILES] && !args[A_DIRS] && isdir)
        return FALSE;
    if (args[A_DIRS] && !args[A_FILES] && !isdir)
        return FALSE;
    if (namepat && !MatchPatternNoCase((STRPTR)namepat, (STRPTR)fib->fib_FileName))
        return FALSE;
    if (have_pat && !MatchPatternNoCase((STRPTR)pat_parsed, (STRPTR)fib->fib_FileName))
        return FALSE;
    if (args[A_SUB] && !contains_nocase(fib->fib_FileName, (char *)args[A_SUB]))
        return FALSE;
    if (have_since && cmpstamp(&fib->fib_Date, &since) < 0)
        return FALSE;
    if (have_upto && cmpstamp(&fib->fib_Date, &upto) > 0)
        return FALSE;
    return TRUE;
}

static void header(const char *dir)
{
    struct DateStamp now;
    char day[16], date[16];
    struct DateTime dt;
    LONG a[3];

    DateStamp(&now);
    memset(&dt, 0, sizeof(dt));
    dt.dat_Stamp = now;
    dt.dat_Format = FORMAT_DOS;
    dt.dat_StrDay = (STRPTR)day;
    dt.dat_StrDate = (STRPTR)date;
    DateToStr(&dt);
    a[0] = (LONG)dir;
    a[1] = (LONG)day;
    a[2] = (LONG)date;
    say("Directory \"%s\" on %s %s\n", a);
}

static BOOL show_head(void)
{
    return !args[A_NOHEAD] && !args[A_LFORMAT];
}

/* subdirectory names collected for ALL */
struct SubDir {
    struct SubDir *next;
    char name[1];
};

/* list the directory 'lock' (named 'dir', "" = current) */
static void list_dir(BPTR lock, const char *dir, const char *namepat, BOOL top)
{
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    LONG files = 0, dirs = 0, blocks = 0, entries = 0;
    struct SubDir *subs = NULL, **tail = &subs;
    BOOL head_done = FALSE;

    if (!fib)
        return;
    if (!Examine(lock, fib)) {
        FreeDosObject(DOS_FIB, fib);
        return;
    }
    while (ExNext(lock, fib)) {
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            broken = TRUE;
            break;
        }
        entries++;
        if (!head_done) {
            if (dir[0] && show_head())
                header(dir);
            head_done = TRUE;
        }
        if (args[A_ALL] && fib->fib_DirEntryType >= 0) {
            struct SubDir *s = AllocVec(sizeof(*s) + strlen(fib->fib_FileName), MEMF_PUBLIC);
            if (s) {
                s->next = NULL;
                strcpy(s->name, fib->fib_FileName);
                *tail = s;
                tail = &s->next;
            }
        }
        if (!wanted(fib, namepat))
            continue;
        print_entry(fib, dir, lock);
        if (fib->fib_DirEntryType >= 0) {
            dirs++;
            blocks += 1;
        } else {
            files++;
            blocks += fib->fib_NumBlocks + 1;
        }
    }
    FreeDosObject(DOS_FIB, fib);

    if (!entries && !namepat && show_head()) {
        LONG a[1];
        a[0] = (LONG)dir;
        say("Directory \"%s\" is empty\n", a);
    } else if ((files || dirs) && show_head() && !(args[A_FILES] && args[A_DIRS])) {
        summary(files, dirs, blocks);
    }
    tot_files += files;
    tot_dirs += dirs;
    tot_blocks += blocks;

    while (subs) {
        struct SubDir *s = subs;
        subs = s->next;
        if (!broken) {
            char path[PATH_LEN];
            BPTR sl;
            BPTR old;
            join(path, dir, s->name);
            old = CurrentDir(lock);
            sl = Lock((STRPTR)s->name, SHARED_LOCK);
            CurrentDir(old);
            if (sl) {
                if (show_head())
                    puts_out("\n");
                list_dir(sl, path, namepat, FALSE);
                UnLock(sl);
            }
        }
        FreeVec(s);
    }
}

static BOOL parse_date(const char *s, struct DateStamp *ds)
{
    struct DateTime dt;
    char buf[32];

    memset(&dt, 0, sizeof(dt));
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    dt.dat_Format = FORMAT_DOS;
    dt.dat_Flags = DTF_SUBST;
    dt.dat_StrDate = (STRPTR)buf;
    if (!StrToDate(&dt))
        return FALSE;
    *ds = dt.dat_Stamp;
    return TRUE;
}

/* returns FALSE on a fatal error (the remaining arguments are skipped) */
static BOOL list_arg(const char *arg, int index, LONG *rc)
{
    char dir[PATH_LEN], pat[PATH_LEN * 2 + 2];
    BPTR lock;
    struct FileInfoBlock *fib;
    const char *file = (char *)FilePart((STRPTR)arg);
    LONG a[2];

    /* a pattern in the last component */
    if (ParsePatternNoCase((STRPTR)file, (STRPTR)pat, sizeof(pat)) == 1) {
        int n = (const char *)file - arg;
        strncpy(dir, arg, n);
        dir[n] = '\0';
        if (n && dir[n - 1] == '/')
            dir[n - 1] = '\0';
        lock = Lock((STRPTR)(dir[0] ? dir : ""), SHARED_LOCK);
        if (!lock) {
            LONG err = IoErr();
            Fault(err, NULL, (STRPTR)pat, sizeof(pat));
            a[0] = (LONG)arg;
            a[1] = (LONG)pat;
            say("No information for \"%s\": %s\n", a);
            SetIoErr(err);
            *rc = RETURN_FAIL;
            return FALSE;
        }
        if (index > 0 && dir[0] && show_head())
            puts_out("\n");
        list_dir(lock, dir, pat, TRUE);
        UnLock(lock);
        return TRUE;
    }

    lock = Lock((STRPTR)arg, SHARED_LOCK);
    if (!lock) {
        LONG err = IoErr();
        Fault(err, NULL, (STRPTR)pat, sizeof(pat));
        a[0] = (LONG)arg;
        a[1] = (LONG)pat;
        say("No information for \"%s\": %s\n", a);
        SetIoErr(err);
        *rc = RETURN_FAIL;
        return FALSE;
    }
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib || !Examine(lock, fib)) {
        if (fib)
            FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        *rc = RETURN_FAIL;
        return FALSE;
    }
    if (fib->fib_DirEntryType >= 0) {
        FreeDosObject(DOS_FIB, fib);
        if (index > 0 && arg[0] && show_head())
            puts_out("\n");
        list_dir(lock, arg, NULL, TRUE);
        UnLock(lock);
        return TRUE;
    }

    /* a single file: listed with its directory's header */
    {
        int n = (const char *)file - arg;
        BPTR parent = ParentDir(lock);
        strncpy(dir, arg, n);
        dir[n] = '\0';
        if (n && dir[n - 1] == '/')
            dir[n - 1] = '\0';
        if (index > 0 && dir[0] && show_head())
            puts_out("\n");
        if (dir[0] && show_head())
            header(dir);
        if (wanted(fib, NULL)) {
            print_entry(fib, dir, parent ? parent : lock);
            if (show_head()) {
                summary(1, 0, fib->fib_NumBlocks + 1);
            }
            tot_files++;
            tot_blocks += fib->fib_NumBlocks + 1;
        }
        if (parent)
            UnLock(parent);
    }
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return TRUE;
}

int main(void)
{
    struct RDArgs *rda;
    LONG rc = 0;
    BPTR tofh = 0;

    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        /* AmigaOS 3.1 List reports bad arguments but returns 0 */
        PrintFault(IoErr(), NULL);
        return 0;
    }
    out = Output();
    if (args[A_TO]) {
        tofh = Open((STRPTR)args[A_TO], MODE_NEWFILE);
        if (!tofh) {
            LONG err = IoErr();
            PrintFault(err, (STRPTR)args[A_TO]);
            FreeArgs(rda);
            SetIoErr(err);
            return RETURN_FAIL;
        }
        out = tofh;
    }
    if (args[A_PAT] &&
        ParsePatternNoCase((STRPTR)args[A_PAT], (STRPTR)pat_parsed, sizeof(pat_parsed)) >= 0)
        have_pat = TRUE;
    if (args[A_SINCE] || args[A_UPTO]) {
        BOOL ok = TRUE;
        if (args[A_SINCE])
            ok = have_since = parse_date((char *)args[A_SINCE], &since);
        if (ok && args[A_UPTO])
            ok = have_upto = parse_date((char *)args[A_UPTO], &upto);
        if (!ok) {
            have_since = have_upto = FALSE;
            puts_out("*** Invalid 'UPTO' or 'SINCE' parameter - ignored\n");
        } else if (have_upto) {
            /* UPTO includes the whole day */
            upto.ds_Minute = 23 * 60 + 59;
            upto.ds_Tick = 59 * TICKS_PER_SECOND + TICKS_PER_SECOND - 1;
        }
    }

    tot_files = tot_dirs = tot_blocks = 0;
    if (args[A_DIR]) {
        STRPTR *d;
        int i = 0;
        for (d = (STRPTR *)args[A_DIR]; *d && !broken; d++, i++)
            if (!list_arg((char *)*d, i, &rc))
                break;
    } else {
        list_arg("", 0, &rc);
    }

    if (!rc && args[A_ALL] && show_head() && (tot_files || tot_dirs)) {
        puts_out("\nTOTAL: ");
        summary(tot_files, tot_dirs, tot_blocks);
    }
    if (broken) {
        puts_out("***Break\n");
        rc = RETURN_WARN;
    }
    if (tofh)
        Close(tofh);
    FreeArgs(rda);
    return rc;
}
