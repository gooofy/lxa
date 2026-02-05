/*
 * simplegad.c - Boolean Gadgets Example
 *
 * This is an RKM-style sample demonstrating boolean gadgets in Intuition.
 * It creates a window with three buttons:
 *   - Button 1: Uses both GACT_IMMEDIATE and GACT_RELVERIFY
 *   - Button 2: Uses only GACT_RELVERIFY
 *   - Exit: Closes the window when clicked
 *
 * The sample has a proper IDCMP event loop and runs until the user
 * clicks the close gadget or the Exit button.
 *
 * Based on RKM Intuition sample code.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfxbase.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>

#include <stdio.h>

struct Library *IntuitionBase;
struct Library *GfxBase;

#define BUTTON_GAD_NUM   (3)
#define BUTTON2_GAD_NUM  (4)
#define BUTTON3_GAD_NUM  (5)
#define MYBUTTONGADWIDTH (100)
#define MYBUTTONGADHEIGHT (50)

/* Border for button gadget */
UWORD buttonBorderData[] =
{
    0,0, MYBUTTONGADWIDTH + 1,0, MYBUTTONGADWIDTH + 1,MYBUTTONGADHEIGHT + 1,
    0,MYBUTTONGADHEIGHT + 1, 0,0,
};

struct Border buttonBorder =
{
    -1,-1,1,0,JAM1,5,buttonBorderData,NULL,
};

/* Border for second button */
UWORD button2BorderData[] =
{
    0,0, MYBUTTONGADWIDTH + 1,0, MYBUTTONGADWIDTH + 1,MYBUTTONGADHEIGHT + 1,
    0,MYBUTTONGADHEIGHT + 1, 0,0,
};

struct Border button2Border =
{
    -1,-1,2,0,JAM1,5,button2BorderData,NULL,
};

/* Border for third button */
UWORD button3BorderData[] =
{
    0,0, MYBUTTONGADWIDTH + 1,0, MYBUTTONGADWIDTH + 1,MYBUTTONGADHEIGHT + 1,
    0,MYBUTTONGADHEIGHT + 1, 0,0,
};

struct Border button3Border =
{
    -1,-1,3,0,JAM1,5,button3BorderData,NULL,
};

/* Text for gadget labels */
struct IntuiText button1Text = { 1, 0, JAM1, 10, 20, NULL, (UBYTE *)"Button 1", NULL };
struct IntuiText button2Text = { 2, 0, JAM1, 10, 20, NULL, (UBYTE *)"Button 2", NULL };
struct IntuiText button3Text = { 3, 0, JAM1, 10, 20, NULL, (UBYTE *)"Exit", NULL };

/* Third button - exit button */
struct Gadget button3Gad =
{
    NULL, 20, 160, MYBUTTONGADWIDTH, MYBUTTONGADHEIGHT,
    GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, &button3Border, NULL, &button3Text, 0, NULL, BUTTON3_GAD_NUM, NULL,
};

/* Second button - only RELVERIFY */
struct Gadget button2Gad =
{
    &button3Gad, 20, 90, MYBUTTONGADWIDTH, MYBUTTONGADHEIGHT,
    GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, &button2Border, NULL, &button2Text, 0, NULL, BUTTON2_GAD_NUM, NULL,
};

/* First button - both IMMEDIATE and RELVERIFY */
struct Gadget buttonGad =
{
    &button2Gad, 20, 20, MYBUTTONGADWIDTH, MYBUTTONGADHEIGHT,
    GFLG_GADGHCOMP, GACT_RELVERIFY | GACT_IMMEDIATE,
    GTYP_BOOLGADGET, &buttonBorder, NULL, &button1Text, 0, NULL, BUTTON_GAD_NUM, NULL,
};

VOID main(int argc, char **argv)
{
    struct Window *win;
    struct IntuiMessage *msg;
    ULONG class;
    struct Gadget *gad;
    UWORD gadgetID;
    BOOL done = FALSE;

    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
    {
        printf("SimpleGad: Failed to open intuition.library v37+\n");
        return;
    }

    GfxBase = OpenLibrary("graphics.library", 37);
    if (!GfxBase)
    {
        printf("SimpleGad: Failed to open graphics.library v37+\n");
        CloseLibrary(IntuitionBase);
        return;
    }

    win = OpenWindowTags(NULL,
                        WA_Width, 400,
                        WA_Height, 250,
                        WA_Gadgets, &buttonGad,
                        WA_Title, (ULONG)"Boolean Gadget Demo",
                        WA_Activate, TRUE,
                        WA_CloseGadget, TRUE,
                        WA_DragBar, TRUE,
                        WA_DepthGadget, TRUE,
                        WA_IDCMP, IDCMP_GADGETDOWN | IDCMP_GADGETUP |
                                  IDCMP_CLOSEWINDOW,
                        TAG_END);
    if (!win)
    {
        printf("SimpleGad: Failed to open window\n");
        CloseLibrary(GfxBase);
        CloseLibrary(IntuitionBase);
        return;
    }

    printf("SimpleGad: Window opened with 3 boolean gadgets\n");
    printf("SimpleGad: Button 1 has IMMEDIATE+RELVERIFY (reports both down and up)\n");
    printf("SimpleGad: Button 2 has RELVERIFY only (reports only up)\n");
    printf("SimpleGad: Exit button closes the window\n");
    printf("SimpleGad: Click the close gadget or Exit button to quit\n");

    /* Main event loop */
    while (!done)
    {
        /* Wait for a message */
        WaitPort(win->UserPort);
        
        /* Process all pending messages */
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL)
        {
            class = msg->Class;
            gad = (struct Gadget *)msg->IAddress;
            
            /* Extract gadget ID if this is a gadget event */
            if (class == IDCMP_GADGETDOWN || class == IDCMP_GADGETUP)
            {
                gadgetID = gad ? gad->GadgetID : 0;
            }
            
            /* Reply to the message before processing */
            ReplyMsg((struct Message *)msg);
            
            /* Handle the message */
            switch (class)
            {
                case IDCMP_CLOSEWINDOW:
                    printf("SimpleGad: Close gadget clicked - exiting\n");
                    done = TRUE;
                    break;
                    
                case IDCMP_GADGETDOWN:
                    printf("SimpleGad: GADGETDOWN - Gadget ID %d pressed\n", gadgetID);
                    break;
                    
                case IDCMP_GADGETUP:
                    printf("SimpleGad: GADGETUP - Gadget ID %d released\n", gadgetID);
                    if (gadgetID == BUTTON3_GAD_NUM)
                    {
                        printf("SimpleGad: Exit button clicked - exiting\n");
                        done = TRUE;
                    }
                    break;
            }
        }
    }

    printf("SimpleGad: Closing window\n");
    CloseWindow(win);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
}
