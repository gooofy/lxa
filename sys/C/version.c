/*
 * VERSION - show version information
 *
 * Template: NAME,VERSION/N,REVISION/N,FILE/S,FULL/S,RES/S
 *
 * As AmigaOS 3.1 (verified on the reference, Phase 221):
 *   Version                 Kickstart 40.70, Workbench 40.42
 *                           (exec version.SoftVer, version.library)
 *   Version exec.library    exec.library 40.10
 *   Version <file>          from the file's $VER string: "list 37.5"
 *   FULL                    adds the date: "(07/15/93)"
 *   VERSION n [REVISION m]  RC 5 if the version found is older
 *   unknown library         <fault text> (RC 20)
 *   file without $VER       Could not find version information for '<f>'
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "NAME,VERSION/N,REVISION/N,FILE/S,FULL/S,RES/S"

enum { A_NAME, A_VERSION, A_REVISION, A_FILE, A_FULL, A_RES, A_COUNT };

static LONG args[A_COUNT];

/* the "(date)" part of an id string, or "" */
static const char *date_of(const char *id)
{
    const char *p = id ? strchr(id, '(') : NULL;
    static char buf[32];
    int n = 0;

    if (!p)
        return "";
    while (*p && *p != ')' && n < (int)sizeof(buf) - 2)
        buf[n++] = *p++;
    if (*p == ')')
        buf[n++] = ')';
    buf[n] = '\0';
    return buf;
}

static LONG check(LONG version, LONG revision)
{
    if (args[A_VERSION]) {
        LONG want = *(LONG *)args[A_VERSION];
        LONG wrev = args[A_REVISION] ? *(LONG *)args[A_REVISION] : 0;
        if (version < want || (version == want && revision < wrev)) {
            SetIoErr(0);
            return RETURN_WARN;
        }
    }
    return 0;
}

static void print_version(const char *name, LONG version, LONG revision, const char *id)
{
    Printf((STRPTR)"%s %ld.%ld", (LONG)name, version, revision);
    if (args[A_FULL] && *date_of(id))
        Printf((STRPTR)" %s", (LONG)date_of(id));
    PutStr((STRPTR)"\n");
}

/* "$VER: name 37.5 (11/08/91)" in a file */
static BOOL file_version(const char *file, LONG *rc)
{
    BPTR fh = Open((STRPTR)file, MODE_OLDFILE);
    static char buf[1024], ver[128];
    LONG n, state = 0, k = 0;
    BOOL found = FALSE;

    if (!fh)
        return FALSE;
    while (!found && (n = Read(fh, buf, sizeof(buf))) > 0) {
        LONG i;
        for (i = 0; i < n && !found; i++) {
            char c = buf[i];
            if (state < 5) {
                state = (c == "$VER:"[state]) ? state + 1 : (c == '$' ? 1 : 0);
                continue;
            }
            if (k == 0 && c == ' ')
                continue;
            if (c == '\0' || c == '\n' || c == '\r' || k >= (LONG)sizeof(ver) - 1)
                found = TRUE;
            else
                ver[k++] = c;
        }
    }
    Close(fh);
    if (!found && state == 5 && k)
        found = TRUE;
    if (!found)
        return FALSE;
    ver[k] = '\0';
    {
        /* name, version.revision, rest */
        char name[64];
        const char *p = ver;
        LONG v = 0, r = 0;
        int j = 0;
        while (*p && *p != ' ' && j < 63)
            name[j++] = *p++;
        name[j] = '\0';
        while (*p == ' ')
            p++;
        while (*p >= '0' && *p <= '9')
            v = v * 10 + (*p++ - '0');
        if (*p == '.') {
            p++;
            while (*p >= '0' && *p <= '9')
                r = r * 10 + (*p++ - '0');
        }
        print_version(name, v, r, p);
        *rc = check(v, r);
    }
    return TRUE;
}

int main(void)
{
    struct RDArgs *rda;
    const char *name;
    LONG rc = 0;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    name = (char *)args[A_NAME];

    if (!name) {
        struct Library *vl = OpenLibrary((STRPTR)"version.library", 0);
        Printf((STRPTR)"Kickstart %ld.%ld", (LONG)SysBase->LibNode.lib_Version, (LONG)SysBase->SoftVer);
        if (vl) {
            Printf((STRPTR)", Workbench %ld.%ld", (LONG)vl->lib_Version, (LONG)vl->lib_Revision);
            if (args[A_FULL] && *date_of((char *)vl->lib_IdString))
                Printf((STRPTR)" %s", (LONG)date_of((char *)vl->lib_IdString));
            CloseLibrary(vl);
        }
        PutStr((STRPTR)"\n");
        rc = check(SysBase->LibNode.lib_Version, SysBase->SoftVer);
        FreeArgs(rda);
        return rc;
    }

    if (!args[A_FILE] && !strchr(name, ':') && !strchr(name, '/')) {
        struct Library *lib = OpenLibrary((STRPTR)name, 0);
        if (lib) {
            print_version(name, lib->lib_Version, lib->lib_Revision, (char *)lib->lib_IdString);
            rc = check(lib->lib_Version, lib->lib_Revision);
            CloseLibrary(lib);
            FreeArgs(rda);
            return rc;
        }
        Forbid();
        lib = (struct Library *)FindName(&SysBase->LibList, (STRPTR)name);
        if (!lib && !stricmp(name, "exec.library"))
            lib = &SysBase->LibNode;
        if (!lib)
            lib = (struct Library *)FindName(&SysBase->DeviceList, (STRPTR)name);
        if (!lib)
            lib = (struct Library *)FindName(&SysBase->ResourceList, (STRPTR)name);
        if (lib) {
            LONG v = lib->lib_Version, r = lib->lib_Revision;
            const char *id = (char *)lib->lib_IdString;
            Permit();
            print_version(name, v, r, id);
            FreeArgs(rda);
            return check(v, r);
        }
        Permit();
    }

    /* a file */
    {
        BPTR lock = Lock((STRPTR)name, SHARED_LOCK);
        if (!lock) {
            LONG err = IoErr();
            PrintFault(err, NULL);
            FreeArgs(rda);
            SetIoErr(err);
            return RETURN_FAIL;
        }
        UnLock(lock);
        if (!file_version(name, &rc)) {
            Printf((STRPTR)"Could not find version information for '%s'\n", (LONG)name);
            FreeArgs(rda);
            SetIoErr(0);
            return RETURN_FAIL;
        }
    }
    FreeArgs(rda);
    return rc;
}
