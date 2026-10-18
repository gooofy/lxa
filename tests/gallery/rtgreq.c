/*
 * GalleryRtgReq - the screen mode requesters of the RTG libraries
 * (Phase 246, reference profile rtg).
 *
 *   GalleryRtgReq p96    p96RequestModeIDTagList, title "Gallery P96 Mode"
 *   GalleryRtgReq cgx    CModeRequestTagList, title "Gallery CGX Mode"
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

    if (!strcmp(mode, "p96"))
    {
        struct TagItem t[] = {
            {P96MA_WindowTitle, (ULONG)"Gallery P96 Mode"},
            {TAG_DONE, 0}};
        P96Base = OpenLibrary((STRPTR)P96NAME, 2);
        if (!P96Base)
            return 20;
        id = p96RequestModeIDTagList(t);
        CloseLibrary(P96Base);
    }
    else
    {
        struct TagItem t[] = {
            {CYBRMREQ_WinTitle, (ULONG)"Gallery CGX Mode"},
            {TAG_DONE, 0}};
        CyberGfxBase = OpenLibrary((STRPTR)"cybergraphics.library", 40);
        if (!CyberGfxBase)
            return 20;
        id = CModeRequestTagList(NULL, t);
        CloseLibrary(CyberGfxBase);
    }
    printf("id=%08lx\n", (unsigned long)id);
    printf("DONE\n");
    gallery_close_libs();
    return 0;
}
