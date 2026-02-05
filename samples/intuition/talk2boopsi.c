/*
 * talk2boopsi.c - BOOPSI Inter-Object Communication Sample
 *
 * This is an RKM-style sample demonstrating BOOPSI object-to-object 
 * communication. It creates a prop gadget and an integer string gadget
 * that automatically update each other when the user changes their values.
 *
 * Key Functions Demonstrated:
 *   - NewObject() / DisposeObject()
 *   - SetGadgetAttrs()
 *   - ICA_MAP attribute for automatic value propagation
 *   - ICA_TARGET for specifying update targets
 *   - propgclass and strgclass BOOPSI gadgets
 *
 * Based on RKM BOOPSI sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#include <exec/types.h>
#include <utility/tagitem.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *IntuitionBase;

/* Attribute mapping: prop gadget -> integer gadget */
struct TagItem prop2intmap[] =
{
    {PGA_Top,   STRINGA_LongVal},  /* Map PGA_Top to STRINGA_LongVal */
    {TAG_END,}
};

/* Attribute mapping: integer gadget -> prop gadget */
struct TagItem int2propmap[] =
{
    {STRINGA_LongVal,   PGA_Top},  /* Map STRINGA_LongVal to PGA_Top */
    {TAG_END,}
};

#define PROPGADGET_ID       1L
#define INTGADGET_ID        2L
#define PROPGADGETWIDTH     10L
#define PROPGADGETHEIGHT    80L
#define INTGADGETHEIGHT     18L
#define VISIBLE             10L
#define TOTAL               100L
#define INITIALVAL          25L
#define MINWINDOWWIDTH      200
#define MINWINDOWHEIGHT     (PROPGADGETHEIGHT + 70)
#define MAXCHARS            3L

void main(void)
{
    struct Window *w;
    struct Gadget *prop, *integer;

    printf("Talk2Boopsi: Starting BOOPSI inter-object communication demo\n");

    IntuitionBase = OpenLibrary("intuition.library", 37L);
    if (IntuitionBase != NULL)
    {
        printf("Talk2Boopsi: Opened intuition.library v%d\n",
               IntuitionBase->lib_Version);

        /* Open window - note it doesn't listen for GADGETUP */
        w = OpenWindowTags(NULL,
                 WA_Width,       MINWINDOWWIDTH,
                 WA_Height,      MINWINDOWHEIGHT,
                 WA_Flags,       WFLG_DEPTHGADGET | WFLG_DRAGBAR |
                                 WFLG_CLOSEGADGET | WFLG_SIZEGADGET,
                 WA_IDCMP,       IDCMP_CLOSEWINDOW,
                 WA_MinWidth,    MINWINDOWWIDTH,
                 WA_MinHeight,   MINWINDOWHEIGHT,
                 WA_Title,       "Talk2Boopsi Demo",
                 TAG_END);

        if (w != NULL)
        {
            printf("Talk2Boopsi: Window opened at (%d,%d) size %dx%d\n",
                   w->LeftEdge, w->TopEdge, w->Width, w->Height);

            /* Create propgclass gadget */
            prop = (struct Gadget *)NewObject(NULL, "propgclass",
                GA_ID,     PROPGADGET_ID,
                GA_Top,    (w->BorderTop) + 5L,
                GA_Left,   (w->BorderLeft) + 5L,
                GA_Width,  PROPGADGETWIDTH,
                GA_Height, PROPGADGETHEIGHT,
                ICA_MAP,   prop2intmap,      /* Attribute mapping */
                PGA_Total,   TOTAL,          /* Integer range */
                PGA_Top,     INITIALVAL,     /* Initial value */
                PGA_Visible, VISIBLE,        /* Knob coverage */
                PGA_NewLook, TRUE,           /* Modern look */
                TAG_END);

            if (prop != NULL)
            {
                printf("Talk2Boopsi: Created propgclass gadget (id=%d)\n",
                       PROPGADGET_ID);
                printf("Talk2Boopsi:   PGA_Total=%d PGA_Top=%d PGA_Visible=%d\n",
                       TOTAL, INITIALVAL, VISIBLE);

                /* Create strgclass gadget */
                integer = (struct Gadget *)NewObject(NULL, "strgclass",
                    GA_ID,       INTGADGET_ID,
                    GA_Top,      (w->BorderTop) + 5L,
                    GA_Left,     (w->BorderLeft) + PROPGADGETWIDTH + 15L,
                    GA_Width,    60L,
                    GA_Height,   INTGADGETHEIGHT,
                    ICA_MAP,     int2propmap,    /* Attribute mapping */
                    ICA_TARGET,  prop,           /* Target for updates */
                    GA_Previous, prop,           /* Link in gadget list */
                    STRINGA_LongVal,  INITIALVAL,
                    STRINGA_MaxChars, MAXCHARS,
                    TAG_END);

                if (integer != NULL)
                {
                    printf("Talk2Boopsi: Created strgclass gadget (id=%d)\n",
                           INTGADGET_ID);
                    printf("Talk2Boopsi:   STRINGA_LongVal=%d STRINGA_MaxChars=%d\n",
                           INITIALVAL, MAXCHARS);

                    /* Now set the prop gadget's target to the integer gadget */
                    SetGadgetAttrs(prop, w, NULL,
                        ICA_TARGET, integer,
                        TAG_END);
                    printf("Talk2Boopsi: Connected prop -> integer via ICA_TARGET\n");

                    /* Add gadgets to window */
                    AddGList(w, prop, -1, -1, NULL);
                    RefreshGList(prop, w, NULL, -1);
                    printf("Talk2Boopsi: Gadgets added and displayed\n");

                    printf("Talk2Boopsi: BOOPSI connection established!\n");
                    printf("Talk2Boopsi: Prop and integer gadgets will auto-update each other\n");

                    /* For automated testing, just delay and exit */
                    printf("Talk2Boopsi: Simulating test (delay 1 sec)\n");
                    Delay(50);

                    /* Cleanup */
                    RemoveGList(w, prop, -1);
                    printf("Talk2Boopsi: Gadgets removed from window\n");

                    DisposeObject(integer);
                    printf("Talk2Boopsi: Disposed strgclass gadget\n");
                }
                else
                {
                    printf("Talk2Boopsi: Failed to create strgclass gadget\n");
                }

                DisposeObject(prop);
                printf("Talk2Boopsi: Disposed propgclass gadget\n");
            }
            else
            {
                printf("Talk2Boopsi: Failed to create propgclass gadget\n");
            }

            CloseWindow(w);
            printf("Talk2Boopsi: Window closed\n");
        }
        else
        {
            printf("Talk2Boopsi: Failed to open window\n");
        }

        CloseLibrary(IntuitionBase);
    }
    else
    {
        printf("Talk2Boopsi: Failed to open intuition.library v37\n");
    }

    printf("Talk2Boopsi: Done\n");
}
