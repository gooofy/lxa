/*
 * ICONX - run a script from its icon
 *
 * Usage: IconX <script>                       (from a shell)
 *        default tool "C:IconX" of a project  (from Workbench)
 *
 * The script runs as a command file (Execute) in a new shell whose output
 * goes to a console window (tooltype WINDOW, default CON:0/50//80/IconX);
 * the tooltype DELAY keeps the window open for that many seconds.
 * Without a script IconX fails silently with RC 20 (AmigaOS 3.1, verified
 * on the reference, Phase 221).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/icon_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/icon.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
struct Library *IconBase;

extern struct WBStartup *_WBenchMsg;

static LONG run_script(const char *script, const char *window, LONG delay)
{
    char cmd[300];
    BPTR con;
    LONG rc;
    struct TagItem tags[3];

    if (strlen(script) > 250)
        return RETURN_FAIL;
    strcpy(cmd, "Execute \"");
    strcat(cmd, script);
    strcat(cmd, "\"");

    con = window ? Open((STRPTR)window, MODE_NEWFILE) : 0;
    tags[0].ti_Tag = SYS_Input;
    tags[0].ti_Data = (ULONG)(con ? con : Input());
    tags[1].ti_Tag = SYS_Output;
    tags[1].ti_Data = (ULONG)(con ? con : Output());
    tags[2].ti_Tag = TAG_DONE;
    rc = SystemTagList((STRPTR)cmd, tags);
    if (con) {
        if (delay > 0)
            Delay(delay * TICKS_PER_SECOND);
        Close(con);
    }
    return rc < 0 ? RETURN_FAIL : rc;
}

int main(int argc, char **argv)
{
    const char *window = "CON:0/50//80/IconX";
    LONG delay = 2;

    if (argc == 0 && _WBenchMsg) {
        /* from Workbench: the project is the second argument */
        struct WBStartup *wbs = _WBenchMsg;
        char name[256];
        BPTR old;
        LONG rc;
        static char winbuf[128];

        if (wbs->sm_NumArgs < 2)
            return RETURN_FAIL;
        old = CurrentDir(wbs->sm_ArgList[1].wa_Lock);
        strncpy(name, (char *)wbs->sm_ArgList[1].wa_Name, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        IconBase = OpenLibrary((STRPTR)"icon.library", 0);
        if (IconBase) {
            struct DiskObject *dob = GetDiskObject((STRPTR)name);
            if (dob) {
                STRPTR v = FindToolType((CONST_STRPTR *)dob->do_ToolTypes, (STRPTR)"WINDOW");
                if (v) {
                    strncpy(winbuf, (char *)v, sizeof(winbuf) - 1);
                    window = winbuf;
                }
                v = FindToolType((CONST_STRPTR *)dob->do_ToolTypes, (STRPTR)"DELAY");
                if (v)
                    StrToLong(v, &delay);
                FreeDiskObject(dob);
            }
            CloseLibrary(IconBase);
        }
        rc = run_script(name, window, delay);
        CurrentDir(old);
        return rc;
    }

    if (argc < 2) {
        SetIoErr(0);
        return RETURN_FAIL;
    }
    return run_script(argv[1], NULL, 0);
}
