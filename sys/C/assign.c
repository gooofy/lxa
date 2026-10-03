/*
 * ASSIGN - create, remove and list logical device names
 *
 * Template: NAME,TARGET/M,LIST/S,EXISTS/S,DISMOUNT/S,DEFER/S,PATH/S,
 *           ADD/S,REMOVE/S,VOLS/S,DIRS/S,DEVICES/S
 *
 * Messages and layout of AmigaOS 3.1's Assign (verified on the reference,
 * Phase 221):
 *   NAME:                       remove the assign (silent)
 *   NAME: dir [dir...]          assign (several directories: multi-assign)
 *   NAME: dir ADD / REMOVE      add a directory to / remove it from it
 *   NAME: path DEFER / PATH     late-binding / non-binding assign
 *   NAME: EXISTS                "NAME           dir" ("+ dir" for more),
 *                               "<path>" late, "[path]" non-binding;
 *                               "NAME: not assigned" (RC 5)
 *   Can't find <dir>            (RC 20)
 *   Invalid device name <name>  (RC 20)
 * Without a name (or with LIST) the volumes, directories and devices are
 * listed (VOLS, DIRS, DEVICES select).
 *
 * The assign table lives in lxa's host VFS; EMU_CALL_DOS_ASSIGN_INFO and
 * EMU_CALL_DOS_ASSIGN_LIST read it (lxa's DosList is not complete yet,
 * Phase 255).
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

#include "emucalls.h"

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "NAME,TARGET/M,LIST/S,EXISTS/S,DISMOUNT/S,DEFER/S,PATH/S,ADD/S,REMOVE/S,VOLS/S,DIRS/S,DEVICES/S"

enum { A_NAME, A_TARGET, A_LIST, A_EXISTS, A_DISMOUNT, A_DEFER, A_PATH, A_ADD, A_REMOVE,
       A_VOLS, A_DIRS, A_DEVICES, A_COUNT };

static ULONG emucall3(ULONG func, ULONG p1, ULONG p2, ULONG p3)
{
    register ULONG d0 __asm("d0") = func;
    register ULONG d1 __asm("d1") = p1;
    register ULONG d2 __asm("d2") = p2;
    register ULONG d3 __asm("d3") = p3;

    __asm volatile ("illegal" : "+r" (d0) : "r" (d1), "r" (d2), "r" (d3) : "cc", "memory");
    return d0;
}

static char info[2048];

/* print one assign as "NAME           dir" (+ further dirs); FALSE if none */
static BOOL show_assign(const char *name)
{
    ULONG type = emucall3(EMU_CALL_DOS_ASSIGN_INFO, (ULONG)name, (ULONG)info, sizeof(info));
    const char *p = info;
    BOOL first = TRUE;

    if (!type)
        return FALSE;
    Printf((STRPTR)"%-15s", (LONG)name);
    while (*p) {
        if (!first)
            PutStr((STRPTR)"             + ");
        if (type == 2)
            Printf((STRPTR)"<%s>\n", (LONG)p);
        else if (type == 3)
            Printf((STRPTR)"[%s]\n", (LONG)p);
        else
            Printf((STRPTR)"%s\n", (LONG)p);
        first = FALSE;
        p += strlen(p) + 1;
    }
    if (first)
        PutStr((STRPTR)"\n");
    return TRUE;
}

