/*
 * COPY - copy files and directories
 *
 * Template: FROM/M,TO/A,ALL/S,QUIET/S,BUF=BUFFER/K/N,CLONE/S,DATES/S,
 *           NOPRO/S,COM/S,NOREQ/S
 *
 * Output of AmigaOS 3.1's Copy (verified on the reference, Phase 221):
 *  - one file to a file or into a directory: silent;
 *  - several sources, patterns and directories report every object:
 *      "   <dest>   [created]"      a destination directory Copy creates
 *      "   <file>..copied."         (indented 8 more per directory level)
 *      "        <dir> (Dir)"        a subdirectory (copied with ALL:
 *                                   "        <dir> (Dir)   [created]")
 *  - "Can't open <from> for input - <fault>" (RC 20),
 *    "Can't open <to> for output - <fault>" (RC 5).
 * CLONE copies date, protection and comment; DATES the date; COM the
 * comment; NOPRO gives the copy default protection.
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

#define TEMPLATE "FROM/M,TO/A,ALL/S,QUIET/S,BUF=BUFFER/K/N,CLONE/S,DATES/S,NOPRO/S,COM/S,NOREQ/S"

enum { A_FROM, A_TO, A_ALL, A_QUIET, A_BUF, A_CLONE, A_DATES, A_NOPRO, A_COM, A_NOREQ, A_COUNT };

#define PATH_LEN 512

static LONG args[A_COUNT];
static BOOL verbose;            /* report objects (several sources) */
static BOOL broken = FALSE;
static UBYTE *buffer;
static LONG bufsize;
static LONG rc = 0;

static void indent(int n)
{
    while (n-- > 0)
        PutStr((STRPTR)" ");
}

static BOOL check_break(void)
{
    if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
        broken = TRUE;
    return broken;
}

static void fault_line(const char *fmt, const char *name, LONG err)
{
    char buf[100];
    Fault(err, NULL, (STRPTR)buf, sizeof(buf));
    Printf((STRPTR)fmt, (LONG)name, (LONG)buf);
}

/* copy the file 'from' to 'to'; fib describes the source */
static BOOL copy_file(const char *from, const char *to, struct FileInfoBlock *fib, int level,
                      const char *shown)
{
    BPTR in, out;
    LONG n;

    if (verbose && !args[A_QUIET]) {
        indent(3 + 8 * level);
        Printf((STRPTR)"%s..", (LONG)shown);
        Flush(Output());
    }
    in = Open((STRPTR)from, MODE_OLDFILE);
    if (!in) {
        LONG err = IoErr();
        if (verbose && !args[A_QUIET])
            PutStr((STRPTR)"\n");
        fault_line("Can't open %s for input - %s\n", from, err);
        SetIoErr(err);
        rc = RETURN_FAIL;
        return FALSE;
    }
    out = Open((STRPTR)to, MODE_NEWFILE);
    if (!out) {
        LONG err = IoErr();
        Close(in);
        if (verbose && !args[A_QUIET])
            PutStr((STRPTR)"\n");
        fault_line("Can't open %s for output - %s\n", to, err);
        SetIoErr(0);
        rc = RETURN_WARN;
        return FALSE;
    }
    while ((n = Read(in, buffer, bufsize)) > 0) {
        if (check_break())
            break;
        if (Write(out, buffer, n) != n) {
            LONG err = IoErr();
            Close(in);
            Close(out);
            DeleteFile((STRPTR)to);
            fault_line("Error writing %s - %s\n", to, err);
            SetIoErr(err);
            rc = RETURN_FAIL;
            return FALSE;
        }
    }
    Close(in);
    Close(out);
    if (broken) {
        DeleteFile((STRPTR)to);
        return FALSE;
    }
    if (args[A_CLONE] || args[A_DATES])
        SetFileDate((STRPTR)to, &fib->fib_Date);
    if ((args[A_CLONE] || args[A_COM]) && fib->fib_Comment[0])
        SetComment((STRPTR)to, fib->fib_Comment);
    if (!args[A_NOPRO])
        SetProtection((STRPTR)to, fib->fib_Protection & ~FIBF_ARCHIVE);
    if (verbose && !args[A_QUIET])
        PutStr((STRPTR)"copied.\n");
    return TRUE;
}

static BOOL is_dir(const char *name)
{
    BPTR lock = Lock((STRPTR)name, SHARED_LOCK);
    BOOL dir = FALSE;
    if (lock) {
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        if (fib && Examine(lock, fib))
            dir = fib->fib_DirEntryType >= 0;
        if (fib)
            FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
    }
    return dir;
}

/* copy the contents of directory 'from' into the directory 'to' */
static BOOL copy_dir(const char *from, const char *to, int level)
{
    BPTR lock = Lock((STRPTR)from, SHARED_LOCK);
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    char src[PATH_LEN], dst[PATH_LEN];
    BOOL ok = TRUE;

    if (!lock || !fib) {
        if (lock)
            UnLock(lock);
        if (fib)
            FreeDosObject(DOS_FIB, fib);
        return FALSE;
    }
    if (Examine(lock, fib)) {
        while (ok && !check_break() && ExNext(lock, fib)) {
            strcpy(src, from);
            AddPart((STRPTR)src, fib->fib_FileName, sizeof(src));
            strcpy(dst, to);
            AddPart((STRPTR)dst, fib->fib_FileName, sizeof(dst));
            if (fib->fib_DirEntryType >= 0) {
                if (!args[A_QUIET]) {
                    indent(8 * (level + 1));
                    Printf((STRPTR)"%s (Dir)", (LONG)fib->fib_FileName);
                }
                if (args[A_ALL]) {
                    BPTR nd;
                    BOOL created = FALSE;
                    if (!is_dir(dst)) {
                        nd = CreateDir((STRPTR)dst);
                        if (!nd) {
                            LONG err = IoErr();
                            if (!args[A_QUIET])
                                PutStr((STRPTR)"\n");
                            fault_line("Can't create directory %s - %s\n", dst, err);
                            rc = RETURN_FAIL;
                            ok = FALSE;
                            break;
                        }
                        UnLock(nd);
                        created = TRUE;
                    }
                    if (!args[A_QUIET])
                        PutStr((STRPTR)(created ? "   [created]\n" : "\n"));
                    ok = copy_dir(src, dst, level + 1);
                } else if (!args[A_QUIET]) {
                    PutStr((STRPTR)"\n");
                }
            } else {
                ok = copy_file(src, dst, fib, level, (char *)fib->fib_FileName);
            }
        }
    }
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return ok;
}

