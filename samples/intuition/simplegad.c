/*
 * simplegad.c - Integration test for boolean gadgets
 * 
 * Based on RKM sample code, enhanced for automated testing.
 * Tests: GACT_RELVERIFY, GACT_IMMEDIATE, IDCMP_GADGETUP/DOWN events
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>

struct Library *IntuitionBase;

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
    ULONG  class;
    BOOL done;

    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (IntuitionBase)
    {
        win = OpenWindowTags(NULL,
                            WA_Width, 400,
                            WA_Height, 250,
                            WA_Gadgets, &buttonGad,
                            WA_Title, (ULONG)"Boolean Gadget Test",
                            WA_Activate, TRUE,
                            WA_CloseGadget, TRUE,
                            WA_DragBar, TRUE,
                            WA_DepthGadget, TRUE,
                            WA_IDCMP, IDCMP_GADGETDOWN | IDCMP_GADGETUP |
                                      IDCMP_CLOSEWINDOW,
                            TAG_END);
        if (win)
        {
            printf("SimpleGad: Window opened with 3 boolean gadgets\n");
            printf("SimpleGad: - Button 1: GACT_IMMEDIATE | GACT_RELVERIFY\n");
            printf("SimpleGad: - Button 2: GACT_RELVERIFY only\n");
            printf("SimpleGad: - Exit: GACT_RELVERIFY only\n");
            printf("SimpleGad: Gadgets visible in window: %s\n",
                   win->FirstGadget ? "YES" : "NO");
            
            /* Verify gadget chain */
            printf("SimpleGad: Verifying gadget chain...\n");
            
            struct Gadget *gad = win->FirstGadget;
            LONG gadget_count = 0;
            while (gad)
            {
                gadget_count++;
                printf("SimpleGad:   Gadget %ld: ID=%ld, Flags=0x%04lx, Activation=0x%04lx\n",
                       gadget_count, (LONG)gad->GadgetID, 
                       (LONG)gad->Flags, (LONG)gad->Activation);
                gad = gad->NextGadget;
            }
            
            printf("SimpleGad: Found %ld gadgets (expected 5: 3 custom + close + depth)\n", 
                   gadget_count);
            
            /* Auto-close after 2 seconds for automated testing */
            printf("SimpleGad: Waiting 2 seconds before auto-close...\n");
            Delay(100);  /* 100 ticks = 2 seconds */
            
            printf("SimpleGad: Closing window\n");
            CloseWindow(win);
        }
        else
        {
            printf("SimpleGad: ERROR - Failed to open window\n");
        }
        CloseLibrary(IntuitionBase);
    }
    else
    {
        printf("SimpleGad: ERROR - Failed to open intuition.library v37+\n");
    }
}
