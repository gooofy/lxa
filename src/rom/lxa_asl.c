/*
 * lxa asl.library implementation
 *
 * Provides the ASL (AmigaOS Standard Library) requester API.
 * Implements basic file and font requesters using Intuition windows.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/exall.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>

#include <utility/tagitem.h>
#include <libraries/asl.h>

#include "util.h"

#define VERSION    39
#define REVISION   2
#define EXLIBNAME  "asl"
#define EXLIBVER   " 39.2 (2025/02/03)"

char __aligned _g_asl_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_asl_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_asl_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_asl_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct GfxBase *GfxBase;
extern struct IntuitionBase *IntuitionBase;

/* AslBase structure */
struct AslBase {
    struct Library lib;
    BPTR           SegList;
};

/* Internal FileRequester structure - maps to public FileRequester structure
 * NOTE: fr_Type MUST be at offset 0 (overlaps fr_Reserved0) so that
 * AslRequest() can determine the requester type regardless of which
 * struct type the pointer actually refers to. */
struct LXAFileRequester {
    ULONG         fr_Type;          /* Requester type (at offset 0) */
    STRPTR        fr_File;          /* Contents of File gadget on exit */
    STRPTR        fr_Drawer;        /* Contents of Drawer gadget on exit */
    UBYTE         fr_Reserved1[10];
    WORD          fr_LeftEdge;      /* Coordinates of requester on exit */
    WORD          fr_TopEdge;
    WORD          fr_Width;
    WORD          fr_Height;
    UBYTE         fr_Reserved2[2];
    LONG          fr_NumArgs;       /* Number of files selected */
    struct WBArg *fr_ArgList;       /* List of files selected */
    APTR          fr_UserData;      /* You can store your own data here */
    UBYTE         fr_Reserved3[8];
    STRPTR        fr_Pattern;       /* Contents of Pattern gadget on exit */
    
    /* Private fields */
    STRPTR        fr_Title;         /* Window title */
    STRPTR        fr_OkText;        /* OK button text */
    STRPTR        fr_CancelText;    /* Cancel button text */
    struct Window *fr_Window;       /* Parent window */
    struct Screen *fr_Screen;       /* Screen to open on */
    BOOL          fr_DoSaveMode;    /* Save mode? */
    BOOL          fr_DoPatterns;    /* Show pattern gadget? */
    BOOL          fr_DrawersOnly;   /* Only show drawers? */
};

/* Internal FontRequester structure
 * Layout MUST match NDK FontRequester: fo_Reserved0[8] then fo_Attr at offset 8.
 * We store fo_Type inside the first 4 bytes of the reserved area so that
 * AslRequest() can determine the requester type from a generic pointer. */
struct LXAFontRequester {
    ULONG         fo_Type;          /* Requester type (at offset 0, inside reserved area) */
    UBYTE         fo_Reserved0b[4]; /* Remaining 4 bytes of fo_Reserved0[8] */
    struct TextAttr fo_Attr;        /* Font attributes on exit (offset 8, matches NDK) */
    UBYTE         fo_FrontPen;      /* Returned front pen */
    UBYTE         fo_BackPen;       /* Returned back pen */
    UBYTE         fo_DrawMode;      /* Returned draw mode */
    UBYTE         fo_Pad;
    APTR          fo_UserData;
    WORD          fo_LeftEdge;
    WORD          fo_TopEdge;
    WORD          fo_Width;
    WORD          fo_Height;
    
    /* Private fields (after public NDK-compatible area) */
    STRPTR        fo_Title;
    STRPTR        fo_OkText;
    STRPTR        fo_CancelText;
    struct Window *fo_Window;
    struct Screen *fo_Screen;
    UWORD         fo_MinHeight;     /* Minimum font height to display */
    UWORD         fo_MaxHeight;     /* Maximum font height to display */
    STRPTR        fo_InitialName;   /* Initial font name from tags */
    UWORD         fo_InitialSize;   /* Initial font size from tags */
};

/* Gadget IDs - File Requester */
#define GID_DRAWER_STR   1
#define GID_FILE_STR     2
#define GID_PATTERN_STR  3
#define GID_OK           4
#define GID_CANCEL       5
#define GID_PARENT       6
#define GID_VOLUMES      7
#define GID_LISTVIEW     8

/* Gadget IDs - Font Requester */
#define GID_FONT_OK      10
#define GID_FONT_CANCEL  11
#define GID_FONT_LIST    12

/* Maximum entries in file list */
#define MAX_FILE_ENTRIES 100
#define MAX_PATH_LEN     256
#define MAX_FILE_LEN     108

/*
 * Library init/open/close/expunge
 */

struct AslBase * __g_lxa_asl_InitLib ( register struct AslBase *aslb __asm("d0"),
                                       register BPTR           seglist __asm("a0"),
                                       register struct ExecBase *sysb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_asl: InitLib() called\n");
    aslb->SegList = seglist;
    return aslb;
}

BPTR __g_lxa_asl_ExpungeLib ( register struct AslBase *aslb __asm("a6") )
{
    return 0;
}

struct AslBase * __g_lxa_asl_OpenLib ( register struct AslBase *aslb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_asl: OpenLib() called, aslb=0x%08lx\n", (ULONG)aslb);
    aslb->lib.lib_OpenCnt++;
    aslb->lib.lib_Flags &= ~LIBF_DELEXP;
    return aslb;
}

