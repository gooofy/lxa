/* OpenWindowTags.c - Adapted from RKM sample */
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

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    if (IntuitionBase) {
        win = OpenWindowTags(NULL,
            WA_Left,   20,
            WA_Top,    20,
            WA_Width,  300,
            WA_Height, 100,
            WA_CloseGadget, TRUE,
            WA_IDCMP, IDCMP_CLOSEWINDOW,
            WA_Title, "Lxa Sample Window",
            TAG_DONE);

        if (win) {
            printf("Window opened successfully.\n");
            /* Wait 2 seconds then close */
            Delay(100);
            CloseWindow(win);
            printf("Window closed.\n");
        } else {
            printf("Failed to open window.\n");
        }
        CloseLibrary(IntuitionBase);
    } else {
        printf("Failed to open intuition.library\n");
    }
    return 0;
}