int main(void)
{
    struct RDArgs *rda;
    STRPTR *from;
    const char *to;
    char pat[PATH_LEN * 2 + 2], dst[PATH_LEN];
    int nfrom = 0;
    BOOL to_is_dir, multi = FALSE;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), (STRPTR)"Copy");
        return RETURN_FAIL;
    }
    from = (STRPTR *)args[A_FROM];
    to = (char *)args[A_TO];
    if (!from || !*from) {
        PrintFault(ERROR_REQUIRED_ARG_MISSING, (STRPTR)"Copy");
        FreeArgs(rda);
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return RETURN_FAIL;
    }
    bufsize = args[A_BUF] ? *(LONG *)args[A_BUF] * 512 : 128 * 512;
    if (bufsize < 512)
        bufsize = 512;
    buffer = AllocVec(bufsize, MEMF_PUBLIC);
    if (!buffer) {
        PrintFault(ERROR_NO_FREE_STORE, (STRPTR)"Copy");
        FreeArgs(rda);
        return RETURN_FAIL;
    }

    while (from[nfrom])
        nfrom++;
    if (nfrom > 1 || ParsePatternNoCase(from[0], (STRPTR)pat, sizeof(pat)) == 1 || is_dir((char *)from[0]))
        multi = TRUE;
    verbose = multi;
    to_is_dir = is_dir(to);

    if (multi && !to_is_dir) {
        BPTR nd = CreateDir((STRPTR)to);
        if (!nd) {
            LONG err = IoErr();
            fault_line("Can't create directory %s - %s\n", to, err);
            FreeVec(buffer);
            FreeArgs(rda);
            SetIoErr(err);
            return RETURN_FAIL;
        }
        UnLock(nd);
        if (!args[A_QUIET])
            Printf((STRPTR)"   %s   [created]\n", (LONG)to);
        to_is_dir = TRUE;
    }

    for (; *from && !rc && !broken; from++) {
        if (ParsePatternNoCase(*from, (STRPTR)pat, sizeof(pat)) == 1) {
            struct AnchorPath *ap = AllocVec(sizeof(struct AnchorPath) + PATH_LEN, MEMF_PUBLIC | MEMF_CLEAR);
            LONG err;
            if (!ap)
                break;
            ap->ap_Strlen = PATH_LEN;
            ap->ap_BreakBits = SIGBREAKF_CTRL_C;
            for (err = MatchFirst(*from, ap); !err && !rc; err = MatchNext(ap)) {
                strcpy(dst, to);
                AddPart((STRPTR)dst, ap->ap_Info.fib_FileName, sizeof(dst));
                if (ap->ap_Info.fib_DirEntryType >= 0) {
                    if (!args[A_QUIET])
                        Printf((STRPTR)"        %s (Dir)", (LONG)ap->ap_Info.fib_FileName);
                    if (args[A_ALL]) {
                        BOOL created = FALSE;
                        if (!is_dir(dst)) {
                            BPTR nd = CreateDir((STRPTR)dst);
                            if (nd) {
                                UnLock(nd);
                                created = TRUE;
                            }
                        }
                        if (!args[A_QUIET])
                            PutStr((STRPTR)(created ? "   [created]\n" : "\n"));
                        copy_dir((char *)ap->ap_Buf, dst, 1);
                    } else if (!args[A_QUIET]) {
                        PutStr((STRPTR)"\n");
                    }
                } else {
                    copy_file((char *)ap->ap_Buf, dst, &ap->ap_Info, 0, (char *)ap->ap_Info.fib_FileName);
                }
            }
            if (err == ERROR_BREAK)
                broken = TRUE;
            MatchEnd(ap);
            FreeVec(ap);
            continue;
        }

        {
            BPTR lock = Lock(*from, SHARED_LOCK);
            struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
            if (!lock || !fib || !Examine(lock, fib)) {
                LONG err = IoErr();
                fault_line("Can't open %s for input - %s\n", (char *)*from, err);
                SetIoErr(err);
                rc = RETURN_FAIL;
                if (lock)
                    UnLock(lock);
                if (fib)
                    FreeDosObject(DOS_FIB, fib);
                break;
            }
            UnLock(lock);
            if (fib->fib_DirEntryType >= 0) {
                copy_dir((char *)*from, to, 0);
            } else {
                strcpy(dst, to);
                if (to_is_dir)
                    AddPart((STRPTR)dst, FilePart(*from), sizeof(dst));
                copy_file((char *)*from, dst, fib, 0, (char *)FilePart(*from));
            }
            FreeDosObject(DOS_FIB, fib);
        }
    }

    if (broken) {
        PutStr((STRPTR)"***Break\n");
        rc = RETURN_WARN;
    }
    FreeVec(buffer);
    FreeArgs(rda);
    return rc;
}
