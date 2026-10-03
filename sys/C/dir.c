/*
 * DIR - list directory contents
 *
 * Template: DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S
 *
 * Layout of AmigaOS 3.1's Dir (verified on the reference, Phase 221):
 * directories first, one per line ("     name (dir)", in directory order;
 * with ALL their contents follow, indented by five more columns), then the
 * files sorted by name (case-insensitive) in two columns ("  %-33s%s").
 * A pattern selects entries; directories found through a pattern are shown
 * with their full path.  OPT takes the letters A (ALL), D (DIRS), F (FILES)
 * and I (INTER).  INTER asks for every entry: Q quits, B goes back up,
 * E enters a directory, DEL deletes, T types the file, C/<cmd> runs a
 * command; RETURN goes on.
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

#define TEMPLATE "DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S"

enum { A_DIR, A_OPT, A_ALL, A_DIRS, A_FILES, A_INTER, A_COUNT };

struct Entry {
    struct Entry *next;
    BOOL dir;
    char name[1];
};

static BOOL opt_all, opt_dirs, opt_files, opt_inter;
static BOOL broken = FALSE, quit = FALSE;

static void spaces(int n)
{
    while (n-- > 0)
        PutStr((STRPTR)" ");
}

static void padded(const char *s, int width)
{
    int n = strlen(s);
    PutStr((STRPTR)s);
    spaces(width - n);
}

static void free_list(struct Entry *e)
{
    while (e) {
        struct Entry *n = e->next;
        FreeVec(e);
        e = n;
    }
}

/* INTER: ask about one entry; returns FALSE to stop listing this level */
static BOOL inter(BPTR lock, const char *name, BOOL isdir, int indent)
{
    char buf[128];
    LONG n;

    for (;;) {
        spaces(indent + 2);
        PutStr((STRPTR)name);
        PutStr((STRPTR)(isdir ? " (dir) ? " : " ? "));
        Flush(Output());
        n = Read(Input(), buf, sizeof(buf) - 1);
        if (n <= 0) {
            quit = TRUE;
            return FALSE;
        }
        buf[n] = '\0';
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == ' '))
            buf[--n] = '\0';
        if (!buf[0])
            return TRUE;
        if (!stricmp(buf, "Q")) {
            quit = TRUE;
            return FALSE;
        }
        if (!stricmp(buf, "B"))
            return FALSE;
        if (!stricmp(buf, "DEL")) {
            BPTR old = CurrentDir(lock);
            if (!DeleteFile((STRPTR)name))
                PrintFault(IoErr(), (STRPTR)name);
            CurrentDir(old);
            return TRUE;
        }
        if (!stricmp(buf, "T") && !isdir) {
            BPTR old = CurrentDir(lock);
            BPTR fh = Open((STRPTR)name, MODE_OLDFILE);
            if (fh) {
                char data[256];
                LONG k;
                while ((k = Read(fh, data, sizeof(data))) > 0)
                    Write(Output(), data, k);
                Close(fh);
            }
            CurrentDir(old);
            return TRUE;
        }
        if ((buf[0] == 'C' || buf[0] == 'c') && (buf[1] == ' ' || !buf[1])) {
            BPTR old = CurrentDir(lock);
            SystemTagList((STRPTR)(buf[1] ? buf + 2 : ""), NULL);
            CurrentDir(old);
            return TRUE;
        }
        if (!stricmp(buf, "E") && isdir)
            return TRUE;
        if (!stricmp(buf, "?")) {
            PutStr((STRPTR)"Q=Quit B=Back E=Enter DEL=Delete T=Type C=Command\n");
            continue;
        }
        return TRUE;
    }
}

