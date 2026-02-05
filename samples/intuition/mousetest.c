/*
 * mousetest.c - Mouse Input Handling Sample
 *
 * This is an RKM-style sample demonstrating mouse input handling in Intuition.
 * It creates a window that:
 *   - Reports mouse movement coordinates
 *   - Reports mouse button presses (left/right/middle)
 *   - Detects double-clicks using DoubleClick()
 *   - Shows shift+click detection via qualifiers
 *
 * Based on RKM Intuition sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#include <devices/inputevent.h>

#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

/* something to use to track the time between messages
** to test for double-clicks.
*/
typedef struct myTimeVal
{
    ULONG LeftSeconds;
    ULONG LeftMicros;
    ULONG RightSeconds;
    ULONG RightMicros;
} MYTIMEVAL;

/* our function prototypes */
VOID doButtons(struct IntuiMessage *msg, MYTIMEVAL *tv);
VOID process_window(struct Window *win);

struct Library *IntuitionBase;
struct GfxBase *GfxBase;

/*
** main() -- set-up everything.
*/
VOID main(int argc, char **argv)
{
    struct Window *win;
    struct Screen *scr;
    struct DrawInfo *dr_info;
    ULONG width;

    printf("MouseTest: Starting mouse input demonstration\n");

    /* Open the libraries we will use. Requires Release 2 (KS V2.04, V37) */
    if (IntuitionBase = OpenLibrary("intuition.library", 37))
    {
        if (GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37))
        {
            /* Lock the default public screen in order to read its DrawInfo data */
            if (scr = LockPubScreen(NULL))
            {
                if (dr_info = GetScreenDrawInfo(scr))
                {
                    /* use wider of space needed for output (18 chars and spaces)
                     * or titlebar text plus room for titlebar gads (approx 18 each)
                     */
                    width = (GfxBase->DefaultFont->tf_XSize * 18);
                    if (width < ((18 * 2) + TextLength(&scr->RastPort, "MouseTest", 9)))
                        width = (18 * 2) + TextLength(&scr->RastPort, "MouseTest", 9);

                    if (win = OpenWindowTags(NULL,
                                WA_Top,    20,
                                WA_Left,   100,
                                WA_InnerWidth,  width,
                                WA_Height, (2 * GfxBase->DefaultFont->tf_YSize) +
                                           scr->WBorTop + scr->Font->ta_YSize + 1 +
                                           scr->WBorBottom,
                                WA_Flags, WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                                          WFLG_ACTIVATE    | WFLG_REPORTMOUSE |
                                          WFLG_RMBTRAP     | WFLG_DRAGBAR,
                                WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_RAWKEY |
                                          IDCMP_MOUSEMOVE   | IDCMP_MOUSEBUTTONS,
                                WA_Title, "MouseTest",
                                WA_PubScreen, scr,
                                TAG_END))
                    {
                        printf("MouseTest: Window opened successfully\n");
                        printf("MouseTest: Move mouse, click and double-click in window\n");
                        printf("MouseTest: Shift+click to test qualifiers\n");
                        printf("MouseTest: Click close gadget to exit\n");

                        SetAPen(win->RPort, dr_info->dri_Pens[TEXTPEN]);
                        SetBPen(win->RPort, dr_info->dri_Pens[BACKGROUNDPEN]);
                        SetDrMd(win->RPort, JAM2);

                        process_window(win);

                        CloseWindow(win);
                        printf("MouseTest: Window closed\n");
                    }
                    else
                    {
                        printf("MouseTest: Failed to open window\n");
                    }
                    FreeScreenDrawInfo(scr, dr_info);
                }
                else
                {
                    printf("MouseTest: Failed to get DrawInfo\n");
                }
                UnlockPubScreen(NULL, scr);
            }
            else
            {
                printf("MouseTest: Failed to lock public screen\n");
            }
            CloseLibrary((struct Library *)GfxBase);
        }
        else
        {
            printf("MouseTest: Failed to open graphics.library\n");
        }
        CloseLibrary(IntuitionBase);
    }
    else
    {
        printf("MouseTest: Failed to open intuition.library\n");
    }
    printf("MouseTest: Done\n");
}

