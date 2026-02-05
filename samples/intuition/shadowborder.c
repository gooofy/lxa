/* ShadowBorder.c - Adapted from RKM sample
 * Demonstrates the use of Intuition Borders with shadow/shine effect
 */

#include <exec/types.h>
#include <intuition/intuition.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *IntuitionBase = NULL;

#define MYBORDER_LEFT   (0)
#define MYBORDER_TOP    (0)

/* This is the border data - a rectangle of 50x30 pixels */
WORD myBorderData[] =
    {
    0,0, 50,0, 50,30, 0,30, 0,0,
    };

int main(int argc, char **argv)
{
    struct Screen   *screen;
    struct DrawInfo *drawinfo;
    struct Window   *win;
    struct Border    shineBorder;
    struct Border    shadowBorder;

    ULONG mySHADOWPEN = 1;  /* set default values for pens */
    ULONG mySHINEPEN  = 2;  /* in case can't get info...   */

    printf("ShadowBorder: Demonstrating Intuition Border rendering\n\n");

    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (IntuitionBase == NULL)
    {
        printf("ShadowBorder: ERROR - Can't open intuition.library v37\n");
        return RETURN_FAIL;
    }
    printf("ShadowBorder: Opened intuition.library v%d\n", IntuitionBase->lib_Version);

    /* Lock the default public screen (Workbench) to get drawing info */
    screen = LockPubScreen(NULL);
    if (screen != NULL)
    {
        printf("ShadowBorder: Locked public screen at 0x%08lx\n", (ULONG)screen);
        
        drawinfo = GetScreenDrawInfo(screen);
        if (drawinfo != NULL)
        {
            /* Get a copy of the correct pens for the screen.
            ** This is very important in case the user or the
            ** application has the pens set in an unusual way.
            */
            mySHADOWPEN = drawinfo->dri_Pens[SHADOWPEN];
            mySHINEPEN  = drawinfo->dri_Pens[SHINEPEN];
            
            printf("ShadowBorder: Got DrawInfo - SHADOWPEN=%lu, SHINEPEN=%lu\n", 
                   mySHADOWPEN, mySHINEPEN);
            
            FreeScreenDrawInfo(screen, drawinfo);
        }
        else
        {
            printf("ShadowBorder: Using default pens (couldn't get DrawInfo)\n");
        }
        UnlockPubScreen(NULL, screen);
    }
    else
    {
        printf("ShadowBorder: Using default pens (couldn't lock screen)\n");
    }

    /* Open a simple window on the workbench screen */
    win = OpenWindowTags(NULL,
                        WA_Width,       200,
                        WA_Height,      100,
                        WA_Title,       (ULONG)"ShadowBorder Demo",
                        WA_Flags,       WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET,
                        WA_IDCMP,       IDCMP_CLOSEWINDOW,
                        WA_RMBTrap,     TRUE,
                        TAG_END);
    if (win == NULL)
    {
        printf("ShadowBorder: ERROR - Can't open window\n");
        CloseLibrary(IntuitionBase);
        return RETURN_FAIL;
    }
    printf("ShadowBorder: Opened window at %d,%d size %dx%d\n", 
           win->LeftEdge, win->TopEdge, win->Width, win->Height);

    /* Set information specific to the shadow component of the border */
    shadowBorder.LeftEdge   = MYBORDER_LEFT + 1;
    shadowBorder.TopEdge    = MYBORDER_TOP + 1;
    shadowBorder.FrontPen   = mySHADOWPEN;
    shadowBorder.NextBorder = &shineBorder;

    /* Set information specific to the shine component of the border */
    shineBorder.LeftEdge    = MYBORDER_LEFT;
    shineBorder.TopEdge     = MYBORDER_TOP;
    shineBorder.FrontPen    = mySHINEPEN;
    shineBorder.NextBorder  = NULL;

    /* The following attributes are the same for both borders */
    shadowBorder.BackPen    = shineBorder.BackPen   = 0;
    shadowBorder.DrawMode   = shineBorder.DrawMode  = JAM1;
    shadowBorder.Count      = shineBorder.Count     = 5;
    shadowBorder.XY         = shineBorder.XY        = myBorderData;

    printf("\nShadowBorder: Border structures initialized\n");
    printf("  Shadow: LeftEdge=%d, TopEdge=%d, FrontPen=%d, Count=%d\n",
           shadowBorder.LeftEdge, shadowBorder.TopEdge, 
           shadowBorder.FrontPen, shadowBorder.Count);
    printf("  Shine:  LeftEdge=%d, TopEdge=%d, FrontPen=%d, Count=%d\n",
           shineBorder.LeftEdge, shineBorder.TopEdge,
           shineBorder.FrontPen, shineBorder.Count);

    /* Draw the border at 10,20 */
    printf("\nShadowBorder: Drawing border at position (10, 20)...\n");
    DrawBorder(win->RPort, &shadowBorder, 10, 20);
    printf("ShadowBorder: First DrawBorder() complete\n");

    /* Draw the border again at 100,20 */
    printf("ShadowBorder: Drawing border at position (100, 20)...\n");
    DrawBorder(win->RPort, &shadowBorder, 100, 20);
    printf("ShadowBorder: Second DrawBorder() complete\n");

    /* Draw a third border at 50,55 */
    printf("ShadowBorder: Drawing border at position (50, 55)...\n");
    DrawBorder(win->RPort, &shadowBorder, 50, 55);
    printf("ShadowBorder: Third DrawBorder() complete\n");

    printf("\nShadowBorder: All borders drawn successfully!\n");

    /* Clean up */
    printf("\nShadowBorder: Closing window...\n");
    CloseWindow(win);

    printf("ShadowBorder: Closing library...\n");
    CloseLibrary(IntuitionBase);

    printf("\nShadowBorder: Demo complete.\n");
    return RETURN_OK;
}
