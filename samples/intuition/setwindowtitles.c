/* setwindowtitles.c - SetWindowTitles repaint regression sample */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *IntuitionBase;

int main(void)
{
    struct Window *window;
    struct IntuiMessage *imsg;
    BOOL done = FALSE;

    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
        return 20;

    window = OpenWindowTags(NULL,
                            WA_Title, (ULONG)"Before",
                            WA_ScreenTitle, (ULONG)"ScreenBefore",
                            WA_Left, 40,
                            WA_Top, 30,
                            WA_Width, 260,
                            WA_Height, 90,
                            WA_DragBar, TRUE,
                            WA_DepthGadget, TRUE,
                            WA_CloseGadget, TRUE,
                            WA_Activate, TRUE,
                            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_VANILLAKEY,
                            TAG_END);
    if (!window)
    {
        CloseLibrary(IntuitionBase);
        return 10;
    }

    ShowTitle(window->WScreen, TRUE);

    printf("READY: before\n");

    while (!done)
    {
        Wait(1L << window->UserPort->mp_SigBit);
        while ((imsg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
        {
            ULONG cls = imsg->Class;
            UWORD code = imsg->Code;

            ReplyMsg((struct Message *)imsg);
            if (cls == IDCMP_CLOSEWINDOW)
                done = TRUE;
            else if (cls == IDCMP_VANILLAKEY && code == 't')
            {
                SetWindowTitles(window, "After", "ScreenAfter");
                printf("READY: after\n");
            }
        }
    }

    CloseWindow(window);
    CloseLibrary(IntuitionBase);
    return 0;
}
