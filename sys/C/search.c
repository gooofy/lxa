/*
 * SEARCH - find text in files
 *
 * Template: FROM/M,SEARCH/A,ALL/S,NONUM/S,QUIET/S,QUICK/S,FILE/S,PATTERN/S
 *
 * Output of AmigaOS 3.1's Search (verified on the reference, Phase 221):
 *  - every file of a pattern or directory is announced ("   name.."),
 *    subdirectories (ALL) as "     name (dir)", nested 5 more columns;
 *    an explicitly named file is not announced;
 *  - matching lines: "%6ld %s" (NONUM: the line alone);
 *  - QUIET prints only the full names of matching files;
 *  - FILE matches the file names instead of the contents, PATTERN takes
 *    SEARCH as an AmigaDOS pattern; matching is case-insensitive;
 *  - RC 0 if something was found, 5 if not, 20 for errors.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

#pragma GCC diagnostic ignored "-Wpointer-sign"

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "FROM/M,SEARCH/A,ALL/S,NONUM/S,QUIET/S,QUICK/S,FILE/S,PATTERN/S"

enum { A_FROM, A_SEARCH, A_ALL, A_NONUM, A_QUIET, A_QUICK, A_FILE, A_PATTERN, A_COUNT };

#define LINE_LEN 512

static LONG args[A_COUNT];
static char pattern[LINE_LEN * 2 + 2];
static BOOL found = FALSE, broken = FALSE;
static LONG rc = 0;

static void indent(int n)
{
    while (n-- > 0)
        PutStr((STRPTR)" ");
}

static BOOL contains(const char *line, const char *what)
{
    int n = strlen(what);
    for (; *line; line++)
        if (!strnicmp(line, what, n))
            return TRUE;
    return n == 0;
}

static BOOL matches(const char *text)
{
    if (args[A_PATTERN])
        return MatchPatternNoCase((STRPTR)pattern, (STRPTR)text);
    return contains(text, (char *)args[A_SEARCH]);
}

static void search_file(const char *path, const char *name, int level, BOOL announce)
{
    BPTR fh;
    static char line[LINE_LEN];
    LONG num = 0;

    if (announce && !args[A_QUIET]) {
        indent(3 + 5 * level);
        Printf((STRPTR)"%s..\n", (LONG)name);
    }
    if (args[A_FILE]) {
        /* names are matched for the files a pattern or directory yields */
        if (announce && matches(name)) {
            found = TRUE;
            if (args[A_QUIET])
                Printf((STRPTR)"%s\n", (LONG)path);
        }
        return;
    }
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh) {
        PrintFault(IoErr(), NULL);
        rc = RETURN_FAIL;
        return;
    }
    while (FGets(fh, (STRPTR)line, sizeof(line))) {
        int n = strlen(line);
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            broken = TRUE;
            break;
        }
        num++;
        if (n && line[n - 1] == '\n')
            line[--n] = '\0';
        if (!matches(line))
            continue;
        found = TRUE;
        if (args[A_QUIET]) {
            char full[512];
            if (NameFromFH(fh, (STRPTR)full, sizeof(full)))
                Printf((STRPTR)"%s\n", (LONG)full);
            else
                Printf((STRPTR)"%s\n", (LONG)path);
            break;
        }
        if (args[A_NONUM])
            Printf((STRPTR)"%s\n", (LONG)line);
        else
            Printf((STRPTR)"%6ld %s\n", num, (LONG)line);
    }
    Close(fh);
}

static void search_dir(BPTR lock, const char *path, const char *pat, int level)
{
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    char sub[512];

    if (!fib)
        return;
    if (Examine(lock, fib)) {
        while (!broken && !rc && ExNext(lock, fib)) {
            if (pat && !MatchPatternNoCase((STRPTR)pat, fib->fib_FileName))
                continue;
            strcpy(sub, path);
            AddPart((STRPTR)sub, fib->fib_FileName, sizeof(sub));
            if (fib->fib_DirEntryType >= 0) {
                if (args[A_ALL]) {
                    BPTR sl = Lock((STRPTR)sub, SHARED_LOCK);
                    if (!args[A_QUIET]) {
                        indent(5 + 5 * level);
                        Printf((STRPTR)"%s (dir)\n", (LONG)fib->fib_FileName);
                    }
                    if (sl) {
                        search_dir(sl, sub, NULL, level + 1);
                        UnLock(sl);
                    }
                }
                continue;
            }
            search_file(sub, (char *)fib->fib_FileName, level, TRUE);
        }
    }
    FreeDosObject(DOS_FIB, fib);
}

int main(void)
{
    struct RDArgs *rda;
    STRPTR *from;
    static STRPTR here[2] = { (STRPTR)"", NULL };
    char pat[LINE_LEN * 2 + 2], dir[512];

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    if (args[A_PATTERN] &&
        ParsePatternNoCase((STRPTR)args[A_SEARCH], (STRPTR)pattern, sizeof(pattern)) < 0) {
        PrintFault(IoErr(), NULL);
        FreeArgs(rda);
        return RETURN_FAIL;
    }
    from = args[A_FROM] ? (STRPTR *)args[A_FROM] : here;
    for (; *from && !broken && !rc; from++) {
        const char *name = (char *)*from;
        if (*name && ParsePatternNoCase(FilePart((STRPTR)name), (STRPTR)pat, sizeof(pat)) == 1) {
            int n = (char *)FilePart((STRPTR)name) - name;
            BPTR lock;
            strncpy(dir, name, n);
            dir[n] = '\0';
            lock = Lock((STRPTR)dir, SHARED_LOCK);
            if (!lock) {
                LONG err = IoErr();
                PrintFault(err, NULL);
                SetIoErr(err);
                rc = RETURN_FAIL;
                break;
            }
            search_dir(lock, dir, pat, 0);
            UnLock(lock);
        } else {
            BPTR lock = Lock((STRPTR)name, SHARED_LOCK);
            struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
            if (!lock || !fib || !Examine(lock, fib)) {
                LONG err = IoErr();
                PrintFault(err, NULL);
                SetIoErr(err);
                rc = RETURN_FAIL;
                if (lock)
                    UnLock(lock);
                if (fib)
                    FreeDosObject(DOS_FIB, fib);
                break;
            }
            if (fib->fib_DirEntryType >= 0) {
                /* a directory argument is shown like an entry */
                if (!args[A_QUIET])
                    Printf((STRPTR)"     %s (dir)\n", (LONG)fib->fib_FileName);
                search_dir(lock, name, NULL, 1);
            }
            else
                search_file(name, (char *)fib->fib_FileName, 0, FALSE);
            FreeDosObject(DOS_FIB, fib);
            UnLock(lock);
        }
    }
    if (broken) {
        PutStr((STRPTR)"***Break\n");
        rc = RETURN_WARN;
    }
    if (!rc && !found)
        rc = RETURN_WARN;
    FreeArgs(rda);
    return rc;
}
