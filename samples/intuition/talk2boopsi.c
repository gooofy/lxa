/*
 * talk2boopsi.c - BOOPSI Inter-Object Communication Sample
 *
 * Based on the RKRM Talk2Boopsi sample. It opens a prop gadget and an integer
 * string gadget, then links them so either control updates the other while the
 * window stays open until the user closes it.
 */

#include <exec/types.h>
#include <utility/tagitem.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>

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
#define MINWINDOWWIDTH      80
#define MINWINDOWHEIGHT     (PROPGADGETHEIGHT + 70)
#define MAXCHARS            3L

void main(void)
{
    struct Window *w;
    struct IntuiMessage *msg;
    struct Gadget *prop, *integer;
    BOOL done = FALSE;

    IntuitionBase = OpenLibrary("intuition.library", 37L);
    if (IntuitionBase != NULL)
    {
        w = OpenWindowTags(NULL,
                  WA_Width,       MINWINDOWWIDTH,
                  WA_Height,      MINWINDOWHEIGHT,
                  WA_Flags,       WFLG_DEPTHGADGET | WFLG_DRAGBAR |
                                  WFLG_CLOSEGADGET | WFLG_SIZEGADGET,
                  WA_IDCMP,       IDCMP_CLOSEWINDOW,
                  WA_MinWidth,    MINWINDOWWIDTH,
                  WA_MinHeight,   MINWINDOWHEIGHT,
                  TAG_END);

        if (w != NULL)
        {
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
                integer = (struct Gadget *)NewObject(NULL, "strgclass",
                    GA_ID,       INTGADGET_ID,
                    GA_Top,      (w->BorderTop) + 5L,
                    GA_Left,     (w->BorderLeft) + PROPGADGETWIDTH + 10L,
                    GA_Width,    MINWINDOWWIDTH -
                                 (w->BorderLeft + w->BorderRight +
                                  PROPGADGETWIDTH + 15L),
                    GA_Height,   INTGADGETHEIGHT,
                    ICA_MAP,     int2propmap,    /* Attribute mapping */
                    ICA_TARGET,  prop,           /* Target for updates */
                    GA_Previous, prop,           /* Link in gadget list */
                    STRINGA_LongVal,  INITIALVAL,
                    STRINGA_MaxChars, MAXCHARS,
                    TAG_END);

                if (integer != NULL)
                {
                    SetGadgetAttrs(prop, w, NULL,
                        ICA_TARGET, integer,
                        TAG_END);

                    AddGList(w, prop, -1, -1, NULL);
                    RefreshGList(prop, w, NULL, -1);

                    while (done == FALSE)
                    {
                        WaitPort((struct MsgPort *)w->UserPort);
                        while ((msg = (struct IntuiMessage *)GetMsg((struct MsgPort *)w->UserPort)) != NULL)
                        {
                            if (msg->Class == IDCMP_CLOSEWINDOW)
                            {
                                done = TRUE;
                            }
                            ReplyMsg((struct Message *)msg);
                        }
                    }

                    RemoveGList(w, prop, -1);
                    DisposeObject(integer);
                }

                DisposeObject(prop);
            }

            CloseWindow(w);
        }

        CloseLibrary(IntuitionBase);
    }
}
