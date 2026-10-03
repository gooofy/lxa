/*
 * RENAME - rename or move files and directories
 *
 * Template: FROM/A/M,TO=AS/A,QUIET/S
 *
 * Several sources (or a pattern) move into the directory TO.  As AmigaOS
 * 3.1 (verified on the reference, Phase 221): silent on success,
 *   Can't rename <from> as <to> because <fault text>      (RC 20)
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

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "FROM/A/M,TO=AS/A,QUIET/S"

enum { A_FROM, A_TO, A_QUIET, A_COUNT };

static LONG do_rename(const char *from, const char *to)
{
    char buf[100];

    if (Rename((STRPTR)from, (STRPTR)to))
        return 0;
    {
        LONG err = IoErr();
        Fault(err, NULL, (STRPTR)buf, sizeof(buf));
        Printf((STRPTR)"Can't rename %s as %s because %s\n", (LONG)from, (LONG)to, (LONG)buf);
        SetIoErr(err);
        return RETURN_FAIL;
    }
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

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    STRPTR *from;
    const char *to;
    char pat[520], dest[512];
    LONG rc = 0;
    int count = 0;
    BOOL multi;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    from = (STRPTR *)args[A_FROM];
    to = (char *)args[A_TO];
    while (from[count])
        count++;
    multi = count > 1 || ParsePatternNoCase(from[0], (STRPTR)pat, sizeof(pat)) == 1;

    if (!multi) {
        rc = do_rename((char *)from[0], to);
    } else {
        if (!is_dir(to)) {
            Printf((STRPTR)"Destination \"%s\" is not a directory\n", (LONG)to);
            FreeArgs(rda);
            return RETURN_FAIL;
        }
        for (; *from && !rc; from++) {
            struct AnchorPath *ap = AllocVec(sizeof(struct AnchorPath) + 512, MEMF_PUBLIC | MEMF_CLEAR);
            LONG err;
            if (!ap)
                break;
            ap->ap_Strlen = 512;
            for (err = MatchFirst(*from, ap); !err; err = MatchNext(ap)) {
                strcpy(dest, to);
                AddPart((STRPTR)dest, ap->ap_Info.fib_FileName, sizeof(dest));
                if (!args[A_QUIET])
                    Printf((STRPTR)"%s..", (LONG)ap->ap_Buf);
                rc = do_rename((char *)ap->ap_Buf, dest);
                if (rc)
                    break;
                if (!args[A_QUIET])
                    PutStr((STRPTR)"done\n");
            }
            if (!rc && err != ERROR_NO_MORE_ENTRIES) {
                Fault(err, NULL, (STRPTR)dest, sizeof(dest));
                Printf((STRPTR)"Can't rename %s as %s because %s\n", (LONG)*from, (LONG)to, (LONG)dest);
                rc = RETURN_FAIL;
            }
            MatchEnd(ap);
            FreeVec(ap);
        }
    }
    FreeArgs(rda);
    return rc;
}