static void list_all(BOOL vols, BOOL dirs, BOOL devs)
{
    static char buf[4096];
    if (vols) {
        PutStr((STRPTR)"Volumes:\n");
        PutStr((STRPTR)"SYS [Mounted]\n");
        PutStr((STRPTR)"\n");
    }
    if (dirs) {
        ULONG count = emucall3(EMU_CALL_DOS_ASSIGN_LIST, (ULONG)buf, sizeof(buf), 0);
        const char *p = buf;
        ULONG i;
        PutStr((STRPTR)"Directories:\n");
        for (i = 0; i < count && *p; i++) {
            const char *name = p;
            p += strlen(p) + 1;
            p += strlen(p) + 1;
            if (!stricmp(name, "SYS"))
                continue;
            show_assign(name);
        }
        PutStr((STRPTR)"\n");
    }
    if (devs) {
        PutStr((STRPTR)"Devices:\n");
        PutStr((STRPTR)"RAM\n");
    }
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    char name[64];
    STRPTR *t;
    LONG rc = 0;
    int n;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }

    if ((args[A_ADD] != 0) + (args[A_REMOVE] != 0) + (args[A_PATH] != 0) + (args[A_DEFER] != 0) > 1) {
        PutStr((STRPTR)"Only one of ADD, SUB, PATH, or DEFER allowed\n");
        FreeArgs(rda);
        SetIoErr(0);
        return RETURN_FAIL;
    }

    if (!args[A_NAME] || args[A_LIST]) {
        BOOL any = args[A_VOLS] || args[A_DIRS] || args[A_DEVICES];
        if (!args[A_NAME])
            list_all(!any || args[A_VOLS], !any || args[A_DIRS], !any || args[A_DEVICES]);
        if (!args[A_NAME]) {
            FreeArgs(rda);
            return 0;
        }
    }

    n = strlen((char *)args[A_NAME]);
    if (n < 2 || ((char *)args[A_NAME])[n - 1] != ':' || n > (int)sizeof(name)) {
        Printf((STRPTR)"Invalid device name %s\n", args[A_NAME]);
        FreeArgs(rda);
        SetIoErr(0);
        return RETURN_FAIL;
    }
    CopyMem((APTR)args[A_NAME], name, n - 1);
    name[n - 1] = '\0';

    if (args[A_EXISTS] || args[A_LIST]) {
        if (!show_assign(name)) {
            Printf((STRPTR)"%s not assigned\n", args[A_NAME]);
            rc = RETURN_WARN;
        }
        FreeArgs(rda);
        SetIoErr(0);
        return rc;
    }

    t = (STRPTR *)args[A_TARGET];
    if (!t || !*t) {
        if (args[A_DISMOUNT]) {
            /* the device itself cannot be removed from lxa's VFS */
            AssignLock((STRPTR)name, 0);
        } else {
            AssignLock((STRPTR)name, 0);
        }
        FreeArgs(rda);
        return 0;
    }

    if (args[A_DEFER] || args[A_PATH]) {
        BOOL ok;
        BPTR lock = Lock(*t, SHARED_LOCK);
        if (args[A_PATH] && !lock) {
            LONG err = IoErr();
            Printf((STRPTR)"Can't find %s\n", (LONG)*t);
            FreeArgs(rda);
            SetIoErr(err);
            return RETURN_FAIL;
        }
        if (lock)
            UnLock(lock);
        ok = args[A_DEFER] ? AssignLate((STRPTR)name, *t) : AssignPath((STRPTR)name, *t);
        if (!ok) {
            LONG err = IoErr();
            Printf((STRPTR)"Can't find %s\n", (LONG)*t);
            SetIoErr(err);
            rc = RETURN_FAIL;
        }
        FreeArgs(rda);
        return rc;
    }

    {
        BOOL first = TRUE;
        for (; *t; t++) {
            BPTR lock = Lock(*t, SHARED_LOCK);
            if (!lock) {
                LONG err = IoErr();
                Printf((STRPTR)"Can't find %s\n", (LONG)*t);
                SetIoErr(err);
                rc = RETURN_FAIL;
                break;
            }
            if (args[A_REMOVE]) {
                if (!RemAssignList((STRPTR)name, lock)) {
                    Printf((STRPTR)"Can't cancel %s\n", (LONG)*t);
                    rc = RETURN_WARN;
                }
                UnLock(lock);
            } else if (args[A_ADD] || !first) {
                if (!AssignAdd((STRPTR)name, lock)) {
                    if (!AssignLock((STRPTR)name, lock)) {
                        UnLock(lock);
                        PrintFault(IoErr(), (STRPTR)args[A_NAME]);
                        rc = RETURN_FAIL;
                        break;
                    }
                }
            } else {
                if (!AssignLock((STRPTR)name, lock)) {
                    UnLock(lock);
                    PrintFault(IoErr(), (STRPTR)args[A_NAME]);
                    rc = RETURN_FAIL;
                    break;
                }
            }
            first = FALSE;
        }
    }
    FreeArgs(rda);
    return rc;
}
