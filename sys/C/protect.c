/*
 * PROTECT - change protection bits
 *
 * Template: FILE/A,FLAGS,ADD/S,SUB/S,ALL/S,QUIET/S
 *
 * FLAGS is a combination of h s p a r w e d: alone it sets exactly these
 * bits, "+flags" or ADD adds them, "-flags" or SUB removes them.  A
 * pattern or ALL processes several files and reports each one
 * ("name..done") unless QUIET.  Behaviour of AmigaOS 3.1 (verified on the
 * reference, Phase 221): silent for a single file, the fault text and
 * RC 20 on failure.
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

#define TEMPLATE "FILE/A,FLAGS,ADD/S,SUB/S,ALL/S,QUIET/S"

enum { A_FILE, A_FLAGS, A_ADD, A_SUB, A_ALL, A_QUIET, A_COUNT };

/* the "on" bits of a flag string (in display form: set = allowed for rwed) */
static BOOL parse_flags(const char *s, LONG *bits)
{
    static const char flags[] = "hsparwed";
    *bits = 0;
    for (; *s; s++) {
        const char *f = strchr(flags, *s | 0x20);
        if (!f || !*s)
            return FALSE;
        *bits |= 1L << (7 - (f - flags));
    }
    return TRUE;
}

/* display form (bit set = flag shown) <-> FIB form (rwed inverted) */
static LONG to_fib(LONG shown)
{
    return shown ^ 0x0F;
}

static LONG new_protection(LONG old, LONG mode, LONG bits)
{
    LONG shown = (old ^ 0x0F) & 0xFF;
    if (mode > 0)
        shown |= bits;
    else if (mode < 0)
        shown &= ~bits;
    else
        shown = bits;
    return (old & ~0xFF) | (to_fib(shown) & 0xFF);
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    const char *fl;
    LONG mode = 0, bits = 0;
    char pat[520];
    LONG rc = 0;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    fl = args[A_FLAGS] ? (char *)args[A_FLAGS] : "";
    if (*fl == '+') {
        mode = 1;
        fl++;
    } else if (*fl == '-') {
        mode = -1;
        fl++;
    }
    if (args[A_ADD])
        mode = 1;
    if (args[A_SUB])
        mode = -1;
    if (!parse_flags(fl, &bits)) {
        PrintFault(ERROR_BAD_TEMPLATE, NULL);
        FreeArgs(rda);
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
            if (!SetProtection((STRPTR)ap->ap_Buf,
                               new_protection(ap->ap_Info.fib_Protection, mode, bits))) {
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
    } else {
        BPTR lock = Lock((STRPTR)args[A_FILE], SHARED_LOCK);
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        if (!lock || !fib || !Examine(lock, fib) ||
            (UnLock(lock), lock = 0,
             !SetProtection((STRPTR)args[A_FILE], new_protection(fib->fib_Protection, mode, bits)))) {
            LONG err = IoErr();
            PrintFault(err, NULL);
            SetIoErr(err);
            rc = RETURN_FAIL;
        }
        if (lock)
            UnLock(lock);
        if (fib)
            FreeDosObject(DOS_FIB, fib);
    }
    FreeArgs(rda);
    return rc;
}
