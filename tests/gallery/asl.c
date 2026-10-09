/*
 * GalleryAsl - asl.library requesters (Phase 238).
 *
 *   GalleryAsl sm        screen-mode requester, no options (title, list, OK/Cancel)
 *   GalleryAsl smfull    screen-mode requester with every ASLSM_Do* gadget,
 *                        initial mode PAL:High Res 640x256x3
 *   GalleryAsl sminfo    as smfull with the property window opened
 *   GalleryAsl smfilter  screen-mode requester restricted by ASLSM_Min/Max*
 *   GalleryAsl file      file requester (SYS:, pattern gadget)
 *   GalleryAsl font      font requester with style/pens/size
 *
 * The requester is opened on the Workbench screen; when it returns, every
 * result field is printed on stdout (a scenario clicks OK/Cancel and waits
 * for "DONE").
 */

#include "gallery.h"
#include <libraries/asl.h>
#include <graphics/modeid.h>
#include <clib/asl_protos.h>

struct Library *AslBase;

static void pr_sm(struct ScreenModeRequester *sm)
{
    printf("sm_DisplayID=%08lx\n", (unsigned long)sm->sm_DisplayID);
    printf("sm_DisplayWidth=%lu sm_DisplayHeight=%lu sm_DisplayDepth=%u\n",
           (unsigned long)sm->sm_DisplayWidth, (unsigned long)sm->sm_DisplayHeight,
           (unsigned)sm->sm_DisplayDepth);
    printf("sm_OverscanType=%u sm_AutoScroll=%d\n", (unsigned)sm->sm_OverscanType, (int)sm->sm_AutoScroll);
    printf("sm_BitMapWidth=%lu sm_BitMapHeight=%lu\n",
           (unsigned long)sm->sm_BitMapWidth, (unsigned long)sm->sm_BitMapHeight);
    printf("sm_Left/Top/Width/Height=%d,%d,%d,%d\n", sm->sm_LeftEdge, sm->sm_TopEdge, sm->sm_Width, sm->sm_Height);
    printf("sm_InfoOpened=%d sm_Info=%d,%d,%d,%d\n", (int)sm->sm_InfoOpened, sm->sm_InfoLeftEdge,
           sm->sm_InfoTopEdge, sm->sm_InfoWidth, sm->sm_InfoHeight);
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "sm";
    APTR req = NULL;
    BOOL ok = FALSE;

    if (!gallery_open_libs())
        return 20;
    AslBase = OpenLibrary((STRPTR)"asl.library", 38);
    if (!AslBase)
        return 20;

    if (!strcmp(mode, "sm"))
    {
        /* GalleryAsl sm [W|H|D|O|A|I ...] [w<n>] [h<n>] [x<n>] [y<n>] [i<hexid>]
         * [t<title>]: the Do* gadgets (Width, Height, Depth, Overscan,
         * AutoScroll), the property window (I), window geometry, the
         * initial display ID and the title */
        struct TagItem tags[24];
        int n = 0, i;
        for (i = 2; i < argc; i++)
        {
            const char *a = argv[i];
            switch (a[0])
            {
                case 'W': tags[n].ti_Tag = ASLSM_DoWidth; tags[n++].ti_Data = TRUE; break;
                case 'H': tags[n].ti_Tag = ASLSM_DoHeight; tags[n++].ti_Data = TRUE; break;
                case 'D': tags[n].ti_Tag = ASLSM_DoDepth; tags[n++].ti_Data = TRUE; break;
                case 'O': tags[n].ti_Tag = ASLSM_DoOverscanType; tags[n++].ti_Data = TRUE; break;
                case 'A': tags[n].ti_Tag = ASLSM_DoAutoScroll; tags[n++].ti_Data = TRUE; break;
                case 'I': tags[n].ti_Tag = ASLSM_InitialInfoOpened; tags[n++].ti_Data = TRUE; break;
                case 'w': tags[n].ti_Tag = ASLSM_InitialWidth; tags[n++].ti_Data = atol(a + 1); break;
                case 'h': tags[n].ti_Tag = ASLSM_InitialHeight; tags[n++].ti_Data = atol(a + 1); break;
                case 'x': tags[n].ti_Tag = ASLSM_InitialLeftEdge; tags[n++].ti_Data = atol(a + 1); break;
                case 'y': tags[n].ti_Tag = ASLSM_InitialTopEdge; tags[n++].ti_Data = atol(a + 1); break;
                case 'i': tags[n].ti_Tag = ASLSM_InitialDisplayID; tags[n++].ti_Data = strtoul(a + 1, NULL, 16); break;
                case 'd': tags[n].ti_Tag = ASLSM_InitialDisplayDepth; tags[n++].ti_Data = atol(a + 1); break;
                case 't': tags[n].ti_Tag = ASLSM_TitleText; tags[n++].ti_Data = (ULONG)(a + 1); break;
                case 'f': tags[n].ti_Tag = ASLSM_PropertyFlags; tags[n++].ti_Data = strtoul(a + 1, NULL, 16); break;
                case 'm': tags[n].ti_Tag = ASLSM_PropertyMask; tags[n++].ti_Data = strtoul(a + 1, NULL, 16); break;
                case 'o': tags[n].ti_Tag = ASLSM_InitialOverscanType; tags[n++].ti_Data = atol(a + 1); break;            }
        }
        tags[n].ti_Tag = TAG_END;
        req = AllocAslRequest(ASL_ScreenModeRequest, tags);
        if (req)
            ok = AslRequestTags(req, TAG_END);
    }
    else if (!strcmp(mode, "smfull") || !strcmp(mode, "sminfo"))
    {
        req = AllocAslRequestTags(ASL_ScreenModeRequest,
            ASLSM_TitleText, (ULONG)"Gallery Screen Mode",
            ASLSM_InitialDisplayID, PAL_MONITOR_ID | HIRES_KEY,
            ASLSM_InitialDisplayWidth, 640,
            ASLSM_InitialDisplayHeight, 256,
            ASLSM_InitialDisplayDepth, 3,
            ASLSM_InitialOverscanType, OSCAN_TEXT,
            ASLSM_InitialAutoScroll, TRUE,
            ASLSM_InitialInfoOpened, !strcmp(mode, "sminfo"),
            ASLSM_DoWidth, TRUE,
            ASLSM_DoHeight, TRUE,
            ASLSM_DoDepth, TRUE,
            ASLSM_DoOverscanType, TRUE,
            ASLSM_DoAutoScroll, TRUE,
            TAG_END);
        if (req)
            ok = AslRequestTags(req, TAG_END);
    }
    else if (!strcmp(mode, "smfilter"))
    {
        req = AllocAslRequestTags(ASL_ScreenModeRequest,
            ASLSM_TitleText, (ULONG)"Gallery Screen Mode",
            ASLSM_MinWidth, 600,
            ASLSM_MinHeight, 400,
            ASLSM_MaxDepth, 4,
            TAG_END);
        if (req)
            ok = AslRequestTags(req, TAG_END);
    }
    else if (!strcmp(mode, "file"))
    {
        req = AllocAslRequestTags(ASL_FileRequest,
            ASLFR_TitleText, (ULONG)"Gallery File",
            ASLFR_InitialDrawer, (ULONG)"SYS:",
            ASLFR_InitialFile, (ULONG)"Startup-Sequence",
            ASLFR_DoPatterns, TRUE,
            TAG_END);
        if (req)
            ok = AslRequestTags(req, TAG_END);
    }
    else if (!strcmp(mode, "font"))
    {
        req = AllocAslRequestTags(ASL_FontRequest,
            ASLFO_TitleText, (ULONG)"Gallery Font",
            ASLFO_InitialName, (ULONG)"topaz.font",
            ASLFO_InitialSize, 8,
            ASLFO_DoStyle, TRUE,
            ASLFO_DoFrontPen, TRUE,
            ASLFO_DoBackPen, TRUE,
            ASLFO_DoDrawMode, TRUE,
            TAG_END);
        if (req)
            ok = AslRequestTags(req, TAG_END);
    }

    printf("result=%d\n", (int)ok);
    if (req && mode[0] == 's')
        pr_sm((struct ScreenModeRequester *)req);
    else if (req && !strcmp(mode, "file"))
    {
        struct FileRequester *fr = req;
        printf("fr_File=%s fr_Drawer=%s\n", fr->fr_File ? (char *)fr->fr_File : "(null)",
               fr->fr_Drawer ? (char *)fr->fr_Drawer : "(null)");
        printf("fr_Left/Top/Width/Height=%d,%d,%d,%d\n", fr->fr_LeftEdge, fr->fr_TopEdge, fr->fr_Width, fr->fr_Height);
    }
    else if (req && !strcmp(mode, "font"))
    {
        struct FontRequester *fo = req;
        printf("fo_Name=%s fo_YSize=%u fo_Style=%u fo_Flags=%u\n",
               fo->fo_Attr.ta_Name ? (char *)fo->fo_Attr.ta_Name : "(null)",
               (unsigned)fo->fo_Attr.ta_YSize, (unsigned)fo->fo_Attr.ta_Style, (unsigned)fo->fo_Attr.ta_Flags);
        printf("fo_FrontPen=%u fo_BackPen=%u fo_DrawMode=%u\n", (unsigned)fo->fo_FrontPen,
               (unsigned)fo->fo_BackPen, (unsigned)fo->fo_DrawMode);
        printf("fo_Left/Top/Width/Height=%d,%d,%d,%d\n", fo->fo_LeftEdge, fo->fo_TopEdge, fo->fo_Width, fo->fo_Height);
    }
    printf("DONE\n");
    fflush(stdout);
    if (req)
        FreeAslRequest(req);
    CloseLibrary(AslBase);
    gallery_close_libs();
    return 0;
}
