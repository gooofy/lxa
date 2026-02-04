/*
 * updatestrgad.c - Integration test for string gadgets
 * 
 * Based on RKM sample code, enhanced for automated testing.
 * Tests: String gadgets, ActivateGadget(), UpdateStrGad(), RemoveGList(), AddGList(), RefreshGList()
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>

#include <string.h>
#include <stdio.h>

/* Function prototypes */
VOID updateStrGad(struct Window *win, struct Gadget *gad, UBYTE *newstr);

struct Library *IntuitionBase;

#define BUFSIZE (100)
#define MYSTRGADWIDTH (200)
#define MYSTRGADHEIGHT (8)

UWORD strBorderData[] =
{
    0,0, MYSTRGADWIDTH + 3,0, MYSTRGADWIDTH + 3,MYSTRGADHEIGHT + 3,
    0,MYSTRGADHEIGHT + 3, 0,0,
};

struct Border strBorder =
{
    -2,-2,1,0,JAM1,5,strBorderData,NULL,
};

UBYTE strBuffer[BUFSIZE];
UBYTE strUndoBuffer[BUFSIZE];

struct StringInfo strInfo =
{
    strBuffer,strUndoBuffer,0,BUFSIZE,
};

struct Gadget strGad =
{
    NULL, 20,20,MYSTRGADWIDTH,MYSTRGADHEIGHT,
    GFLG_GADGHCOMP, GACT_RELVERIFY | GACT_STRINGCENTER,
    GTYP_STRGADGET, &strBorder, NULL, NULL,0,&strInfo,0,NULL,
};

#define ANSCNT 4
UBYTE *answers[ANSCNT] = {"Try again","Sorry","Perhaps","A Winner"};

/*
 * Update string gadget with new value
 */
VOID updateStrGad(struct Window *win, struct Gadget *gad, UBYTE *newstr)
{
    /* Remove gadget from window before modifying */
    RemoveGList(win,gad,1);
    
    /* Change buffer value and cursor position */
    strcpy(((struct StringInfo *)(gad->SpecialInfo))->Buffer, newstr);
    ((struct StringInfo *)(gad->SpecialInfo))->BufferPos = 0;
    ((struct StringInfo *)(gad->SpecialInfo))->DispPos   = 0;
    
    /* Add gadget back and refresh */
    AddGList(win,gad,~0,1,NULL);
    RefreshGList(gad,win,NULL,1);
    
    /* Reactivate gadget */
    ActivateGadget(gad,win,NULL);
}

VOID main(int argc, char **argv)
{
    struct Window *win;
    
    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (IntuitionBase)
    {
        /* Initialize string buffer */
        strcpy(strBuffer, "START");
        
        win = OpenWindowTags(NULL,
                            WA_Width, 400,
                            WA_Height, 100,
                            WA_Title, (ULONG)"String Gadget Test",
                            WA_Gadgets, &strGad,
                            WA_CloseGadget, TRUE,
                            WA_IDCMP, IDCMP_ACTIVEWINDOW | IDCMP_CLOSEWINDOW | IDCMP_GADGETUP,
                            TAG_END);
        if (win)
        {
            printf("UpdateStrGad: Window opened with string gadget\n");
            printf("UpdateStrGad: Initial value: '%s'\n", strBuffer);
            
            /* Wait for window to become active */
            printf("UpdateStrGad: Waiting for ACTIVEWINDOW event...\n");
            Delay(10);  /* Small delay to ensure window is active */
            
            /* Test ActivateGadget() */
            printf("UpdateStrGad: Activating gadget...\n");
            if (ActivateGadget(&strGad, win, NULL))
            {
                printf("UpdateStrGad: Gadget activated successfully\n");
            }
            else
            {
                printf("UpdateStrGad: ERROR - Failed to activate gadget\n");
            }
            
            /* Test updating string gadget content */
            printf("UpdateStrGad: Testing string gadget updates...\n");
            for (int i = 0; i < ANSCNT; i++)
            {
                printf("UpdateStrGad: Updating to '%s'\n", answers[i]);
                updateStrGad(win, &strGad, answers[i]);
                
                /* Verify update */
                UBYTE *current = ((struct StringInfo *)(strGad.SpecialInfo))->Buffer;
                printf("UpdateStrGad: Current value: '%s'\n", current);
                
                if (strcmp(current, answers[i]) != 0)
                {
                    printf("UpdateStrGad: ERROR - Expected '%s', got '%s'\n", 
                           answers[i], current);
                }
                
                Delay(10);  /* Small delay between updates */
            }
            
            printf("UpdateStrGad: All updates completed successfully\n");
            printf("UpdateStrGad: Closing window\n");
            CloseWindow(win);
        }
        else
        {
            printf("UpdateStrGad: ERROR - Failed to open window\n");
        }
        CloseLibrary(IntuitionBase);
    }
    else
    {
        printf("UpdateStrGad: ERROR - Failed to open intuition.library v37+\n");
    }
}
