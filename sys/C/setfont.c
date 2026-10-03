/*
 * SETFONT - set the font of the shell window
 *
 * Template: NAME/A,SIZE/N/A,SCREEN/S,WINDOW/S,BOLD/S,ITALIC/S,UNDERLINE/S
 *
 * Opens the font (".font" is added when missing) and sets it in the
 * console window of the shell (found with ACTION_DISK_INFO on the console
 * handler), with SCREEN also in the window's screen.  As AmigaOS 3.1
 * (verified on the reference, Phase 221): a font that cannot be opened
 * gives its fault text, a shell without a console window RC 20 silently.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/graphics.h>
#include <inline/diskfont.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
struct GfxBase *GfxBase;

enum { A_NAME, A_SIZE, A_SCREEN, A_WINDOW, A_BOLD, A_ITALIC, A_UNDERLINE, A_COUNT };

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    struct Library *DiskfontBase;
    struct TextAttr ta;
    struct TextFont *font;
    char name[64];
    struct Process *me = (struct Process *)FindTask(NULL);
    struct Window *win = NULL;
    LONG rc = 0;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)"NAME/A,SIZE/N/A,SCREEN/S,WINDOW/S,BOLD/S,ITALIC/S,UNDERLINE/S", args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    strncpy(name, (char *)args[A_NAME], sizeof(name) - 6);
    name[sizeof(name) - 6] = '\0';
    if (strlen(name) < 5 || stricmp(name + strlen(name) - 5, ".font"))
        strcat(name, ".font");

    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 0);
    ta.ta_Name = (STRPTR)name;
    ta.ta_YSize = *(LONG *)args[A_SIZE];
    ta.ta_Style = (args[A_BOLD] ? FSF_BOLD : 0) | (args[A_ITALIC] ? FSF_ITALIC : 0) |
                  (args[A_UNDERLINE] ? FSF_UNDERLINED : 0);
    ta.ta_Flags = 0;
    font = DiskfontBase ? OpenDiskFont(&ta) : NULL;
    if (!font) {
        LONG err = IoErr();
        if (!err)
            err = ERROR_OBJECT_NOT_FOUND;
        PrintFault(err, NULL);
        SetIoErr(err);
        rc = RETURN_FAIL;
        goto done;
    }

    /* the console window of this shell */
    if (me->pr_ConsoleTask) {
        struct InfoData *id = AllocVec(sizeof(*id), MEMF_PUBLIC | MEMF_CLEAR);
        if (id) {
            if (DoPkt((struct MsgPort *)me->pr_ConsoleTask, ACTION_DISK_INFO, MKBADDR(id), 0, 0, 0, 0))
                win = (struct Window *)id->id_VolumeNode;
            FreeVec(id);
        }
    }
    if (!win || !GfxBase) {
        CloseFont(font);
        SetIoErr(0);
        rc = RETURN_FAIL;
        goto done;
    }
    SetFont(win->RPort, font);
    if (args[A_SCREEN] && win->WScreen)
        SetFont(&win->WScreen->RastPort, font);
    /* the font stays open while the window uses it */

done:
    if (DiskfontBase)
        CloseLibrary(DiskfontBase);
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    FreeArgs(rda);
    return rc;
}
