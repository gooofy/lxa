/*
 * EXECUTE command - run an AmigaDOS script (command file)
 *
 * Template: FILE/A,ARGS/F
 *
 * The script is interpreted by the lxa shell (SYS:System/Shell), which
 * provides the command-file-only commands (IF/ELSE/ENDIF, SKIP/LAB, ...).
 * Like AmigaOS, the script inherits the caller's input and output.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "FILE/A,ARGS/F"

static const char *shell_path(void)
{
    static const char *candidates[] = { "SYS:System/Shell", "System:Shell", NULL };
    int i;

    for (i = 0; candidates[i]; i++) {
        BPTR lock = Lock((STRPTR)candidates[i], SHARED_LOCK);
        if (lock) {
            UnLock(lock);
            return candidates[i];
        }
    }
    return NULL;
}

int main(void)
{
    LONG args[2] = { 0, 0 };
    struct RDArgs *rda;
    const char *shell;
    char cmd[512];
    BPTR lock;
    LONG rc;
    struct TagItem tags[3];

    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), (STRPTR)"EXECUTE");
        return RETURN_FAIL;
    }

    lock = Lock((STRPTR)args[0], SHARED_LOCK);
    if (!lock) {
        PrintFault(IoErr(), (STRPTR)"EXECUTE");
        FreeArgs(rda);
        return RETURN_FAIL;
    }
    UnLock(lock);

    shell = shell_path();
    if (!shell || strlen(shell) + strlen((char *)args[0]) + 4 > sizeof(cmd)) {
        PrintFault(ERROR_OBJECT_NOT_FOUND, (STRPTR)"EXECUTE");
        FreeArgs(rda);
        return RETURN_FAIL;
    }

    strcpy(cmd, shell);
    strcat(cmd, " ");
    strcat(cmd, (char *)args[0]);

    tags[0].ti_Tag = SYS_Input;
    tags[0].ti_Data = (ULONG)Input();
    tags[1].ti_Tag = SYS_Output;
    tags[1].ti_Data = (ULONG)Output();
    tags[2].ti_Tag = TAG_DONE;
    tags[2].ti_Data = 0;

    rc = SystemTagList((STRPTR)cmd, tags);

    FreeArgs(rda);
    return rc < 0 ? RETURN_FAIL : rc;
}
