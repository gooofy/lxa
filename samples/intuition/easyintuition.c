/* EasyIntuition.c - Adapted from RKM sample
 * Demonstrates opening a custom screen and window using tags
 * Modified for automated testing (no event loop)
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/displayinfo.h>
#include <libraries/dos.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *IntuitionBase = NULL;

/* Position and sizes for our window */
#define WIN_LEFTEDGE   20
#define WIN_TOPEDGE    20
#define WIN_WIDTH     400
#define WIN_MINWIDTH   80
#define WIN_HEIGHT    150
#define WIN_MINHEIGHT  20

int main(int argc, char *argv[])
{
    UWORD pens[] = {~0};  /* Terminator for SA_Pens */

    struct Screen *screen1 = NULL;
    struct Window *window1 = NULL;

    printf("EasyIntuition: Demonstrating custom Screen and Window creation\n\n");

    /* Open the Intuition Library */
    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (IntuitionBase == NULL)
    {
        printf("EasyIntuition: ERROR - Can't open intuition.library v37\n");
        return RETURN_FAIL;
    }
    printf("EasyIntuition: Opened intuition.library v%d\n", IntuitionBase->lib_Version);

    /* Open a custom screen */
    printf("\nEasyIntuition: Opening custom screen...\n");
    screen1 = OpenScreenTags(NULL,
                             SA_Pens,     (ULONG)pens,
                             SA_Depth,    2,
                             SA_Width,    640,
                             SA_Height,   200,
                             SA_Title,    (ULONG)"Custom EasyIntuition Screen",
                             TAG_DONE);

    if (screen1 == NULL)
    {
        printf("EasyIntuition: ERROR - Can't open custom screen\n");
        CloseLibrary(IntuitionBase);
        return RETURN_FAIL;
    }
    printf("EasyIntuition: Screen opened successfully!\n");
    printf("  Address: 0x%08lx\n", (ULONG)screen1);
    printf("  Size: %dx%d, Depth: %d\n", screen1->Width, screen1->Height, screen1->BitMap.Depth);
    printf("  Title: %s\n", screen1->Title ? (char *)screen1->Title : "(null)");

    /* Open a window on our custom screen */
    printf("\nEasyIntuition: Opening window on custom screen...\n");
    window1 = OpenWindowTags(NULL,
                             /* Specify window dimensions and limits */
                             WA_Left,         WIN_LEFTEDGE,
                             WA_Top,          WIN_TOPEDGE,
                             WA_Width,        WIN_WIDTH,
                             WA_Height,       WIN_HEIGHT,
                             WA_MinWidth,     WIN_MINWIDTH,
                             WA_MinHeight,    WIN_MINHEIGHT,
                             WA_MaxWidth,     ~0,
                             WA_MaxHeight,    ~0,
                             /* Specify the system gadgets we want */
                             WA_CloseGadget,  TRUE,
                             WA_SizeGadget,   TRUE,
                             WA_DepthGadget,  TRUE,
                             WA_DragBar,      TRUE,
                             /* Specify other attributes */
                             WA_Activate,     TRUE,
                             WA_NoCareRefresh,TRUE,
                             /* Specify the events we want to know about */
                             WA_IDCMP,        IDCMP_CLOSEWINDOW,
                             /* Attach the window to our custom screen */
                             WA_CustomScreen, screen1,
                             WA_Title,        (ULONG)"EasyWindow",
                             WA_ScreenTitle,  (ULONG)"Custom Screen - EasyWindow is Active",
                             TAG_DONE);

    if (window1 == NULL)
    {
        printf("EasyIntuition: ERROR - Can't open window\n");
        CloseScreen(screen1);
        CloseLibrary(IntuitionBase);
        return RETURN_FAIL;
    }
    printf("EasyIntuition: Window opened successfully!\n");
    printf("  Address: 0x%08lx\n", (ULONG)window1);
    printf("  Position: %d,%d\n", window1->LeftEdge, window1->TopEdge);
    printf("  Size: %dx%d\n", window1->Width, window1->Height);
    printf("  Title: %s\n", window1->Title ? (char *)window1->Title : "(null)");

    /* Verify screen attachment */
    if (window1->WScreen == screen1)
    {
        printf("  Attached to: Custom screen (correct)\n");
    }
    else
    {
        printf("  WARNING: Window attached to wrong screen!\n");
    }

    /* Verify signal bit setup */
    printf("\nEasyIntuition: Checking IDCMP setup...\n");
    printf("  UserPort: 0x%08lx\n", (ULONG)window1->UserPort);
    if (window1->UserPort)
    {
        printf("  Signal Bit: %d\n", window1->UserPort->mp_SigBit);
        printf("  Signal Mask: 0x%08lx\n", 1UL << window1->UserPort->mp_SigBit);
    }

    /* Draw something to show it works */
    printf("\nEasyIntuition: Drawing test pattern in window...\n");
    SetAPen(window1->RPort, 1);
    RectFill(window1->RPort, 30, 40, 370, 130);
    SetAPen(window1->RPort, 2);
    RectFill(window1->RPort, 50, 60, 350, 110);

    printf("EasyIntuition: Test pattern drawn\n");

    /* Clean up */
    printf("\nEasyIntuition: Cleanup - Closing window...\n");
    CloseWindow(window1);

    printf("EasyIntuition: Closing screen...\n");
    CloseScreen(screen1);

    printf("EasyIntuition: Closing library...\n");
    CloseLibrary(IntuitionBase);

    printf("\nEasyIntuition: Demo complete.\n");
    return RETURN_OK;
}