BPTR __g_lxa_asl_CloseLib ( register struct AslBase *aslb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_asl: CloseLib() called, aslb=0x%08lx\n", (ULONG)aslb);
    aslb->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_asl_ExtFuncLib ( void )
{
    return 0;
}

/*
 * Helper functions
 */

/* Copy string to allocated buffer */
static STRPTR asl_strdup(CONST_STRPTR str)
{
    STRPTR copy;
    LONG len;
    
    if (!str)
        return NULL;
    
    len = 0;
    while (str[len]) len++;
    len++;
    
    copy = AllocMem(len, MEMF_PUBLIC);
    if (copy) {
        LONG i;
        for (i = 0; i < len; i++)
            copy[i] = str[i];
    }
    return copy;
}

/* Free string allocated with asl_strdup */
static void asl_strfree(STRPTR str)
{
    if (str) {
        LONG len = 0;
        while (str[len]) len++;
        FreeMem(str, len + 1);
    }
}

/* Parse tags for file requester */
static void parse_fr_tags(struct LXAFileRequester *fr, struct TagItem *tagList)
{
    struct TagItem *tag;
    
    if (!tagList)
        return;
    
    for (tag = tagList; tag->ti_Tag != TAG_DONE; tag++) {
        if (tag->ti_Tag == TAG_SKIP) {
            tag += tag->ti_Data;
            continue;
        }
        if (tag->ti_Tag == TAG_IGNORE)
            continue;
        if (tag->ti_Tag == TAG_MORE) {
            tag = (struct TagItem *)tag->ti_Data;
            if (!tag) break;
            tag--;
            continue;
        }
        
        switch (tag->ti_Tag) {
            case ASLFR_TitleText:
                fr->fr_Title = (STRPTR)tag->ti_Data;
                break;
            case ASLFR_PositiveText:
                fr->fr_OkText = (STRPTR)tag->ti_Data;
                break;
            case ASLFR_NegativeText:
                fr->fr_CancelText = (STRPTR)tag->ti_Data;
                break;
            case ASLFR_InitialLeftEdge:
                fr->fr_LeftEdge = (WORD)tag->ti_Data;
                break;
            case ASLFR_InitialTopEdge:
                fr->fr_TopEdge = (WORD)tag->ti_Data;
                break;
            case ASLFR_InitialWidth:
                fr->fr_Width = (WORD)tag->ti_Data;
                break;
            case ASLFR_InitialHeight:
                fr->fr_Height = (WORD)tag->ti_Data;
                break;
            case ASLFR_InitialFile:
                if (fr->fr_File) asl_strfree(fr->fr_File);
                fr->fr_File = asl_strdup((CONST_STRPTR)tag->ti_Data);
                break;
            case ASLFR_InitialDrawer:
                if (fr->fr_Drawer) asl_strfree(fr->fr_Drawer);
                fr->fr_Drawer = asl_strdup((CONST_STRPTR)tag->ti_Data);
                break;
            case ASLFR_InitialPattern:
                if (fr->fr_Pattern) asl_strfree(fr->fr_Pattern);
                fr->fr_Pattern = asl_strdup((CONST_STRPTR)tag->ti_Data);
                break;
            case ASLFR_Window:
                fr->fr_Window = (struct Window *)tag->ti_Data;
                break;
            case ASLFR_Screen:
                fr->fr_Screen = (struct Screen *)tag->ti_Data;
                break;
            case ASLFR_DoSaveMode:
                fr->fr_DoSaveMode = (BOOL)tag->ti_Data;
                break;
            case ASLFR_DoPatterns:
                fr->fr_DoPatterns = (BOOL)tag->ti_Data;
                break;
            case ASLFR_DrawersOnly:
                fr->fr_DrawersOnly = (BOOL)tag->ti_Data;
                break;
            case ASLFR_UserData:
                fr->fr_UserData = (APTR)tag->ti_Data;
                break;
        }
    }
}

/* Parse tags for font requester */
static void parse_fo_tags(struct LXAFontRequester *fo, struct TagItem *tagList)
{
    struct TagItem *tag;
    
    if (!tagList)
        return;
    
    for (tag = tagList; tag->ti_Tag != TAG_DONE; tag++)
    {
        if (tag->ti_Tag == TAG_SKIP)
        {
            tag += tag->ti_Data;
            continue;
        }
        if (tag->ti_Tag == TAG_IGNORE)
            continue;
        if (tag->ti_Tag == TAG_MORE)
        {
            tag = (struct TagItem *)tag->ti_Data;
            if (!tag) break;
            tag--;
            continue;
        }
        
        switch (tag->ti_Tag)
        {
            case ASLFO_TitleText:
                fo->fo_Title = (STRPTR)tag->ti_Data;
                break;
            case ASLFO_PositiveText:
                fo->fo_OkText = (STRPTR)tag->ti_Data;
                break;
            case ASLFO_NegativeText:
                fo->fo_CancelText = (STRPTR)tag->ti_Data;
                break;
            case ASLFO_InitialLeftEdge:
                fo->fo_LeftEdge = (WORD)tag->ti_Data;
                break;
            case ASLFO_InitialTopEdge:
                fo->fo_TopEdge = (WORD)tag->ti_Data;
                break;
            case ASLFO_InitialWidth:
                fo->fo_Width = (WORD)tag->ti_Data;
                break;
            case ASLFO_InitialHeight:
                fo->fo_Height = (WORD)tag->ti_Data;
                break;
            case ASLFO_InitialName:
                fo->fo_InitialName = (STRPTR)tag->ti_Data;
                break;
            case ASLFO_InitialSize:
                fo->fo_InitialSize = (UWORD)tag->ti_Data;
                break;
            case ASLFO_MinHeight:
                fo->fo_MinHeight = (UWORD)tag->ti_Data;
                break;
            case ASLFO_MaxHeight:
                fo->fo_MaxHeight = (UWORD)tag->ti_Data;
                break;
            case ASLFO_Window:
                fo->fo_Window = (struct Window *)tag->ti_Data;
                break;
            case ASLFO_Screen:
                fo->fo_Screen = (struct Screen *)tag->ti_Data;
                break;
            case ASLFO_UserData:
                fo->fo_UserData = (APTR)tag->ti_Data;
                break;
        }
    }
}

/* Display the file requester window and handle interaction */
static BOOL do_file_request(struct LXAFileRequester *fr)
{
    struct NewWindow nw;
    struct Window *win;
    struct Screen *scr;
    struct IntuiMessage *imsg;
    struct Gadget *gadList = NULL, *gad, *lastGad = NULL;
    struct StringInfo *drawerSI = NULL, *fileSI = NULL;
    struct RastPort *rp;
    BOOL result = FALSE;
    BOOL done = FALSE;
    UBYTE drawerBuf[MAX_PATH_LEN];
    UBYTE fileBuf[MAX_FILE_LEN];
    WORD winWidth, winHeight;
    WORD btnWidth = 60, btnHeight = 14;
    WORD strHeight = 14;
    WORD margin = 8;
    WORD listTop, listHeight;
    BPTR lock;
    struct FileInfoBlock *fib = NULL;
    WORD fileCount = 0;
    WORD entryY;
    
    DPRINTF(LOG_DEBUG, "_asl: do_file_request() drawer='%s' file='%s'\n",
            fr->fr_Drawer ? (char*)fr->fr_Drawer : "(null)",
            fr->fr_File ? (char*)fr->fr_File : "(null)");
    
    /* Get screen to open on */
    if (fr->fr_Window) {
        scr = fr->fr_Window->WScreen;
    } else if (fr->fr_Screen) {
        scr = fr->fr_Screen;
    } else {
        /* Use Workbench screen */
        scr = NULL;  /* Will use default public screen */
    }
    
    /* Set defaults */
    winWidth = fr->fr_Width > 0 ? fr->fr_Width : 300;
    winHeight = fr->fr_Height > 0 ? fr->fr_Height : 200;
    
    /* Determine window position and clamp to screen bounds */
    {
        WORD scrWidth = 640, scrHeight = 256;
        WORD winLeft, winTop;
        
        if (scr)
        {
            scrWidth = scr->Width;
            scrHeight = scr->Height;
        }
        
        winLeft = fr->fr_LeftEdge >= 0 ? fr->fr_LeftEdge : 50;
        winTop = fr->fr_TopEdge >= 0 ? fr->fr_TopEdge : 30;
        
        if (winHeight > scrHeight - winTop)
            winHeight = scrHeight - winTop;
        if (winWidth > scrWidth - winLeft)
            winWidth = scrWidth - winLeft;
        if (winHeight < 100)
            winHeight = 100;
        if (winWidth < 200)
            winWidth = 200;
    }
    
    /* Initialize buffers */
    if (fr->fr_Drawer) {
        LONG i;
        for (i = 0; i < MAX_PATH_LEN - 1 && fr->fr_Drawer[i]; i++)
            drawerBuf[i] = fr->fr_Drawer[i];
        drawerBuf[i] = 0;
    } else {
        drawerBuf[0] = 0;
    }
    
    if (fr->fr_File) {
        LONG i;
        for (i = 0; i < MAX_FILE_LEN - 1 && fr->fr_File[i]; i++)
            fileBuf[i] = fr->fr_File[i];
        fileBuf[i] = 0;
    } else {
        fileBuf[0] = 0;
    }
    
    /* Allocate StringInfo structures */
    drawerSI = AllocMem(sizeof(struct StringInfo), MEMF_PUBLIC | MEMF_CLEAR);
    fileSI = AllocMem(sizeof(struct StringInfo), MEMF_PUBLIC | MEMF_CLEAR);
    if (!drawerSI || !fileSI)
        goto cleanup;
    
    drawerSI->Buffer = drawerBuf;
    drawerSI->MaxChars = MAX_PATH_LEN;
    drawerSI->NumChars = 0;
    while (drawerBuf[drawerSI->NumChars]) drawerSI->NumChars++;
    
    fileSI->Buffer = fileBuf;
    fileSI->MaxChars = MAX_FILE_LEN;
    fileSI->NumChars = 0;
    while (fileBuf[fileSI->NumChars]) fileSI->NumChars++;
    
    /* Create gadgets */
    /* Drawer string gadget */
    gad = AllocMem(sizeof(struct Gadget), MEMF_PUBLIC | MEMF_CLEAR);
    if (!gad) goto cleanup;
    gadList = gad;
    lastGad = gad;
    
    gad->LeftEdge = margin + 60;
    gad->TopEdge = 20;
    gad->Width = winWidth - margin*2 - 60;
    gad->Height = strHeight;
    gad->GadgetID = GID_DRAWER_STR;
    gad->GadgetType = GTYP_STRGADGET;
    gad->Flags = GFLG_GADGHCOMP;
    gad->Activation = GACT_RELVERIFY;
    gad->SpecialInfo = drawerSI;
    
    /* File string gadget */
    gad = AllocMem(sizeof(struct Gadget), MEMF_PUBLIC | MEMF_CLEAR);
    if (!gad) goto cleanup;
    lastGad->NextGadget = gad;
    lastGad = gad;
    
    gad->LeftEdge = margin + 60;
    gad->TopEdge = winHeight - 20 - strHeight - btnHeight - margin;
    gad->Width = winWidth - margin*2 - 60;
    gad->Height = strHeight;
    gad->GadgetID = GID_FILE_STR;
    gad->GadgetType = GTYP_STRGADGET;
    gad->Flags = GFLG_GADGHCOMP;
    gad->Activation = GACT_RELVERIFY;
    gad->SpecialInfo = fileSI;
    
    /* OK button */
    gad = AllocMem(sizeof(struct Gadget), MEMF_PUBLIC | MEMF_CLEAR);
    if (!gad) goto cleanup;
    lastGad->NextGadget = gad;
    lastGad = gad;
    
    gad->LeftEdge = margin;
    gad->TopEdge = winHeight - 20 - btnHeight;
    gad->Width = btnWidth;
    gad->Height = btnHeight;
    gad->GadgetID = GID_OK;
    gad->GadgetType = GTYP_BOOLGADGET;
    gad->Flags = GFLG_GADGHCOMP;
    gad->Activation = GACT_RELVERIFY;
    
    /* Cancel button */
    gad = AllocMem(sizeof(struct Gadget), MEMF_PUBLIC | MEMF_CLEAR);
    if (!gad) goto cleanup;
    lastGad->NextGadget = gad;
    lastGad = gad;
    
    gad->LeftEdge = winWidth - margin - btnWidth;
    gad->TopEdge = winHeight - 20 - btnHeight;
    gad->Width = btnWidth;
    gad->Height = btnHeight;
    gad->GadgetID = GID_CANCEL;
    gad->GadgetType = GTYP_BOOLGADGET;
    gad->Flags = GFLG_GADGHCOMP;
    gad->Activation = GACT_RELVERIFY;
    
    /* Open window */
    nw.LeftEdge = fr->fr_LeftEdge >= 0 ? fr->fr_LeftEdge : 50;
    nw.TopEdge = fr->fr_TopEdge >= 0 ? fr->fr_TopEdge : 30;
    nw.Width = winWidth;
    nw.Height = winHeight;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_GADGETUP | IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    nw.FirstGadget = gadList;
    nw.CheckMark = NULL;
    nw.Title = fr->fr_Title ? fr->fr_Title : (UBYTE *)"Select File";
    nw.Screen = scr;
    nw.BitMap = NULL;
    nw.MinWidth = 200;
    nw.MinHeight = 100;
    nw.MaxWidth = 640;
    nw.MaxHeight = 480;
    nw.Type = scr ? CUSTOMSCREEN : WBENCHSCREEN;
    
    win = OpenWindow(&nw);
    if (!win) {
        DPRINTF(LOG_ERROR, "_asl: Failed to open requester window\n");
        goto cleanup;
    }
    
    rp = win->RPort;
    
    /* Draw labels */
    SetAPen(rp, 1);
    Move(rp, margin, 20 + 10);
    Text(rp, (STRPTR)"Drawer:", 7);
    Move(rp, margin, winHeight - 20 - strHeight - btnHeight - margin + 10);
    Text(rp, (STRPTR)"File:", 5);
    
    /* Draw button labels */
    Move(rp, margin + (btnWidth - 16)/2, winHeight - 20 - btnHeight + 10);
    Text(rp, fr->fr_OkText ? fr->fr_OkText : (STRPTR)"OK", fr->fr_OkText ? strlen((char*)fr->fr_OkText) : 2);
    Move(rp, winWidth - margin - btnWidth + (btnWidth - 48)/2, winHeight - 20 - btnHeight + 10);
    Text(rp, fr->fr_CancelText ? fr->fr_CancelText : (STRPTR)"Cancel", fr->fr_CancelText ? strlen((char*)fr->fr_CancelText) : 6);
    
    /* Draw file list area */
    listTop = 20 + strHeight + margin;
    listHeight = winHeight - listTop - strHeight - btnHeight - margin*2 - 20;
    
    SetAPen(rp, 2);
    Move(rp, margin, listTop);
    Draw(rp, margin, listTop + listHeight);
    Draw(rp, winWidth - margin, listTop + listHeight);
    SetAPen(rp, 1);
    Draw(rp, winWidth - margin, listTop);
    Draw(rp, margin, listTop);
    
    /* Read and display directory contents */
    fib = AllocMem(sizeof(struct FileInfoBlock), MEMF_PUBLIC);
    if (fib && drawerBuf[0]) {
        lock = Lock(drawerBuf, ACCESS_READ);
        if (lock) {
            if (Examine(lock, fib)) {
                entryY = listTop + 10;
                fileCount = 0;
                
                while (ExNext(lock, fib) && entryY < listTop + listHeight - 10 && fileCount < MAX_FILE_ENTRIES) {
                    /* Skip if drawers only and this is a file */
                    if (fr->fr_DrawersOnly && fib->fib_DirEntryType < 0)
                        continue;
                    
                    SetAPen(rp, 1);
                    Move(rp, margin + 4, entryY);
                    
                    /* Show directory indicator */
                    if (fib->fib_DirEntryType > 0) {
                        Text(rp, (STRPTR)"[", 1);
                    }
                    Text(rp, fib->fib_FileName, strlen((char*)fib->fib_FileName));
                    if (fib->fib_DirEntryType > 0) {
                        Text(rp, (STRPTR)"]", 1);
                    }
                    
                    entryY += 10;
                    fileCount++;
                }
            }
            UnLock(lock);
        }
    }
    
    /* Refresh gadgets */
    RefreshGList(gadList, win, NULL, -1);
    
    /* Event loop */
    while (!done) {
        WaitPort(win->UserPort);
        
        while ((imsg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
            ULONG msgClass = imsg->Class;
            struct Gadget *msgGadget = (struct Gadget *)imsg->IAddress;
            
            ReplyMsg((struct Message *)imsg);
            
            switch (msgClass) {
                case IDCMP_CLOSEWINDOW:
                    done = TRUE;
                    result = FALSE;
                    break;
                    
                case IDCMP_GADGETUP:
                    if (msgGadget) {
                        switch (msgGadget->GadgetID) {
                            case GID_OK:
                                /* Update requester with current values */
                                if (fr->fr_File) asl_strfree(fr->fr_File);
                                fr->fr_File = asl_strdup(fileBuf);
                                if (fr->fr_Drawer) asl_strfree(fr->fr_Drawer);
                                fr->fr_Drawer = asl_strdup(drawerBuf);
                                done = TRUE;
                                result = TRUE;
                                break;
                                
                            case GID_CANCEL:
                                done = TRUE;
                                result = FALSE;
                                break;
                                
                            case GID_DRAWER_STR:
                            case GID_FILE_STR:
                                /* String gadget updated - could refresh list here */
                                break;
                        }
                    }
                    break;
                    
                case IDCMP_REFRESHWINDOW:
                    BeginRefresh(win);
                    EndRefresh(win, TRUE);
                    break;
            }
        }
    }
    
    CloseWindow(win);
    win = NULL;
    
cleanup:
    /* Free gadgets */
    gad = gadList;
    while (gad) {
        struct Gadget *next = gad->NextGadget;
        FreeMem(gad, sizeof(struct Gadget));
        gad = next;
    }
    
    if (drawerSI) FreeMem(drawerSI, sizeof(struct StringInfo));
    if (fileSI) FreeMem(fileSI, sizeof(struct StringInfo));
    if (fib) FreeMem(fib, sizeof(struct FileInfoBlock));
    
    return result;
}

/* Display the font requester window and handle interaction */
static BOOL do_font_request(struct LXAFontRequester *fo)
{
    struct NewWindow nw;
    struct Window *win;
    struct Screen *scr;
    struct IntuiMessage *imsg;
    struct Gadget *gadList = NULL, *gad, *lastGad = NULL;
    struct RastPort *rp;
    BOOL result = FALSE;
    BOOL done = FALSE;
    WORD winWidth, winHeight;
    WORD btnWidth = 60, btnHeight = 14;
    WORD margin = 8;
    WORD listTop, listHeight;
    WORD entryY;
    WORD fontCount = 0;
    
    DPRINTF(LOG_DEBUG, "_asl: do_font_request()\n");
    
    /* Get screen to open on */
    if (fo->fo_Window)
    {
        scr = fo->fo_Window->WScreen;
    }
    else if (fo->fo_Screen)
    {
        scr = fo->fo_Screen;
    }
    else
    {
        scr = NULL;
    }
    
    /* Set defaults */
    winWidth = fo->fo_Width > 0 ? fo->fo_Width : 300;
    winHeight = fo->fo_Height > 0 ? fo->fo_Height : 200;
    
    /* Determine window position and clamp to screen bounds */
    {
        WORD scrWidth = 640, scrHeight = 256;
        WORD winLeft, winTop;
        
        if (scr)
        {
            scrWidth = scr->Width;
            scrHeight = scr->Height;
        }
        
        winLeft = fo->fo_LeftEdge >= 0 ? fo->fo_LeftEdge : 50;
        winTop = fo->fo_TopEdge >= 0 ? fo->fo_TopEdge : 30;
        
        if (winHeight > scrHeight - winTop)
            winHeight = scrHeight - winTop;
        if (winWidth > scrWidth - winLeft)
            winWidth = scrWidth - winLeft;
        if (winHeight < 100)
            winHeight = 100;
        if (winWidth < 200)
            winWidth = 200;
    }
    
    /* Create OK button */
    gad = AllocMem(sizeof(struct Gadget), MEMF_PUBLIC | MEMF_CLEAR);
    if (!gad) goto cleanup;
    gadList = gad;
    lastGad = gad;
    
    gad->LeftEdge = margin;
    gad->TopEdge = winHeight - 20 - btnHeight;
    gad->Width = btnWidth;
    gad->Height = btnHeight;
    gad->GadgetID = GID_FONT_OK;
    gad->GadgetType = GTYP_BOOLGADGET;
    gad->Flags = GFLG_GADGHCOMP;
    gad->Activation = GACT_RELVERIFY;
    
    /* Create Cancel button */
    gad = AllocMem(sizeof(struct Gadget), MEMF_PUBLIC | MEMF_CLEAR);
    if (!gad) goto cleanup;
    lastGad->NextGadget = gad;
    lastGad = gad;
    
    gad->LeftEdge = winWidth - margin - btnWidth;
    gad->TopEdge = winHeight - 20 - btnHeight;
    gad->Width = btnWidth;
    gad->Height = btnHeight;
    gad->GadgetID = GID_FONT_CANCEL;
    gad->GadgetType = GTYP_BOOLGADGET;
    gad->Flags = GFLG_GADGHCOMP;
    gad->Activation = GACT_RELVERIFY;
    
    /* Open window */
    nw.LeftEdge = fo->fo_LeftEdge >= 0 ? fo->fo_LeftEdge : 50;
    nw.TopEdge = fo->fo_TopEdge >= 0 ? fo->fo_TopEdge : 30;
    nw.Width = winWidth;
    nw.Height = winHeight;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_GADGETUP | IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    nw.FirstGadget = gadList;
    nw.CheckMark = NULL;
    nw.Title = fo->fo_Title ? fo->fo_Title : (UBYTE *)"Select Font";
    nw.Screen = scr;
    nw.BitMap = NULL;
    nw.MinWidth = 200;
    nw.MinHeight = 100;
    nw.MaxWidth = 640;
    nw.MaxHeight = 480;
    nw.Type = scr ? CUSTOMSCREEN : WBENCHSCREEN;
    
    win = OpenWindow(&nw);
    if (!win)
    {
        DPRINTF(LOG_ERROR, "_asl: Failed to open font requester window\n");
        goto cleanup;
    }
    
    rp = win->RPort;
    
    /* Draw button labels */
    SetAPen(rp, 1);
    {
        STRPTR okText = fo->fo_OkText ? fo->fo_OkText : (STRPTR)"OK";
        STRPTR cancelText = fo->fo_CancelText ? fo->fo_CancelText : (STRPTR)"Cancel";
        LONG okLen = 0, cancelLen = 0;
        
        while (okText[okLen]) okLen++;
        while (cancelText[cancelLen]) cancelLen++;
        
        Move(rp, margin + (btnWidth - okLen * 8) / 2, winHeight - 20 - btnHeight + 10);
        Text(rp, okText, okLen);
        Move(rp, winWidth - margin - btnWidth + (btnWidth - cancelLen * 8) / 2, winHeight - 20 - btnHeight + 10);
        Text(rp, cancelText, cancelLen);
    }
    
    /* Draw font list area */
    listTop = 20 + margin;
    listHeight = winHeight - listTop - btnHeight - margin * 2 - 20;
    
    SetAPen(rp, 2);
    Move(rp, margin, listTop);
    Draw(rp, margin, listTop + listHeight);
    Draw(rp, winWidth - margin, listTop + listHeight);
    SetAPen(rp, 1);
    Draw(rp, winWidth - margin, listTop);
    Draw(rp, margin, listTop);
    
    /* Display available fonts */
    {
        STRPTR fontName = (STRPTR)"topaz.font";
        WORD fontSize = 8;
        LONG nameLen = 10;  /* strlen("topaz.font") */
        
        entryY = listTop + 10;
        fontCount = 0;
        
        /* Display the ROM font */
        if (entryY + 10 < listTop + listHeight)
        {
            SetAPen(rp, 1);
            Move(rp, margin + 4, entryY);
            Text(rp, fontName, nameLen);
            
            /* Display size next to name */
            {
                UBYTE sizeBuf[8];
                WORD sz = fontSize;
                WORD pos = 0;
                
                if (sz >= 10)
                {
                    sizeBuf[pos++] = '0' + (sz / 10);
                }
                sizeBuf[pos++] = '0' + (sz % 10);
                sizeBuf[pos] = 0;
                
                Move(rp, winWidth - margin - 30, entryY);
                Text(rp, sizeBuf, pos);
            }
            
            fontCount++;
            entryY += 10;
        }
    }
    
    /* Draw "Font:" label above the list */
    SetAPen(rp, 1);
    Move(rp, margin, listTop - 2);
    Text(rp, (STRPTR)"Font:", 5);
    
    /* Draw current selection info */
    {
        STRPTR curName = fo->fo_InitialName ? fo->fo_InitialName : (STRPTR)"topaz.font";
        LONG curLen = 0;
        
        while (curName[curLen]) curLen++;
        
        Move(rp, margin + 48, listTop - 2);
        Text(rp, curName, curLen);
    }
    
    /* Event loop */
    while (!done)
    {
        WaitPort(win->UserPort);
        
        while ((imsg = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL)
        {
            ULONG class_id = imsg->Class;
            struct Gadget *igad = (struct Gadget *)imsg->IAddress;
            
            ReplyMsg((struct Message *)imsg);
            
            switch (class_id)
            {
                case IDCMP_GADGETUP:
                    if (igad->GadgetID == GID_FONT_OK)
                    {
                        /* Set the output font attributes */
                        fo->fo_Attr.ta_Name = asl_strdup(
                            fo->fo_InitialName ? fo->fo_InitialName : (CONST_STRPTR)"topaz.font");
                        fo->fo_Attr.ta_YSize = fo->fo_InitialSize > 0 ? fo->fo_InitialSize : 8;
                        fo->fo_Attr.ta_Style = 0;  /* FS_NORMAL */
                        fo->fo_Attr.ta_Flags = FPF_ROMFONT | FPF_DESIGNED;
                        result = TRUE;
                        done = TRUE;
                    }
                    else if (igad->GadgetID == GID_FONT_CANCEL)
                    {
                        result = FALSE;
                        done = TRUE;
                    }
                    break;
                    
                case IDCMP_CLOSEWINDOW:
                    result = FALSE;
                    done = TRUE;
                    break;
                    
                case IDCMP_REFRESHWINDOW:
                    BeginRefresh(win);
                    EndRefresh(win, TRUE);
                    break;
            }
        }
    }
    
    /* Store window position/size for next invocation */
    fo->fo_LeftEdge = win->LeftEdge;
    fo->fo_TopEdge = win->TopEdge;
    fo->fo_Width = win->Width;
    fo->fo_Height = win->Height;
    
    CloseWindow(win);
    win = NULL;
    
cleanup:
    /* Free gadgets */
    gad = gadList;
    while (gad)
    {
        struct Gadget *next = gad->NextGadget;
        FreeMem(gad, sizeof(struct Gadget));
        gad = next;
    }
    
    return result;
}

/*
 * ASL Functions (V36+)
 */

/* AllocFileRequest - Obsolete, use AllocAslRequest instead */
APTR _asl_AllocFileRequest ( register struct AslBase *AslBase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_asl: AllocFileRequest() (obsolete) -> AllocAslRequest(ASL_FileRequest)\n");
    
    struct LXAFileRequester *req = AllocMem(sizeof(struct LXAFileRequester), MEMF_CLEAR | MEMF_PUBLIC);
    if (req) {
        req->fr_Type = ASL_FileRequest;
        req->fr_Width = 300;
        req->fr_Height = 200;
    }
    return req;
}

/* FreeFileRequest - Obsolete, use FreeAslRequest instead */
void _asl_FreeFileRequest ( register struct AslBase *AslBase __asm("a6"),
                            register APTR fileReq __asm("a0") )
{
    struct LXAFileRequester *fr = (struct LXAFileRequester *)fileReq;
    
    DPRINTF (LOG_DEBUG, "_asl: FreeFileRequest() fileReq=0x%08lx\n", (ULONG)fileReq);
    
    if (fr) {
        if (fr->fr_File) asl_strfree(fr->fr_File);
        if (fr->fr_Drawer) asl_strfree(fr->fr_Drawer);
        if (fr->fr_Pattern) asl_strfree(fr->fr_Pattern);
        FreeMem(fr, sizeof(struct LXAFileRequester));
    }
}

/* RequestFile - Obsolete, use AslRequest instead */
BOOL _asl_RequestFile ( register struct AslBase *AslBase __asm("a6"),
                        register APTR fileReq __asm("a0") )
{
    struct LXAFileRequester *fr = (struct LXAFileRequester *)fileReq;
    
    DPRINTF (LOG_DEBUG, "_asl: RequestFile() fileReq=0x%08lx\n", (ULONG)fileReq);
    
    if (!fr)
        return FALSE;
    
    return do_file_request(fr);
}

/* AllocAslRequest - Allocate an ASL requester */
APTR _asl_AllocAslRequest ( register struct AslBase *AslBase __asm("a6"),
                            register ULONG reqType __asm("d0"),
                            register struct TagItem *tagList __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_asl: AllocAslRequest() reqType=%ld\n", reqType);
    
    switch (reqType) {
        case ASL_FileRequest: {
            struct LXAFileRequester *fr = AllocMem(sizeof(struct LXAFileRequester), MEMF_CLEAR | MEMF_PUBLIC);
            if (fr) {
                fr->fr_Type = ASL_FileRequest;
                fr->fr_LeftEdge = -1;  /* Sentinel: not set by app */
                fr->fr_TopEdge = -1;   /* Sentinel: not set by app */
                fr->fr_Width = 300;
                fr->fr_Height = 200;
                parse_fr_tags(fr, tagList);
            }
            return fr;
        }
        
        case ASL_FontRequest: {
            struct LXAFontRequester *fo = AllocMem(sizeof(struct LXAFontRequester), MEMF_CLEAR | MEMF_PUBLIC);
            if (fo) {
                fo->fo_Type = ASL_FontRequest;
                fo->fo_LeftEdge = -1;   /* Sentinel: not set by app */
                fo->fo_TopEdge = -1;    /* Sentinel: not set by app */
                fo->fo_Width = 300;
                fo->fo_Height = 200;
                fo->fo_MinHeight = 5;   /* Default per RKRM */
                fo->fo_MaxHeight = 24;  /* Default per RKRM */
                fo->fo_InitialSize = 8; /* Default font size */
                parse_fo_tags(fo, tagList);
            }
            return fo;
        }
        
        case ASL_ScreenModeRequest:
        default:
            /* Not implemented yet */
            return NULL;
    }
}

/* FreeAslRequest - Free an ASL requester */
void _asl_FreeAslRequest ( register struct AslBase *AslBase __asm("a6"),
                           register APTR requester __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_asl: FreeAslRequest() requester=0x%08lx\n", (ULONG)requester);
    
    if (!requester)
        return;
    
    /* Check type by looking at first field after reserved bytes */
    struct LXAFileRequester *fr = (struct LXAFileRequester *)requester;
    
    if (fr->fr_Type == ASL_FileRequest) {
        if (fr->fr_File) asl_strfree(fr->fr_File);
        if (fr->fr_Drawer) asl_strfree(fr->fr_Drawer);
        if (fr->fr_Pattern) asl_strfree(fr->fr_Pattern);
        FreeMem(fr, sizeof(struct LXAFileRequester));
    } else if (fr->fr_Type == ASL_FontRequest) {
        struct LXAFontRequester *fo = (struct LXAFontRequester *)requester;
        if (fo->fo_Attr.ta_Name) asl_strfree(fo->fo_Attr.ta_Name);
        FreeMem(requester, sizeof(struct LXAFontRequester));
    } else {
        /* Unknown type - try to free as file requester size */
        FreeMem(requester, sizeof(struct LXAFileRequester));
    }
}

/* AslRequest - Display a requester */
BOOL _asl_AslRequest ( register struct AslBase *AslBase __asm("a6"),
                       register APTR requester __asm("a0"),
                       register struct TagItem *tagList __asm("a1") )
{
    struct LXAFileRequester *fr = (struct LXAFileRequester *)requester;
    
    DPRINTF (LOG_DEBUG, "_asl: AslRequest() requester=0x%08lx\n", (ULONG)requester);
    
    if (!requester)
        return FALSE;
    
    /* Apply any tags passed to AslRequest */
    if (fr->fr_Type == ASL_FileRequest) {
        parse_fr_tags(fr, tagList);
        return do_file_request(fr);
    } else if (fr->fr_Type == ASL_FontRequest) {
        struct LXAFontRequester *fo = (struct LXAFontRequester *)requester;
        parse_fo_tags(fo, tagList);
        return do_font_request(fo);
    }
    
    return FALSE;
}

/*
 * Library structure definitions
 */

struct MyDataInit
{
    UWORD ln_Type_Init     ; UWORD ln_Type_Offset     ; UWORD ln_Type_Content     ;
    UBYTE ln_Name_Init     ; UBYTE ln_Name_Offset     ; ULONG ln_Name_Content     ;
    UWORD lib_Flags_Init   ; UWORD lib_Flags_Offset   ; UWORD lib_Flags_Content   ;
    UWORD lib_Version_Init ; UWORD lib_Version_Offset ; UWORD lib_Version_Content ;
    UWORD lib_Revision_Init; UWORD lib_Revision_Offset; UWORD lib_Revision_Content;
    UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
    ULONG ENDMARK;
};

extern APTR              __g_lxa_asl_FuncTab [];
extern struct MyDataInit __g_lxa_asl_DataTab;
extern struct InitTable  __g_lxa_asl_InitTab;
extern APTR              __g_lxa_asl_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                  // UWORD rt_MatchWord
    &ROMTag,                        // struct Resident *rt_MatchTag
    &__g_lxa_asl_EndResident,       // APTR  rt_EndSkip
    RTF_AUTOINIT,                   // UBYTE rt_Flags
    VERSION,                        // UBYTE rt_Version
    NT_LIBRARY,                     // UBYTE rt_Type
    0,                              // BYTE  rt_Pri
    &_g_asl_ExLibName[0],           // char  *rt_Name
    &_g_asl_ExLibID[0],             // char  *rt_IdString
    &__g_lxa_asl_InitTab            // APTR  rt_Init
};

APTR __g_lxa_asl_EndResident;
struct Resident *__lxa_asl_ROMTag = &ROMTag;

struct InitTable __g_lxa_asl_InitTab =
{
    (ULONG)               sizeof(struct AslBase),
    (APTR              *) &__g_lxa_asl_FuncTab[0],
    (APTR)                &__g_lxa_asl_DataTab,
    (APTR)                __g_lxa_asl_InitLib
};

/* Function table - from asl_lib.fd
 * Standard library functions at -6 through -24
 * First real function at -30
 */
APTR __g_lxa_asl_FuncTab [] =
{
    __g_lxa_asl_OpenLib,           // -6   Standard
    __g_lxa_asl_CloseLib,          // -12  Standard
    __g_lxa_asl_ExpungeLib,        // -18  Standard
    __g_lxa_asl_ExtFuncLib,        // -24  Standard (reserved)
    _asl_AllocFileRequest,         // -30  AllocFileRequest (obsolete)
    _asl_FreeFileRequest,          // -36  FreeFileRequest (obsolete)
    _asl_RequestFile,              // -42  RequestFile (obsolete)
    _asl_AllocAslRequest,          // -48  AllocAslRequest
    _asl_FreeAslRequest,           // -54  FreeAslRequest
    _asl_AslRequest,               // -60  AslRequest
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_asl_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_asl_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_asl_ExLibID[0],
    (ULONG) 0
};
