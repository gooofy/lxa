/*
 * Probe (Phase 237e): OpenWindowTagList() with WA_PubScreenName NULL.
 * Fred Fish JEd (disk 776) passes WA_PubScreenName with a NULL name when
 * its configuration names no screen.  Prints whether the window opens and
 * on which screen.
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct IntuitionBase *IntuitionBase;

int main(void)
{
    struct Screen *def;
    struct Window *w;
    struct TagItem tags[] = {
        { WA_Left, 0 }, { WA_Top, 20 }, { WA_Width, 200 }, { WA_Height, 60 },
        { WA_Title, (ULONG)"probe pubnull" },
        { WA_PubScreenName, 0 },
        { TAG_DONE, 0 }
    };

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (!IntuitionBase)
        return 20;
    def = LockPubScreen(NULL);
    w = OpenWindowTagList(NULL, tags);
    P_BOOL("window opened", w != NULL);
    if (w) {
        P_BOOL("on the default public screen", w->WScreen == def);
        CloseWindow(w);
    }
    if (def)
        UnlockPubScreen(NULL, def);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
