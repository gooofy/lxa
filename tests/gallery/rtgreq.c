/*
 * GalleryRtgReq - the screen mode requesters of the RTG libraries
 * (Phase 246, reference profile rtg).
 *
 *   GalleryRtgReq p96      p96RequestModeIDTagList, title "Gallery P96 Mode"
 *   GalleryRtgReq p96init  ... with the 640x480 16 bit mode preselected,
 *                          buttons "Use"/"Abort", 16 bit modes and up
 *   GalleryRtgReq p96def   ... without tags
 *   GalleryRtgReq cgx      CModeRequestTagList, title "Gallery CGX Mode"
 *   GalleryRtgReq cgxdef   ... without tags
 *
 * The requester is opened on the Workbench screen; when it returns, the
 * selected mode ID is printed on stdout followed by "DONE" (a scenario
 * clicks OK/Cancel and waits for it).
 */

#include "gallery.h"
#define LXA_P96_INLINES
#include <cybergraphx/cybergraphics.h>
#include <inline/cybergraphics.h>
#include <lxa_p96.h>

struct Library *P96Base = NULL;
struct Library *CyberGfxBase = NULL;

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "p96";
    ULONG id = 0;

    if (!gallery_open_libs())
        return 20;

    if (!strncmp(mode, "p96", 3))
    {
        struct TagItem t[8];
        int n = 0;
        P96Base = OpenLibrary((STRPTR)P96NAME, 2);
        if (!P96Base)
            return 20;
        if (!strcmp(mode, "p96"))
        {
            t[n].ti_Tag = P96MA_WindowTitle; t[n++].ti_Data = (ULONG)"Gallery P96 Mode";
        }
        else if (!strcmp(mode, "p96init"))
        {
            struct TagItem b[] = {
                {P96BIDTAG_NominalWidth, 640}, {P96BIDTAG_NominalHeight, 480},
                {P96BIDTAG_Depth, 16}, {TAG_DONE, 0}};
            ULONG init = p96BestModeIDTagList(b);
            printf("init=%08lx\n", (unsigned long)init);
            t[n].ti_Tag = P96MA_WindowTitle; t[n++].ti_Data = (ULONG)"Gallery P96 Init";
            t[n].ti_Tag = P96MA_DisplayID; t[n++].ti_Data = init;
            t[n].ti_Tag = P96MA_OKText; t[n++].ti_Data = (ULONG)"Use";
            t[n].ti_Tag = P96MA_CancelText; t[n++].ti_Data = (ULONG)"Abort";
            t[n].ti_Tag = P96MA_MinDepth; t[n++].ti_Data = 16;
        }
        t[n].ti_Tag = TAG_DONE;
        id = p96RequestModeIDTagList(t);
        CloseLibrary(P96Base);
    }
    else
    {
        struct TagItem t[4];
        int n = 0;
        CyberGfxBase = OpenLibrary((STRPTR)"cybergraphics.library", 40);
        if (!CyberGfxBase)
            return 20;
        if (!strcmp(mode, "cgx"))
        {
            t[n].ti_Tag = CYBRMREQ_WinTitle; t[n++].ti_Data = (ULONG)"Gallery CGX Mode";
        }
        t[n].ti_Tag = TAG_DONE;
        id = CModeRequestTagList(NULL, t);
        CloseLibrary(CyberGfxBase);
    }
    printf("id=%08lx\n", (unsigned long)id);
    printf("DONE\n");
    gallery_close_libs();
    return 0;
}
