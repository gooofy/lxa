/*
 * updatestrgad.c - String gadget demonstration
 * 
 * Based on RKM sample code.
 * Tests: String gadgets, ActivateGadget(), UpdateStrGad(), RemoveGList(), AddGList(), RefreshGList()
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfxbase.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>

#include <string.h>
#include <stdio.h>

/* Function prototypes */
VOID updateStrGad(struct Window *win, struct Gadget *gad, UBYTE *newstr);

struct Library *IntuitionBase;
struct Library *GfxBase;

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

int main(int argc, char **argv)
{
    struct Window *win;
    struct IntuiMessage *msg;
    BOOL done = FALSE;
    
    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
    {
        printf("ERROR: Failed to open intuition.library v37+\n");
        return 1;
    }
    
    GfxBase = OpenLibrary("graphics.library", 37);
    if (!GfxBase)
    {
        printf("ERROR: Failed to open graphics.library v37+\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    
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
    if (!win)
    {
        printf("ERROR: Failed to open window\n");
        CloseLibrary(GfxBase);
        CloseLibrary(IntuitionBase);
        return 1;
    }
    
    printf("Window opened with string gadget\n");
    printf("Initial value: '%s'\n", strBuffer);
    
    /* Event loop */
    while (!done)
    {
        Wait(1L << win->UserPort->mp_SigBit);

        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL)
        {
            if (msg->Class == IDCMP_CLOSEWINDOW)
            {
                printf("IDCMP_CLOSEWINDOW\n");
                done = TRUE;
            }
            else if (msg->Class == IDCMP_GADGETUP)
            {
                struct StringInfo *si = (struct StringInfo *)((struct Gadget *)msg->IAddress)->SpecialInfo;
                printf("IDCMP_GADGETUP: string is '%s'\n", si->Buffer);
            }
            else if (msg->Class == IDCMP_ACTIVEWINDOW)
            {
                printf("Window activated\n");
            }

            ReplyMsg((struct Message *)msg);
        }
    }
    
    CloseWindow(win);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);

    return 0;
}
