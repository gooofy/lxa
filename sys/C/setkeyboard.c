/*
 * SETKEYBOARD - set the default keymap
 *
 * Template: KEYMAP/A
 *
 * The keymap comes from keymap.resource (where the ROM's "usa" lives and
 * loaded keymaps stay) or from DEVS:Keymaps/<name>, a loadable file whose
 * first hunk starts with its KeyMapNode.  It becomes the default of
 * keymap.library (SetKeyMapDefault) and console.device
 * (CD_SETDEFAULTKEYMAP).  As AmigaOS 3.1 (verified on the reference,
 * Phase 221): silent on success,
 *   ERROR: can't load requested keymap           (RC 20)
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/keymap.h>
#include <devices/console.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/keymap_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/keymap.h>

#include <string.h>

#ifndef CONU_LIBRARY
#define CONU_LIBRARY (-1)
#endif

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
struct Library *KeymapBase;

static struct KeyMapNode *find_loaded(const char *name)
{
    struct KeyMapResource *kr = (struct KeyMapResource *)OpenResource((STRPTR)"keymap.resource");
    struct Node *n;

    if (!kr)
        return NULL;
    Forbid();
    for (n = kr->kr_List.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (n->ln_Name && !stricmp(n->ln_Name, name)) {
            Permit();
            return (struct KeyMapNode *)n;
        }
    Permit();
    return NULL;
}

static struct KeyMapNode *load(const char *name)
{
    char path[128];
    BPTR seg;
    struct KeyMapNode *kmn;
    struct KeyMapResource *kr;

    strcpy(path, "DEVS:Keymaps/");
    strncat(path, name, sizeof(path) - strlen(path) - 1);
    seg = LoadSeg((STRPTR)path);
    if (!seg)
        return NULL;
    kmn = (struct KeyMapNode *)((UBYTE *)BADDR(seg) + sizeof(BPTR));
    if (!kmn->kn_Node.ln_Name || stricmp(kmn->kn_Node.ln_Name, name)) {
        UnLoadSeg(seg);
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return NULL;
    }
    /* the keymap stays loaded, listed in keymap.resource */
    kr = (struct KeyMapResource *)OpenResource((STRPTR)"keymap.resource");
    if (kr) {
        Forbid();
        AddTail(&kr->kr_List, &kmn->kn_Node);
        Permit();
    }
    return kmn;
}

int main(void)
{
    LONG args[1] = { 0 };
    struct RDArgs *rda;
    struct KeyMapNode *kmn;
    struct KeyMap *km = NULL;

    rda = ReadArgs((STRPTR)"KEYMAP/A", args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    KeymapBase = OpenLibrary((STRPTR)"keymap.library", 0);
    kmn = find_loaded((char *)args[0]);
    if (kmn)
        km = &kmn->kn_KeyMap;
    else if (!stricmp((char *)args[0], "usa") && KeymapBase)
        km = AskKeyMapDefault();    /* the ROM keymap */
    else if ((kmn = load((char *)args[0])))
        km = &kmn->kn_KeyMap;

    if (!km) {
        PutStr((STRPTR)"ERROR: can't load requested keymap\n");
        if (KeymapBase)
            CloseLibrary(KeymapBase);
        FreeArgs(rda);
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return RETURN_FAIL;
    }

    if (KeymapBase) {
        SetKeyMapDefault(km);
        CloseLibrary(KeymapBase);
    }
    {
        struct MsgPort *port = CreateMsgPort();
        struct IOStdReq *io = port ? (struct IOStdReq *)CreateIORequest(port, sizeof(*io)) : NULL;
        if (io && !OpenDevice((STRPTR)"console.device", CONU_LIBRARY, (struct IORequest *)io, 0)) {
            io->io_Command = CD_SETDEFAULTKEYMAP;
            io->io_Data = km;
            io->io_Length = sizeof(struct KeyMap);
            DoIO((struct IORequest *)io);
            CloseDevice((struct IORequest *)io);
        }
        if (io)
            DeleteIORequest((struct IORequest *)io);
        if (port)
            DeleteMsgPort(port);
    }
    FreeArgs(rda);
    return 0;
}