static void list_level(BPTR lock, int indent, const char *pattern, BOOL fullpath)
{
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    struct Entry *dirs = NULL, **dtail = &dirs, *files = NULL;
    struct Entry *e;
    int count;

    if (!fib)
        return;
    if (Examine(lock, fib)) {
        while (ExNext(lock, fib)) {
            int len;
            if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
                broken = TRUE;
                break;
            }
            if (pattern && !MatchPatternNoCase((STRPTR)pattern, fib->fib_FileName))
                continue;
            len = strlen(fib->fib_FileName);
            e = AllocVec(sizeof(*e) + len, MEMF_PUBLIC);
            if (!e)
                break;
            e->next = NULL;
            e->dir = fib->fib_DirEntryType >= 0;
            strcpy(e->name, fib->fib_FileName);
            if (e->dir) {
                *dtail = e;
                dtail = &e->next;
            } else {
                /* files sorted by name */
                struct Entry **pp = &files;
                while (*pp && stricmp((*pp)->name, e->name) <= 0)
                    pp = &(*pp)->next;
                e->next = *pp;
                *pp = e;
            }
        }
    }
    FreeDosObject(DOS_FIB, fib);

    if (!opt_files || opt_dirs) {
        for (e = dirs; e && !broken && !quit; e = e->next) {
            if (opt_inter) {
                if (!inter(lock, e->name, TRUE, indent))
                    break;
            } else {
                spaces(indent + 5);
                if (fullpath) {
                    char full[512];
                    if (NameFromLock(lock, (STRPTR)full, sizeof(full)) &&
                        AddPart((STRPTR)full, (STRPTR)e->name, sizeof(full)))
                        PutStr((STRPTR)full);
                    else
                        PutStr((STRPTR)e->name);
                } else {
                    PutStr((STRPTR)e->name);
                }
                PutStr((STRPTR)" (dir)\n");
            }
            if (opt_all) {
                BPTR old = CurrentDir(lock);
                BPTR sub = Lock((STRPTR)e->name, SHARED_LOCK);
                CurrentDir(old);
                if (sub) {
                    list_level(sub, indent + 5, NULL, FALSE);
                    UnLock(sub);
                }
            }
        }
    }

    if (!opt_dirs || opt_files) {
        count = 0;
        for (e = files; e && !broken && !quit; e = e->next) {
            if (opt_inter) {
                if (!inter(lock, e->name, FALSE, indent))
                    break;
                continue;
            }
            if (count % 2 == 0) {
                spaces(indent + 2);
                padded(e->name, 33);
            } else {
                PutStr((STRPTR)e->name);
                PutStr((STRPTR)"\n");
            }
            count++;
        }
        if (!opt_inter && count % 2)
            PutStr((STRPTR)"\n");
    }
    free_list(dirs);
    free_list(files);
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    const char *arg;
    char pat[520], dirpart[512];
    BPTR lock;
    LONG rc = 0;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    opt_all = args[A_ALL] != 0;
    opt_dirs = args[A_DIRS] != 0;
    opt_files = args[A_FILES] != 0;
    opt_inter = args[A_INTER] != 0;
    if (args[A_OPT]) {
        const char *o;
        for (o = (char *)args[A_OPT]; *o; o++) {
            switch (*o) {
            case 'a': case 'A': opt_all = TRUE; break;
            case 'd': case 'D': opt_dirs = TRUE; break;
            case 'f': case 'F': opt_files = TRUE; break;
            case 'i': case 'I': opt_inter = TRUE; break;
            }
        }
    }

    arg = args[A_DIR] ? (char *)args[A_DIR] : "";
    if (*arg && ParsePatternNoCase(FilePart((STRPTR)arg), (STRPTR)pat, sizeof(pat)) == 1) {
        int n = (char *)FilePart((STRPTR)arg) - arg;
        strncpy(dirpart, arg, n);
        dirpart[n] = '\0';
        lock = Lock((STRPTR)dirpart, SHARED_LOCK);
        if (!lock) {
            LONG err = IoErr();
            Printf((STRPTR)"Could not get information for %s\n", (LONG)arg);
            PrintFault(err, NULL);
            FreeArgs(rda);
            SetIoErr(err);
            return RETURN_FAIL;
        }
        list_level(lock, 0, pat, TRUE);
        UnLock(lock);
    } else {
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        lock = Lock((STRPTR)arg, SHARED_LOCK);
        if (!lock || !fib || !Examine(lock, fib)) {
            LONG err = IoErr();
            Printf((STRPTR)"Could not get information for %s\n", (LONG)arg);
            PrintFault(err, NULL);
            if (lock)
                UnLock(lock);
            if (fib)
                FreeDosObject(DOS_FIB, fib);
            FreeArgs(rda);
            SetIoErr(err);
            return RETURN_FAIL;
        }
        if (fib->fib_DirEntryType < 0) {
            Printf((STRPTR)"%s is not a directory\n", (LONG)arg);
            PrintFault(ERROR_DIR_NOT_FOUND, NULL);
            UnLock(lock);
            FreeDosObject(DOS_FIB, fib);
            FreeArgs(rda);
            SetIoErr(ERROR_DIR_NOT_FOUND);
            return RETURN_FAIL;
        }
        FreeDosObject(DOS_FIB, fib);
        list_level(lock, 0, NULL, FALSE);
        UnLock(lock);
    }
    if (broken) {
        PutStr((STRPTR)"***Break\n");
        rc = RETURN_WARN;
    }
    FreeArgs(rda);
    return rc;
}
