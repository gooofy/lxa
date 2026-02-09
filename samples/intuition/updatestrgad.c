/*
 * updatestrgad.c - String gadget demonstration
 * 
 * Based on RKM sample code.
 * Tests: String gadgets, ActivateGadget(), UpdateStrGad(), RemoveGList(), AddGList(), RefreshGList()
 * 
 * This sample demonstrates:
 * - Auto-activating a string gadget when window becomes active
 * - Updating gadget contents programmatically on user input
 * - Cycling through response strings on RETURN key
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
VOID handleWindow(struct Window *win, struct Gadget *gad);

struct Library *IntuitionBase;

/* NOTE that the use of constant size and positioning values are
** not recommended; it just makes it easy to show what is going on.
** The position of the gadget should be dynamically adjusted depending
** on the height of the font in the title bar of the window.  This
** example adapts the gadget height to the screen font. Alternately,
** you could specify your font under V37 with the StringExtend structure.
*/
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

/* Cycling answers - shown when user hits RETURN in the gadget */
#define ANSCNT 4
UBYTE *answers[ANSCNT] = {"Try again","Sorry","Perhaps","A Winner"};
int ansnum = 0;

/* Text shown when window is activated */
UBYTE *activated_txt = "Activated";

/*
** main - show the use of a string gadget.
*/
int main(int argc, char **argv)
{
    struct Window *win;
    
    /* make sure to get intuition version 37, for OpenWindowTags() */
    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
    {
        printf("ERROR: Failed to open intuition.library v37+\n");
        return 1;
    }
    
    /* Load a value into the string gadget buffer.
    ** This will be displayed when the gadget is first created.
    */
    strcpy(strBuffer, "START");
    
    win = OpenWindowTags(NULL,
                        WA_Width, 400,
                        WA_Height, 100,
                        WA_Title, (ULONG)"Activate Window, Enter Text",
                        WA_Gadgets, &strGad,
                        WA_CloseGadget, TRUE,
                        WA_IDCMP, IDCMP_ACTIVEWINDOW | IDCMP_CLOSEWINDOW | IDCMP_GADGETUP,
                        TAG_END);
    if (!win)
    {
        printf("ERROR: Failed to open window\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    
    printf("Window opened with string gadget\n");
    printf("Initial value: '%s'\n", strBuffer);
    
    handleWindow(win, &strGad);
    
    CloseWindow(win);
    CloseLibrary(IntuitionBase);

    return 0;
}


/*
** Process messages received by the window.  Quit when the close gadget
** is selected, activate the gadget when the window becomes active.
*/
VOID handleWindow(struct Window *win, struct Gadget *gad)
{
    struct IntuiMessage *msg;
    ULONG class;
    
    for (;;)
    {
        Wait(1L << win->UserPort->mp_SigBit);
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL)
        {
            /* Stash message contents and reply, important when message
            ** triggers some lengthy processing
            */
            class = msg->Class;
            ReplyMsg((struct Message *)msg);
            
            switch (class)
            {
                case IDCMP_ACTIVEWINDOW:
                    /* activate the string gadget.  This is how to activate a
                    ** string gadget in a new window--wait for the window to
                    ** become active by waiting for the IDCMP_ACTIVEWINDOW
                    ** event, then activate the gadget.  Here we report on
                    ** the success or failure.
                    */
                    printf("IDCMP_ACTIVEWINDOW\n");
                    if (ActivateGadget(gad, win, NULL))
                    {
                        printf("Gadget activated, updating to 'Activated'\n");
                        updateStrGad(win, gad, activated_txt);
                    }
                    break;
                    
                case IDCMP_CLOSEWINDOW:
                    /* here is the way out of the loop and the routine.
                    ** be sure that the message was replied...
                    */
                    printf("IDCMP_CLOSEWINDOW\n");
                    return;
                    
                case IDCMP_GADGETUP:
                    /* If user hit RETURN in our string gadget for demonstration,
                    ** we will change what he entered.  We only have 1 gadget,
                    ** so we don't have to check which gadget.
                    */
                    {
                        struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
                        printf("IDCMP_GADGETUP: string was '%s', changing to '%s'\n", 
                               si->Buffer, answers[ansnum]);
                        updateStrGad(win, gad, answers[ansnum]);
                        if (++ansnum >= ANSCNT) 
                            ansnum = 0;  /* point to next answer */
                    }
                    break;
            }
        }
    }
}


/*
** Routine to update the value in the string gadget's buffer, then
** activate the gadget.
*/
VOID updateStrGad(struct Window *win, struct Gadget *gad, UBYTE *newstr)
{
    /* first, remove the gadget from the window.  this must be done before
    ** modifying any part of the gadget!!!
    */
    RemoveGList(win, gad, 1);
    
    /* For fun, change the value in the buffer, as well as the cursor and
    ** initial display position.
    */
    strcpy(((struct StringInfo *)(gad->SpecialInfo))->Buffer, newstr);
    ((struct StringInfo *)(gad->SpecialInfo))->BufferPos = 0;
    ((struct StringInfo *)(gad->SpecialInfo))->DispPos   = 0;
    
    /* Add the gadget back, placing it at the end of the list (~0)
    ** and refresh its imagery.
    */
    AddGList(win, gad, ~0, 1, NULL);
    RefreshGList(gad, win, NULL, 1);
    
    /* Activate the string gadget */
    ActivateGadget(gad, win, NULL);
}
