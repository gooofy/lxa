/*
 * WHICH - find a command the way the shell does
 *
 * Template: FILE/A,NORES/S,RES/S,ALL/S
 *
 * Search order (as the shell): the resident list ("RES <name>"), the
 * internal commands ("INTERNAL <name>"), the current directory, the path
 * (cli_CommandDir) and C: - files are shown with their full name.  NORES
 * skips the resident list, RES searches only there, ALL shows every match.
 * Nothing found: no output, RC 5, IoErr() 205 (AmigaOS 3.1, verified on
 * the reference, Phase 221).
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#ifndef CMD_INTERNAL
#define CMD_INTERNAL -2
#endif
#ifndef CMD_DISABLED
#define CMD_DISABLED -999
#endif

#define TEMPLATE "FILE/A,NORES/S,RES/S,ALL/S"

enum { A_FILE, A_NORES, A_RES, A_ALL, A_COUNT };

struct PathEntry {
    BPTR next;
    BPTR lock;
};

static int found = 0;

/* name in directory 'dir': print its full name */
static BOOL try_dir(BPTR dir, const char *name)
{
    BPTR old = CurrentDir(dir);
    BPTR lock = Lock((STRPTR)name, SHARED_LOCK);
    BOOL ok = FALSE;

    CurrentDir(old);
    if (lock) {
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        char full[512];
        if (fib && Examine(lock, fib) && fib->fib_DirEntryType < 0 &&
            NameFromLock(lock, (STRPTR)full, sizeof(full))) {
            Printf((STRPTR)"%s\n", (LONG)full);
            found++;
            ok = TRUE;
        }
        if (fib)
            FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
    }
    return ok;
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    struct Process *me = (struct Process *)FindTask(NULL);
    struct CommandLineInterface *cli = Cli();
    const char *name;
    BOOL all;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        /* AmigaOS 3.1: RC 5 */
        PrintFault(IoErr(), NULL);
        return RETURN_WARN;
    }
    name = (char *)args[A_FILE];
    all = args[A_ALL] != 0;

    if (!args[A_NORES] && !strchr(name, ':') && !strchr(name, '/')) {
        struct Segment *seg;
        Forbid();
        seg = FindSegment((STRPTR)name, NULL, FALSE);
        if (seg) {
            Permit();
            Printf((STRPTR)"RES %s\n", (LONG)name);
            found++;
        } else {
            seg = FindSegment((STRPTR)name, NULL, TRUE);
            Permit();
            if (seg && seg->seg_UC == CMD_INTERNAL) {
                char nm[64];
                int l = seg->seg_Name[0];
                if (l > 63)
                    l = 63;
                CopyMem(&seg->seg_Name[1], nm, l);
                nm[l] = '\0';
                Printf((STRPTR)"INTERNAL %s\n", (LONG)nm);
                found++;
            }
        }
    }

    if (!args[A_RES] && (all || !found)) {
        BOOL done = FALSE;
        if (try_dir(me->pr_CurrentDir, name) && !all)
            done = TRUE;
        if (!done && !strchr(name, ':') && cli) {
            struct PathEntry *pe;
            for (pe = BADDR(cli->cli_CommandDir); pe && !done; pe = BADDR(pe->next))
                if (try_dir(pe->lock, name) && !all)
                    done = TRUE;
        }
        if (!done && !strchr(name, ':')) {
            BPTR c = Lock((STRPTR)"C:", SHARED_LOCK);
            if (c) {
                try_dir(c, name);
                UnLock(c);
            }
        }
    }

    FreeArgs(rda);
    if (!found) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return RETURN_WARN;
    }
    return 0;
}
