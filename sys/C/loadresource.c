/*
 * LOADRESOURCE - keep libraries, devices and fonts in memory
 *
 * Template: NAME/M/A,LOCK/S
 *
 * Opens each library (*.library), device (*.device) or font (*.font) and
 * never closes it, so it stays in memory (LOCK: also under low memory -
 * lxa never expunges open libraries anyway).  As AmigaOS 3.1 (verified on
 * the reference, Phase 221): silent on success,
 *   '<name>' - <fault text>                    (RC 20)
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <graphics/text.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/diskfont_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/diskfont.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static BOOL ends_with(const char *s, const char *suffix)
{
    int n = strlen(s), m = strlen(suffix);
    return n >= m && !stricmp(s + n - m, suffix);
}

static BOOL load(const char *name)
{
    const char *file = (const char *)FilePart((STRPTR)name);
    BPTR lock = Lock((STRPTR)name, SHARED_LOCK);

    if (!lock) {
        /* a library or device may also be given by its name only */
        if (!strchr(name, ':') && !strchr(name, '/') && ends_with(name, ".library")) {
            if (OpenLibrary((STRPTR)name, 0))
                return TRUE;
            SetIoErr(ERROR_OBJECT_NOT_FOUND);
        }
        return FALSE;
    }
    UnLock(lock);

    if (ends_with(file, ".library")) {
        /* by path first (a disk library), else by name */
        if (OpenLibrary((STRPTR)name, 0) || OpenLibrary((STRPTR)file, 0))
            return TRUE;
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return FALSE;
    }
    if (ends_with(file, ".device")) {
        struct IORequest *io = AllocVec(sizeof(struct IOStdReq), MEMF_PUBLIC | MEMF_CLEAR);
        if (io && !OpenDevice((STRPTR)file, 0, io, 0))
            return TRUE;    /* kept open */
        if (io)
            FreeVec(io);
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return FALSE;
    }
    if (ends_with(file, ".font")) {
        struct Library *DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 0);
        BOOL ok = FALSE;
        if (DiskfontBase) {
            struct TextAttr ta;
            ta.ta_Name = (STRPTR)file;
            ta.ta_YSize = 8;
            ta.ta_Style = 0;
            ta.ta_Flags = 0;
            ok = OpenDiskFont(&ta) != NULL;     /* kept open */
        }
        if (!ok)
            SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return ok;
    }
    /* anything else: a loadable file kept in memory */
    if (LoadSeg((STRPTR)name))
        return TRUE;
    return FALSE;
}

int main(void)
{
    LONG args[2] = { 0, 0 };
    struct RDArgs *rda;
    STRPTR *n;
    LONG rc = 0;

    rda = ReadArgs((STRPTR)"NAME/M/A,LOCK/S", args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    for (n = (STRPTR *)args[0]; *n; n++) {
        if (!load((char *)*n)) {
            LONG err = IoErr();
            char buf[100];
            Fault(err, NULL, (STRPTR)buf, sizeof(buf));
            Printf((STRPTR)"'%s' - %s\n", (LONG)*n, (LONG)buf);
            SetIoErr(err);
            rc = RETURN_FAIL;
            break;
        }
    }
    FreeArgs(rda);
    return rc;
}
