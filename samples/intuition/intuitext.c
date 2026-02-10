/*
 * intuitext.c - Integration test for IntuiText rendering
 * 
 * Based on RKM sample code, enhanced for automated testing.
 * Tests: PrintIText(), GetScreenDrawInfo(), LockPubScreen(), UnlockPubScreen(), FreeScreenDrawInfo()
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *IntuitionBase = NULL;

#define MYTEXT_LEFT (0)
#define MYTEXT_TOP  (0)

VOID main(int argc, char **argv)
{
    struct Screen    *screen;
    struct DrawInfo  *drawinfo;
    struct Window    *win;
    struct IntuiText  myIText;
    struct TextAttr   myTextAttr;
    
    ULONG myTEXTPEN;
    ULONG myBACKGROUNDPEN;
    
    IntuitionBase = OpenLibrary("intuition.library",37);
    if (IntuitionBase)
    {
        printf("IntuiText: Locking public screen...\n");
        screen = LockPubScreen(NULL);
        if (screen)
        {
            printf("IntuiText: Public screen locked (0x%08lx)\n", (ULONG)screen);
            
            printf("IntuiText: Getting screen draw info...\n");
            drawinfo = GetScreenDrawInfo(screen);
            if (drawinfo)
            {
                printf("IntuiText: DrawInfo obtained\n");
                
                /* Get pens from screen */
                myTEXTPEN = drawinfo->dri_Pens[TEXTPEN];
                myBACKGROUNDPEN = drawinfo->dri_Pens[BACKGROUNDPEN];
                
                printf("IntuiText: Text pen: %ld, Background pen: %ld\n",
                       myTEXTPEN, myBACKGROUNDPEN);
                
                /* Create TextAttr from screen font */
                myTextAttr.ta_Name  = drawinfo->dri_Font->tf_Message.mn_Node.ln_Name;
                myTextAttr.ta_YSize = drawinfo->dri_Font->tf_YSize;
                myTextAttr.ta_Style = drawinfo->dri_Font->tf_Style;
                myTextAttr.ta_Flags = drawinfo->dri_Font->tf_Flags;
                
                printf("IntuiText: Font: %s, YSize: %ld\n",
                       myTextAttr.ta_Name, (LONG)myTextAttr.ta_YSize);
                
                /* Open window on public screen */
                printf("IntuiText: Opening window...\n");
                win = OpenWindowTags(NULL,
                                    WA_PubScreen, screen,
                                    WA_RMBTrap, TRUE,
                                    TAG_END);
                if (win)
                {
                    printf("IntuiText: Window opened (%ldx%ld at %ld,%ld)\n",
                           (LONG)win->Width, (LONG)win->Height,
                           (LONG)win->LeftEdge, (LONG)win->TopEdge);
                    printf("IntuiText: BorderTop=%ld BorderLeft=%ld\n",
                           (LONG)win->BorderTop, (LONG)win->BorderLeft);
                    
                    /* Set up IntuiText */
                    myIText.FrontPen    = myTEXTPEN;
                    myIText.BackPen     = myBACKGROUNDPEN;
                    myIText.DrawMode    = JAM2;
                    myIText.LeftEdge    = MYTEXT_LEFT;
                    myIText.TopEdge     = MYTEXT_TOP;
                    myIText.ITextFont   = &myTextAttr;
                    myIText.IText       = (UBYTE *)"Hello, World.  ;-)";
                    myIText.NextText    = NULL;
                    
                    /* Draw text at position 10,10 */
                    printf("IntuiText: Drawing text with PrintIText()...\n");
                    PrintIText(win->RPort, &myIText, 10, 10);
                    printf("IntuiText: Text drawn successfully\n");
                    
                    /* Test with multiple IntuiText structures */
                    struct IntuiText text2, text3;
                    
                    text2.FrontPen = 2;
                    text2.BackPen = myBACKGROUNDPEN;
                    text2.DrawMode = JAM2;
                    text2.LeftEdge = 0;
                    text2.TopEdge = 15;
                    text2.ITextFont = &myTextAttr;
                    text2.IText = (UBYTE *)"Line 2 - Red";
                    text2.NextText = &text3;
                    
                    text3.FrontPen = 3;
                    text3.BackPen = myBACKGROUNDPEN;
                    text3.DrawMode = JAM2;
                    text3.LeftEdge = 0;
                    text3.TopEdge = 30;
                    text3.ITextFont = &myTextAttr;
                    text3.IText = (UBYTE *)"Line 3 - Green";
                    text3.NextText = NULL;
                    
                    printf("IntuiText: Drawing chained IntuiText...\n");
                    PrintIText(win->RPort, &text2, 10, 10);
                    printf("IntuiText: Chained text drawn successfully\n");
                    
                    /* Wait before closing */
                    printf("IntuiText: Waiting 2 seconds...\n");
                    Delay(100);
                    
                    printf("IntuiText: Closing window\n");
                    CloseWindow(win);
                }
                else
                {
                    printf("IntuiText: ERROR - Failed to open window\n");
                }
                
                printf("IntuiText: Freeing screen draw info\n");
                FreeScreenDrawInfo(screen, drawinfo);
            }
            else
            {
                printf("IntuiText: ERROR - Failed to get screen draw info\n");
            }
            
            printf("IntuiText: Unlocking public screen\n");
            UnlockPubScreen(NULL, screen);
        }
        else
        {
            printf("IntuiText: ERROR - Failed to lock public screen\n");
        }
        
        CloseLibrary(IntuitionBase);
    }
    else
    {
        printf("IntuiText: ERROR - Failed to open intuition.library v37+\n");
    }
}
