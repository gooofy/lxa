/*
 * FILENOTE - set the comment of a file
 *
 * Template: FILE/A,COMMENT,ALL/S,QUIET/S
 *
 * An empty (or missing) comment removes it.  A pattern or ALL processes
 * several files, reporting each ("name..done") unless QUIET.  As AmigaOS
 * 3.1 (verified on the reference, Phase 221): silent for a single file,
 * the fault text and RC 20 on failure; comments longer than 79 characters
 * fail with "comment is too long".
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

#define TEMPLATE "FILE/A,COMMENT,ALL/S,QUIET/S"

enum { A_FILE, A_COMMENT, A_ALL, A_QUIET, A_COUNT };

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    const char *comment;
    char pat[520];
    LONG rc = 0;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    comment = args[A_COMMENT] ? (char *)args[A_COMMENT] : "";
    if (strlen(comment) > 79) {
        PrintFault(ERROR_COMMENT_TOO_BIG, NULL);
        FreeArgs(rda);
        SetIoErr(ERROR_COMMENT_TOO_BIG);
        return RETURN_FAIL;
    }

    if (ParsePatternNoCase((STRPTR)args[A_FILE], (STRPTR)pat, sizeof(pat)) == 1 || args[A_ALL]) {
        struct AnchorPath *ap = AllocVec(sizeof(struct AnchorPath) + 512, MEMF_PUBLIC | MEMF_CLEAR);
        LONG err;
        if (!ap) {
            FreeArgs(rda);
            return RETURN_FAIL;
        }
        ap->ap_Strlen = 512;
        ap->ap_BreakBits = SIGBREAKF_CTRL_C;
        for (err = MatchFirst((STRPTR)args[A_FILE], ap); !err; err = MatchNext(ap)) {
            if (ap->ap_Info.fib_DirEntryType >= 0) {
                if (ap->ap_Flags & APF_DIDDIR) {
                    ap->ap_Flags &= ~APF_DIDDIR;
                    continue;
                }
                if (args[A_ALL])
                    ap->ap_Flags |= APF_DODIR;
            }
            if (!SetComment((STRPTR)ap->ap_Buf, (STRPTR)comment)) {
                err = IoErr();
                Printf((STRPTR)"%s..", (LONG)ap->ap_Buf);
                PrintFault(err, NULL);
                rc = RETURN_FAIL;
                break;
            }
            if (!args[A_QUIET])
                Printf((STRPTR)"%s..done\n", (LONG)ap->ap_Buf);
        }
        if (!rc && err != ERROR_NO_MORE_ENTRIES) {
            if (err == ERROR_BREAK)
                PutStr((STRPTR)"***Break\n");
            else
                PrintFault(err, NULL);
            SetIoErr(err);
            rc = err == ERROR_BREAK ? RETURN_WARN : RETURN_FAIL;
        }
        MatchEnd(ap);
        FreeVec(ap);
    } else if (!SetComment((STRPTR)args[A_FILE], (STRPTR)comment)) {
        LONG err = IoErr();
        PrintFault(err, NULL);
        SetIoErr(err);
        rc = RETURN_FAIL;
    }
    FreeArgs(rda);
    return rc;
}
