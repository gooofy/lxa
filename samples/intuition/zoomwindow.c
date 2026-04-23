/* zoomwindow.c - Opens a window with WA_Zoom (zoom/zip gadget) */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <stdio.h>

struct Library *IntuitionBase;

int main(int argc, char **argv)
{
    struct Window *win;
    LONG zoomCoords[4] = { 20, 20, 200, 50 };  /* zoomed-out size */

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    if (!IntuitionBase) {
        printf("Failed to open intuition.library\n");
        return 1;
    }

    win = OpenWindowTags(NULL,
        WA_Left,        20,
        WA_Top,         20,
        WA_Width,       400,
        WA_Height,      150,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_Zoom,        (ULONG)zoomCoords,
        WA_IDCMP,       IDCMP_CLOSEWINDOW,
        WA_Title,       (ULONG)"ZoomWindow Test",
        TAG_DONE);

    if (win) {
        printf("ZoomWindow opened successfully.\n");
        /* Wait for close gadget click */
        struct IntuiMessage *msg;
        ULONG sigs = (1L << win->UserPort->mp_SigBit);
        for (;;) {
            Wait(sigs);
            while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL) {
                ULONG cls = msg->Class;
                ReplyMsg((struct Message *)msg);
                if (cls == IDCMP_CLOSEWINDOW) goto done;
            }
        }
done:
        CloseWindow(win);
        printf("ZoomWindow closed.\n");
    } else {
        printf("Failed to open ZoomWindow.\n");
    }

    CloseLibrary(IntuitionBase);
    return 0;
}
