/*
 * GalleryReq - system requesters (Phase 223).
 *
 *   GalleryReq          BuildEasyRequest(): title, two-line body, three buttons
 *   GalleryReq auto     BuildSysRequest(NULL, ...) (the AutoRequest() look;
 *                       without a reference window it is titled "System Request")
 *
 * The requesters are built non-blocking and serviced with SysReqHandler(),
 * so the program can still quit on CTRL-C or on CLOSEWINDOW of its parent
 * window.
 */

#include "gallery.h"

static struct IntuiText body2 = { 0, 1, JAM2, 15, 15, NULL, (UBYTE *)"Insert volume in any drive", NULL };
static struct IntuiText body1 = { 0, 1, JAM2, 15, 5, NULL, (UBYTE *)"Please replace volume Work:", &body2 };
static struct IntuiText postext = { 0, 1, JAM2, 6, 3, NULL, (UBYTE *)"Retry", NULL };
static struct IntuiText negtext = { 0, 1, JAM2, 6, 3, NULL, (UBYTE *)"Cancel", NULL };

int main(int argc, char **argv)
{
    struct Window *parent, *req;
    struct EasyStruct es;
    BOOL done = FALSE;

    if (!gallery_open_libs())
        return 20;

    parent = OpenWindowTags(NULL,
        WA_Left, 0, WA_Top, 200, WA_Width, 160, WA_Height, 40,
        WA_Title, (ULONG)"Req Parent",
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW,
        TAG_END);
    if (!parent)
        return 10;

    if (argc > 1 && !strcmp(argv[1], "auto"))
    {
        req = BuildSysRequest(NULL, &body1, &postext, &negtext, 0, 320, 70);
    }
    else
    {
        es.es_StructSize = sizeof(es);
        es.es_Flags = 0;
        es.es_Title = (UBYTE *)"Gallery Request";
        es.es_TextFormat = (UBYTE *)"This is an EasyRequest with %ld lines\nof body text, centred.";
        es.es_GadgetFormat = (UBYTE *)"OK|Maybe|Cancel";
        req = BuildEasyRequest(parent, &es, 0, 2L);
    }
    if (!req || req == (struct Window *)1)
    {
        printf("GalleryReq: no requester window\n");
        CloseWindow(parent);
        return 10;
    }

    while (!done)
    {
        ULONG sigs = Wait(SIGBREAKF_CTRL_C | (1UL << parent->UserPort->mp_SigBit) |
                          (1UL << req->UserPort->mp_SigBit));
        struct IntuiMessage *im;

        if (sigs & SIGBREAKF_CTRL_C)
            done = TRUE;
        while ((im = (struct IntuiMessage *)GetMsg(parent->UserPort)) != NULL)
        {
            if (im->Class == IDCMP_CLOSEWINDOW)
                done = TRUE;
            ReplyMsg((struct Message *)im);
        }
        if (!done && SysReqHandler(req, NULL, FALSE) != -2)
            done = TRUE;
    }
    FreeSysRequest(req);
    CloseWindow(parent);
    gallery_close_libs();
    return 0;
}