/*
** process_window() - simple message loop for processing IntuiMessages
*/
VOID process_window(struct Window *win)
{
    USHORT done;
    struct IntuiMessage *msg;
    MYTIMEVAL tv;
    UBYTE prt_buff[14];
    LONG xText, yText;  /* places to position text in window. */
    ULONG move_count = 0;

    done = FALSE;
    tv.LeftSeconds = 0;  /* initial values for testing double-click */
    tv.LeftMicros  = 0;
    tv.RightSeconds = 0;
    tv.RightMicros  = 0;
    xText = win->BorderLeft + (win->IFont->tf_XSize * 2);
    yText = win->BorderTop + 3 + win->IFont->tf_Baseline;

    while (!done)
    {
        Wait((1L << win->UserPort->mp_SigBit));

        while ((!done) &&
               (msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
        {
            switch (msg->Class)
            {
                case IDCMP_CLOSEWINDOW:
                    printf("MouseTest: Close gadget clicked\n");
                    done = TRUE;
                    break;
                case IDCMP_MOUSEMOVE:
                    /* Show the current position of the mouse relative to the
                    ** upper left hand corner of our window
                    */
                    Move(win->RPort, xText, yText);
                    sprintf(prt_buff, "X%5d Y%5d", msg->MouseX, msg->MouseY);
                    Text(win->RPort, prt_buff, 13);
                    
                    /* Only print every Nth move to reduce output spam */
                    move_count++;
                    if (move_count <= 3)
                    {
                        printf("MouseTest: MouseMove X=%d Y=%d\n", 
                               msg->MouseX, msg->MouseY);
                    }
                    break;
                case IDCMP_MOUSEBUTTONS:
                    doButtons(msg, &tv);
                    break;
            }
            ReplyMsg((struct Message *)msg);
        }
    }
    
    if (move_count > 3)
    {
        printf("MouseTest: (%ld additional mouse moves)\n", move_count - 3);
    }
}

/*
** Show what mouse buttons were pushed
*/
VOID doButtons(struct IntuiMessage *msg, MYTIMEVAL *tv)
{
    /* Yes, qualifiers can apply to the mouse also. That is how
    ** we get the shift select on the Workbench. This shows how
    ** to see if a specific bit is set within the qualifier
    */
    if (msg->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
        printf("MouseTest: Shift ");

    switch (msg->Code)
    {
        case SELECTDOWN:
            printf("MouseTest: Left Button Down at X=%ld Y=%ld", 
                   msg->MouseX, msg->MouseY);
            if (DoubleClick(tv->LeftSeconds, tv->LeftMicros, 
                           msg->Seconds, msg->Micros))
            {
                printf(" DoubleClick!");
            }
            else
            {
                tv->LeftSeconds = msg->Seconds;
                tv->LeftMicros  = msg->Micros;
                tv->RightSeconds = 0;
                tv->RightMicros  = 0;
            }
            printf("\n");
            break;
        case SELECTUP:
            printf("MouseTest: Left Button Up at X=%ld Y=%ld\n", 
                   msg->MouseX, msg->MouseY);
            break;
        case MENUDOWN:
            printf("MouseTest: Right Button Down at X=%ld Y=%ld", 
                   msg->MouseX, msg->MouseY);
            if (DoubleClick(tv->RightSeconds, tv->RightMicros, 
                           msg->Seconds, msg->Micros))
            {
                printf(" DoubleClick!");
            }
            else
            {
                tv->LeftSeconds = 0;
                tv->LeftMicros  = 0;
                tv->RightSeconds = msg->Seconds;
                tv->RightMicros  = msg->Micros;
            }
            printf("\n");
            break;
        case MENUUP:
            printf("MouseTest: Right Button Up at X=%ld Y=%ld\n", 
                   msg->MouseX, msg->MouseY);
            break;
        case MIDDLEDOWN:
            printf("MouseTest: Middle Button Down at X=%ld Y=%ld\n", 
                   msg->MouseX, msg->MouseY);
            break;
        case MIDDLEUP:
            printf("MouseTest: Middle Button Up at X=%ld Y=%ld\n", 
                   msg->MouseX, msg->MouseY);
            break;
    }
}
