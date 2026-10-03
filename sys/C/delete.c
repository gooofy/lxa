/*
 * DELETE - delete files and directories
 *
 * Template: FILE/M/A,ALL/S,QUIET/S,FORCE/S
 *
 * Output of AmigaOS 3.1's Delete (verified on the reference, Phase 221):
 *   <name>  Deleted                    for every deleted object (not QUIET)
 *   <name>  Not Deleted: <fault text>  (RC 20)
 *   No file to delete                  nothing matched (RC 5)
 * With ALL the contents of a directory go first.  FORCE also deletes
 * delete-protected objects.  Bad arguments are reported, then Delete
 * finds no file to delete (RC 5), as on AmigaOS 3.1.
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

#define TEMPLATE "FILE/M/A,ALL/S,QUIET/S,FORCE/S"

enum { A_FILE, A_ALL, A_QUIET, A_FORCE, A_COUNT };

static BOOL quiet, force, all;
static LONG deleted = 0;
static LONG rc = 0;
static BOOL broken = FALSE;

static BOOL delete_one(const char *name, LONG prot)
{
    if (force && (prot & FIBF_DELETE))
        SetProtection((STRPTR)name, prot & ~FIBF_DELETE);
    if (DeleteFile((STRPTR)name)) {
        deleted++;
        if (!quiet)
            Printf((STRPTR)"%s  Deleted\n", (LONG)name);
        return TRUE;
    }
    {
        LONG err = IoErr();
        char buf[100];
        Fault(err, NULL, (STRPTR)buf, sizeof(buf));
        Printf((STRPTR)"%s  Not Deleted: %s\n", (LONG)name, (LONG)buf);
        SetIoErr(err);
        rc = RETURN_FAIL;
        return FALSE;
    }
}

/* delete the contents of directory 'path' (ALL) */
static BOOL delete_contents(const char *path)
{
    BPTR lock = Lock((STRPTR)path, SHARED_LOCK);
    struct FileInfoBlock *fib;
    char sub[512];
    BOOL ok = TRUE;

    if (!lock)
        return FALSE;
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return FALSE;
    }
    /* deleting while scanning: restart the scan after every deletion */
    for (;;) {
        BOOL found = FALSE;
        if (!Examine(lock, fib))
            break;
        while (ExNext(lock, fib)) {
            if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
                broken = TRUE;
                ok = FALSE;
                break;
            }
            strcpy(sub, path);
            AddPart((STRPTR)sub, fib->fib_FileName, sizeof(sub));
            if (fib->fib_DirEntryType >= 0 && !delete_contents(sub)) {
                ok = FALSE;
                break;
            }
            if (!delete_one(sub, fib->fib_Protection)) {
                ok = FALSE;
                break;
            }
            found = TRUE;
            break;
        }
        if (!found || !ok)
            break;
    }
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return ok;
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    STRPTR *files;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        PutStr((STRPTR)"No file to delete\n");
        SetIoErr(0);
        return RETURN_WARN;
    }
    quiet = args[A_QUIET] != 0;
    force = args[A_FORCE] != 0;
    all = args[A_ALL] != 0;

    for (files = (STRPTR *)args[A_FILE]; *files && !rc && !broken; files++) {
        struct AnchorPath *ap = AllocVec(sizeof(struct AnchorPath) + 512, MEMF_PUBLIC | MEMF_CLEAR);
        LONG err;
        if (!ap)
            break;
        ap->ap_Strlen = 512;
        ap->ap_BreakBits = SIGBREAKF_CTRL_C;
        /* collect first, then delete (deleting changes the directory) */
        {
            struct Name { struct Name *next; LONG prot; BOOL dir; char name[1]; } *list = NULL, **tail = &list, *n;
            for (err = MatchFirst(*files, ap); !err; err = MatchNext(ap)) {
                n = AllocVec(sizeof(*n) + strlen(ap->ap_Buf), MEMF_PUBLIC);
                if (!n)
                    break;
                n->next = NULL;
                n->prot = ap->ap_Info.fib_Protection;
                n->dir = ap->ap_Info.fib_DirEntryType >= 0;
                strcpy(n->name, ap->ap_Buf);
                *tail = n;
                tail = &n->next;
            }
            MatchEnd(ap);
            if (err == ERROR_BREAK)
                broken = TRUE;
            while (list) {
                n = list;
                list = n->next;
                if (!rc && !broken) {
                    if (n->dir && all)
                        delete_contents(n->name);
                    if (!rc && !broken)
                        delete_one(n->name, n->prot);
                }
                FreeVec(n);
            }
        }
        FreeVec(ap);
    }

    if (broken) {
        PutStr((STRPTR)"***Break\n");
        rc = RETURN_WARN;
    } else if (!rc && !deleted) {
        PutStr((STRPTR)"No file to delete\n");
        SetIoErr(0);
        rc = RETURN_WARN;
    }
    FreeArgs(rda);
    return rc;
}
