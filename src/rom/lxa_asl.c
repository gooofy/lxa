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
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <graphics/rastport.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>

#include <graphics/monitor.h>
#include <libraries/gadtools.h>
#include <clib/gadtools_protos.h>
#include <inline/gadtools.h>

#include <utility/tagitem.h>
#include <utility/hooks.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>
#include <libraries/asl.h>

#include "util.h"

#define VERSION    40
#define REVISION   6
#define EXLIBNAME  "asl"
#define EXLIBVER   " 40.6 (7.5.93)\r\n"

char __aligned _g_asl_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_asl_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_asl_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_asl_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct GfxBase *GfxBase;
extern struct IntuitionBase *IntuitionBase;
extern struct Library *GadToolsBase;
extern struct UtilityBase *UtilityBase;

/* AslBase structure */
struct AslBase {
    struct Library lib;
    BPTR           SegList;
};

#define ASL_REQUEST_MAGIC 0x41534c52UL

struct AslRequestHeader {
    ULONG magic;
    ULONG type;
    ULONG size;
};

/* Internal FileRequester structure - public prefix matches NDK FileRequester. */
struct LXAFileRequester {
    UBYTE         fr_Reserved0[4];
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
    BOOL          fr_DoMultiSelect; /* Multi-select requested? */
    BOOL          fr_DrawersOnly;   /* Only show drawers? */
};

/* Internal FontRequester structure - public prefix matches NDK FontRequester. */
struct LXAFontRequester {
    UBYTE         fo_Reserved0[8];
    struct TextAttr fo_Attr;        /* Font attributes on exit (offset 8, matches NDK) */
    UBYTE         fo_FrontPen;      /* Returned front pen */
    UBYTE         fo_BackPen;       /* Returned back pen */
    UBYTE         fo_DrawMode;      /* Returned draw mode */
    UBYTE         fo_SpecialDrawMode;
    APTR          fo_UserData;
    WORD          fo_LeftEdge;
    WORD          fo_TopEdge;
    WORD          fo_Width;
    WORD          fo_Height;
    struct TTextAttr fo_TAttr;
    
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
    UBYTE         fo_InitialStyle;  /* Initial style from tags */
    UBYTE         fo_InitialFlags;  /* Initial flags from tags */
    ULONG         fo_RequestFlags;  /* ASLFO_Flags */
    BOOL          fo_DoStyle;       /* Show style buttons? */
    BOOL          fo_FixedWidthOnly;/* Restrict to fixed-width fonts? */
};

/* Internal ScreenModeRequester structure - public prefix matches NDK ScreenModeRequester. */
struct LXAScreenModeRequester {
    ULONG         sm_DisplayID;
    ULONG         sm_DisplayWidth;
    ULONG         sm_DisplayHeight;
    UWORD         sm_DisplayDepth;
    UWORD         sm_OverscanType;
    BOOL          sm_AutoScroll;
    ULONG         sm_BitMapWidth;
    ULONG         sm_BitMapHeight;
    WORD          sm_LeftEdge;
    WORD          sm_TopEdge;
    WORD          sm_Width;
    WORD          sm_Height;
    BOOL          sm_InfoOpened;
    WORD          sm_InfoLeftEdge;
    WORD          sm_InfoTopEdge;
    WORD          sm_InfoWidth;
    WORD          sm_InfoHeight;
    APTR          sm_UserData;

    /* private: the tag state (ASLSM_*) */
    STRPTR        sm_Title;
    STRPTR        sm_OkText;
    STRPTR        sm_CancelText;
    struct Window *sm_Window;
    struct Screen *sm_Screen;
    STRPTR        sm_PubScreenName;
    BOOL          sm_PrivateIDCMP;
    BOOL          sm_SleepWindow;
    struct Hook  *sm_IntuiMsgFunc;
    struct Hook  *sm_FilterFunc;
    struct List  *sm_CustomSMList;
    struct TextAttr *sm_TextAttr;
    APTR          sm_Locale;
    ULONG         sm_PropertyFlags;
    ULONG         sm_PropertyMask;
    ULONG         sm_MinWidth;
    ULONG         sm_MaxWidth;
    ULONG         sm_MinHeight;
    ULONG         sm_MaxHeight;
    UWORD         sm_MinDepth;
    UWORD         sm_MaxDepth;
    BOOL          sm_DoWidth;
    BOOL          sm_DoHeight;
    BOOL          sm_DoDepth;
    BOOL          sm_DoOverscanType;
    BOOL          sm_DoAutoScroll;
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
    PRIVATE_FUNCTION_ERROR("_asl", "ExtFuncLib");
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

static APTR asl_alloc_request(ULONG req_type, ULONG struct_size)
{
    struct AslRequestHeader *hdr;
    ULONG total_size = sizeof(struct AslRequestHeader) + struct_size;

    hdr = AllocMem(total_size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!hdr)
        return NULL;

    hdr->magic = ASL_REQUEST_MAGIC;
    hdr->type = req_type;
    hdr->size = struct_size;

    return (APTR)(hdr + 1);
}

static struct AslRequestHeader *asl_request_header(APTR requester)
{
    struct AslRequestHeader *hdr;

    if (!requester)
        return NULL;

    hdr = ((struct AslRequestHeader *)requester) - 1;
    if (hdr->magic != ASL_REQUEST_MAGIC)
        return NULL;

    return hdr;
}

static ULONG asl_request_type(APTR requester)
{
    struct AslRequestHeader *hdr = asl_request_header(requester);

    if (!hdr)
        return INVALID_ID;

    return hdr->type;
}

static void asl_free_request_memory(APTR requester)
{
    struct AslRequestHeader *hdr = asl_request_header(requester);
    ULONG total_size;

    if (!hdr)
        return;

    total_size = sizeof(struct AslRequestHeader) + hdr->size;
    hdr->magic = 0;
    FreeMem(hdr, total_size);
}

static void asl_clear_file_arglist(struct LXAFileRequester *fr)
{
    LONG i;

    if (!fr || !fr->fr_ArgList)
    {
        if (fr)
            fr->fr_NumArgs = 0;
        return;
    }

    for (i = 0; i < fr->fr_NumArgs; i++)
    {
        if (fr->fr_ArgList[i].wa_Lock)
            UnLock(fr->fr_ArgList[i].wa_Lock);
        if (fr->fr_ArgList[i].wa_Name)
            asl_strfree(fr->fr_ArgList[i].wa_Name);
    }

    FreeMem(fr->fr_ArgList, sizeof(struct WBArg) * fr->fr_NumArgs);
    fr->fr_ArgList = NULL;
    fr->fr_NumArgs = 0;
}

static void asl_build_file_arglist(struct LXAFileRequester *fr)
{
    BPTR lock = 0;
    STRPTR name_copy;

    if (!fr)
        return;

    asl_clear_file_arglist(fr);

    if (!fr->fr_File || !fr->fr_File[0])
        return;

    fr->fr_ArgList = AllocMem(sizeof(struct WBArg), MEMF_PUBLIC | MEMF_CLEAR);
    if (!fr->fr_ArgList)
        return;

    name_copy = asl_strdup(fr->fr_File);
    if (!name_copy)
    {
        FreeMem(fr->fr_ArgList, sizeof(struct WBArg));
        fr->fr_ArgList = NULL;
        return;
    }

    if (fr->fr_Drawer && fr->fr_Drawer[0])
        lock = Lock(fr->fr_Drawer, ACCESS_READ);

    fr->fr_ArgList[0].wa_Lock = lock;
    fr->fr_ArgList[0].wa_Name = name_copy;
    fr->fr_NumArgs = 1;
}

static void asl_set_font_selection(struct LXAFontRequester *fo, CONST_STRPTR name, UWORD size, UBYTE style, UBYTE flags)
{
    if (!fo)
        return;

    if (fo->fo_Attr.ta_Name)
        asl_strfree(fo->fo_Attr.ta_Name);

    fo->fo_Attr.ta_Name = asl_strdup(name ? name : (CONST_STRPTR)"topaz.font");
    fo->fo_Attr.ta_YSize = size ? size : 8;
    fo->fo_Attr.ta_Style = style;
    fo->fo_Attr.ta_Flags = flags;

    fo->fo_TAttr.tta_Name = fo->fo_Attr.ta_Name;
    fo->fo_TAttr.tta_YSize = fo->fo_Attr.ta_YSize;
    fo->fo_TAttr.tta_Style = fo->fo_Attr.ta_Style;
    fo->fo_TAttr.tta_Flags = fo->fo_Attr.ta_Flags;
    fo->fo_TAttr.tta_Tags = NULL;
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
            case ASLFR_DoMultiSelect:
                fr->fr_DoMultiSelect = (BOOL)tag->ti_Data;
                break;
            case ASLFR_DrawersOnly:
                fr->fr_DrawersOnly = (BOOL)tag->ti_Data;
                break;
            case ASLFR_Flags1:
                fr->fr_DoSaveMode = (tag->ti_Data & FRF_DOSAVEMODE) ? TRUE : FALSE;
                fr->fr_DoPatterns = (tag->ti_Data & FRF_DOPATTERNS) ? TRUE : FALSE;
                fr->fr_DoMultiSelect = (tag->ti_Data & FRF_DOMULTISELECT) ? TRUE : FALSE;
                break;
            case ASLFR_Flags2:
                fr->fr_DrawersOnly = (tag->ti_Data & FRF_DRAWERSONLY) ? TRUE : FALSE;
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
            case ASLFO_InitialStyle:
                fo->fo_InitialStyle = (UBYTE)tag->ti_Data;
                break;
            case ASLFO_InitialFlags:
                fo->fo_InitialFlags = (UBYTE)tag->ti_Data;
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
            case ASLFO_Flags:
                fo->fo_RequestFlags = tag->ti_Data;
                fo->fo_DoStyle = (tag->ti_Data & FOF_DOSTYLE) ? TRUE : FALSE;
                fo->fo_FixedWidthOnly = (tag->ti_Data & FOF_FIXEDWIDTHONLY) ? TRUE : FALSE;
                break;
            case ASLFO_DoStyle:
                fo->fo_DoStyle = (BOOL)tag->ti_Data;
                break;
            case ASLFO_FixedWidthOnly:
                fo->fo_FixedWidthOnly = (BOOL)tag->ti_Data;
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
                                asl_build_file_arglist(fr);
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
                        asl_set_font_selection(fo,
                            fo->fo_InitialName ? fo->fo_InitialName : (CONST_STRPTR)"topaz.font",
                            fo->fo_InitialSize > 0 ? fo->fo_InitialSize : 8,
                            fo->fo_InitialStyle,
                            fo->fo_InitialFlags ? fo->fo_InitialFlags : (FPF_ROMFONT | FPF_DESIGNED));
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
 * ======================================================================
 * Screen-mode requester (Phase 238)
 *
 * Built from GadTools gadgets and laid out as asl.library 40.x of
 * AmigaOS 3.1 does it.  Every rule below was measured on the reference
 * system with tests/gallery/asl.c (scenarios gallery-asl-*, probe
 * tests/probes/asl/smalloc):
 *
 *   - list: every available display mode except the DEFAULT monitor's,
 *     filtered by ASLSM_PropertyFlags/Mask (default DIPF_IS_WB), the
 *     Min/Max width/height/depth ranges, ASLSM_FilterFunc, plus
 *     ASLSM_CustomSMList; named "<MONITOR>:<w> x <h> <EHB |HAM |DPF |DPF2 >
 *     [Interlaced]" (the ROM database has no DTAG_NAME) and sorted by name.
 *   - the optional rows (Overscan, Width+Height, Colors, AutoScroll) are
 *     stacked above the OK/Cancel buttons, 2 pixels apart, as a block
 *     centred in the window; the list (GadTools quantises it to whole
 *     lines) takes the space above them.
 *   - window 318x198 at 30,20 by default (the fields of the requester),
 *     sizeable, zoom box = minimum size; a "Control" menu.
 *   - clicking a mode loads its overscan size into Width/Height; the
 *     Overscan cycle does the same; "Next Mode"/"Last Mode" only move the
 *     selection; "Restore" goes back to the initial values.
 *   - OK clamps the depth to the mode, sets sm_BitMapHeight to the display
 *     height and leaves sm_BitMapWidth alone (it stays 0, as on 3.1); the
 *     window box is stored on OK and on Cancel; with the property window
 *     open its offset is stored and sm_InfoWidth/Height get the requester's
 *     size (as 3.1 does).
 * ======================================================================
 */

#define SMG_OK          1
#define SMG_CANCEL      2
#define SMG_LIST        3
#define SMG_INFOLIST    4
#define SMG_WIDTH       5
#define SMG_HEIGHT      6
#define SMG_DEPTH       7
#define SMG_OVERSCAN    9
#define SMG_AUTOSCROLL  11

#define SM_MAX_PROPS    10

struct SMNode
{
    struct Node node;               /* ln_Name = name */
    ULONG       id;
    ULONG       props;
    struct DimensionInfo dims;
    UBYTE       name[64];
};

struct SMSession
{
    struct LXAScreenModeRequester *sm;
    struct Screen   *scr;
    BOOL             pub_locked;
    struct Window   *win;
    struct Window   *info;
    APTR             vi;
    struct TextAttr  ta;
    struct TextFont *font;
    WORD             fw, fh;
    struct Gadget   *glist;
    struct Gadget   *info_glist;
    struct Gadget   *g_list, *g_width, *g_height, *g_colors, *g_slider, *g_oscan, *g_auto;
    struct Gadget   *g_infolist;
    struct Menu     *menu;
    struct List      modes;
    LONG             nmodes;
    struct SMNode   *sel;
    LONG             selidx;
    /* the current values and the initial ones ("Restore") */
    ULONG            id, width, height;
    UWORD            depth, oscan;
    BOOL             autoscroll;
    ULONG            i_id, i_width, i_height;
    UWORD            i_depth, i_oscan;
    BOOL             i_autoscroll;
    /* the property window list */
    struct List      props;
    struct Node      prop_node[SM_MAX_PROPS];
    UBYTE            freq[32];
    UBYTE            colors[16];
    WORD             minw, minh;
    ULONG            click_secs, click_mics;
    LONG             click_idx;
    struct Requester sleep_req;
    BOOL             sleeping;
    BOOL             shared_port;
    BOOL             done, result;
};

static STRPTR g_sm_oscan_labels[] =
{
    (STRPTR)"Text Size", (STRPTR)"Graphics Size", (STRPTR)"Extreme Size", (STRPTR)"Maximum Size", NULL
};

static struct NewMenu g_sm_newmenu[] =
{
    { NM_TITLE, (STRPTR)"Control",          NULL,       0, 0, NULL },
    { NM_ITEM,  (STRPTR)"Last Mode",        (STRPTR)"L", 0, 0, NULL },
    { NM_ITEM,  (STRPTR)"Next Mode",        (STRPTR)"N", 0, 0, NULL },
    { NM_ITEM,  NM_BARLABEL,                NULL,       0, 0, NULL },
    { NM_ITEM,  (STRPTR)"Property List...", (STRPTR)"?", 0, 0, NULL },
    { NM_ITEM,  (STRPTR)"Restore",          (STRPTR)"R", 0, 0, NULL },
    { NM_ITEM,  NM_BARLABEL,                NULL,       0, 0, NULL },
    { NM_ITEM,  (STRPTR)"OK",               (STRPTR)"O", 0, 0, NULL },
    { NM_ITEM,  (STRPTR)"Cancel",           (STRPTR)"C", 0, 0, NULL },
    { NM_END,   NULL,                       NULL,       0, 0, NULL }
};

#define SM_MENU_LAST     0
#define SM_MENU_NEXT     1
#define SM_MENU_PROPS    3
#define SM_MENU_RESTORE  4
#define SM_MENU_OK       6
#define SM_MENU_CANCEL   7

static void parse_sm_tags(struct LXAScreenModeRequester *sm, struct TagItem *tagList)
{
    struct TagItem *tstate = tagList, *tag;

    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        ULONG d = tag->ti_Data;

        switch (tag->ti_Tag)
        {
            case ASLSM_Window:              sm->sm_Window = (struct Window *)d; break;
            case ASLSM_Screen:              sm->sm_Screen = (struct Screen *)d; break;
            case ASLSM_PubScreenName:       sm->sm_PubScreenName = (STRPTR)d; break;
            case ASLSM_PrivateIDCMP:        sm->sm_PrivateIDCMP = d ? TRUE : FALSE; break;
            case ASLSM_IntuiMsgFunc:        sm->sm_IntuiMsgFunc = (struct Hook *)d; break;
            case ASLSM_SleepWindow:         sm->sm_SleepWindow = d ? TRUE : FALSE; break;
            case ASLSM_UserData:            sm->sm_UserData = (APTR)d; break;
            case ASLSM_TextAttr:            sm->sm_TextAttr = (struct TextAttr *)d; break;
            case ASLSM_Locale:              sm->sm_Locale = (APTR)d; break;
            case ASLSM_TitleText:           sm->sm_Title = (STRPTR)d; break;
            case ASLSM_PositiveText:        sm->sm_OkText = (STRPTR)d; break;
            case ASLSM_NegativeText:        sm->sm_CancelText = (STRPTR)d; break;
            case ASLSM_InitialLeftEdge:     sm->sm_LeftEdge = (WORD)d; break;
            case ASLSM_InitialTopEdge:      sm->sm_TopEdge = (WORD)d; break;
            case ASLSM_InitialWidth:        sm->sm_Width = (WORD)d; break;
            case ASLSM_InitialHeight:       sm->sm_Height = (WORD)d; break;
            case ASLSM_InitialDisplayID:    sm->sm_DisplayID = d; break;
            case ASLSM_InitialDisplayWidth: sm->sm_DisplayWidth = d; break;
            case ASLSM_InitialDisplayHeight:sm->sm_DisplayHeight = d; break;
            case ASLSM_InitialDisplayDepth: sm->sm_DisplayDepth = (UWORD)d; break;
            case ASLSM_InitialOverscanType: sm->sm_OverscanType = (UWORD)d; break;
            case ASLSM_InitialAutoScroll:   sm->sm_AutoScroll = d ? TRUE : FALSE; break;
            case ASLSM_InitialInfoOpened:   sm->sm_InfoOpened = d ? TRUE : FALSE; break;
            case ASLSM_InitialInfoLeftEdge: sm->sm_InfoLeftEdge = (WORD)d; break;
            case ASLSM_InitialInfoTopEdge:  sm->sm_InfoTopEdge = (WORD)d; break;
            case ASLSM_DoWidth:             sm->sm_DoWidth = d ? TRUE : FALSE; break;
            case ASLSM_DoHeight:            sm->sm_DoHeight = d ? TRUE : FALSE; break;
            case ASLSM_DoDepth:             sm->sm_DoDepth = d ? TRUE : FALSE; break;
            case ASLSM_DoOverscanType:      sm->sm_DoOverscanType = d ? TRUE : FALSE; break;
            case ASLSM_DoAutoScroll:        sm->sm_DoAutoScroll = d ? TRUE : FALSE; break;
            case ASLSM_PropertyFlags:       sm->sm_PropertyFlags = d; break;
            case ASLSM_PropertyMask:        sm->sm_PropertyMask = d; break;
            case ASLSM_MinWidth:            sm->sm_MinWidth = d; break;
            case ASLSM_MaxWidth:            sm->sm_MaxWidth = d; break;
            case ASLSM_MinHeight:           sm->sm_MinHeight = d; break;
            case ASLSM_MaxHeight:           sm->sm_MaxHeight = d; break;
            case ASLSM_MinDepth:            sm->sm_MinDepth = (UWORD)d; break;
            case ASLSM_MaxDepth:            sm->sm_MaxDepth = (UWORD)d; break;
            case ASLSM_FilterFunc:          sm->sm_FilterFunc = (struct Hook *)d; break;
            case ASLSM_CustomSMList:        sm->sm_CustomSMList = (struct List *)d; break;
            default:                        break;
        }
    }
}

static void sm_init_defaults(struct LXAScreenModeRequester *sm)
{
    /* AllocAslRequest() defaults, tests/probes/asl/smalloc.ref.out */
    sm->sm_DisplayID = LORES_KEY;
    sm->sm_DisplayWidth = 640;
    sm->sm_DisplayHeight = 200;
    sm->sm_DisplayDepth = 2;
    sm->sm_OverscanType = OSCAN_TEXT;
    sm->sm_AutoScroll = TRUE;
    sm->sm_LeftEdge = 30;
    sm->sm_TopEdge = 20;
    sm->sm_Width = 318;
    sm->sm_Height = 198;
    sm->sm_InfoLeftEdge = 16;
    sm->sm_InfoTopEdge = 25;
    sm->sm_InfoWidth = 280;
    sm->sm_InfoHeight = 84;
    sm->sm_PropertyFlags = DIPF_IS_WB;
    sm->sm_PropertyMask = DIPF_IS_WB;
    sm->sm_MinWidth = 16;
    sm->sm_MaxWidth = 16368;
    sm->sm_MinHeight = 16;
    sm->sm_MaxHeight = 16384;
    sm->sm_MinDepth = 1;
    sm->sm_MaxDepth = 24;
}

static void sm_putnum(UBYTE *buf, WORD *pos, ULONG v)
{
    UBYTE tmp[12];
    WORD n = 0;

    do
    {
        tmp[n++] = (UBYTE)('0' + v % 10);
        v /= 10;
    } while (v && n < 11);
    while (n > 0)
        buf[(*pos)++] = tmp[--n];
}

static void sm_putstr(UBYTE *buf, WORD *pos, WORD max, CONST_STRPTR s)
{
    while (s && *s && *pos < max - 1)
        buf[(*pos)++] = *s++;
}

/* "<MONITOR>:<w> x <h> <attr>[Interlaced]" */
static void sm_build_name(struct SMNode *n)
{
    struct NameInfo ni;
    struct MonitorInfo mi;
    UBYTE mon[32];
    WORD pos = 0, i;
    ULONG p = n->props;

    if (GetDisplayInfoData(NULL, (UBYTE *)&ni, sizeof(ni), DTAG_NAME, n->id) > sizeof(struct QueryHeader) &&
        ni.Name[0])
    {
        for (i = 0; i < (WORD)sizeof(n->name) - 1 && i < DISPLAYNAMELEN && ni.Name[i]; i++)
            n->name[i] = ni.Name[i];
        n->name[i] = 0;
        return;
    }

    mon[0] = 0;
    if (GetDisplayInfoData(NULL, (UBYTE *)&mi, sizeof(mi), DTAG_MNTR, n->id) > sizeof(struct QueryHeader) &&
        mi.Mspc && mi.Mspc->ms_Node.xln_Name)
    {
        CONST_STRPTR s = (CONST_STRPTR)mi.Mspc->ms_Node.xln_Name;
        for (i = 0; i < 31 && s[i] && s[i] != '.'; i++)
            mon[i] = (s[i] >= 'a' && s[i] <= 'z') ? (UBYTE)(s[i] - 32) : (UBYTE)s[i];
        mon[i] = 0;
    }
    if (!mon[0])
    {
        ULONG m = n->id & MONITOR_ID_MASK;
        CONST_STRPTR s = m == PAL_MONITOR_ID ? (CONST_STRPTR)"PAL" : m == NTSC_MONITOR_ID ? (CONST_STRPTR)"NTSC" :
                         (CONST_STRPTR)"UNKNOWN";
        for (i = 0; s[i]; i++)
            mon[i] = s[i];
        mon[i] = 0;
    }

    sm_putstr(n->name, &pos, sizeof(n->name), (CONST_STRPTR)mon);
    n->name[pos++] = ':';
    sm_putnum(n->name, &pos, n->dims.Nominal.MaxX - n->dims.Nominal.MinX + 1);
    sm_putstr(n->name, &pos, sizeof(n->name), (CONST_STRPTR)" x ");
    sm_putnum(n->name, &pos, n->dims.Nominal.MaxY - n->dims.Nominal.MinY + 1);
    n->name[pos++] = ' ';
    if (p & DIPF_IS_DUALPF)
        sm_putstr(n->name, &pos, sizeof(n->name), (p & DIPF_IS_PF2PRI) ? (CONST_STRPTR)"DPF2 " : (CONST_STRPTR)"DPF ");
    else if (p & DIPF_IS_HAM)
        sm_putstr(n->name, &pos, sizeof(n->name), (CONST_STRPTR)"HAM ");
    else if (p & DIPF_IS_EXTRAHALFBRITE)
        sm_putstr(n->name, &pos, sizeof(n->name), (CONST_STRPTR)"EHB ");
    if (p & DIPF_IS_LACE)
        sm_putstr(n->name, &pos, sizeof(n->name), (CONST_STRPTR)"Interlaced");
    n->name[pos] = 0;
}

static BOOL sm_accept(struct LXAScreenModeRequester *sm, ULONG id, ULONG props, struct DimensionInfo *dims)
{
    if ((props & sm->sm_PropertyMask) != (sm->sm_PropertyFlags & sm->sm_PropertyMask))
        return FALSE;
    /* a mode is offered when its raster range meets the allowed range */
    if ((ULONG)dims->MaxRasterWidth < sm->sm_MinWidth || (ULONG)dims->MinRasterWidth > sm->sm_MaxWidth)
        return FALSE;
    if ((ULONG)dims->MaxRasterHeight < sm->sm_MinHeight || (ULONG)dims->MinRasterHeight > sm->sm_MaxHeight)
        return FALSE;
    if (dims->MaxDepth < sm->sm_MinDepth)
        return FALSE;
    if (sm->sm_FilterFunc && !CallHookPkt(sm->sm_FilterFunc, sm, (APTR)id))
        return FALSE;
    return TRUE;
}

static void sm_insert_sorted(struct SMSession *s, struct SMNode *n)
{
    struct Node *at;

    n->node.ln_Name = (char *)n->name;
    for (at = s->modes.lh_Head; at->ln_Succ; at = at->ln_Succ)
        if (strcmp((const char *)n->name, (const char *)at->ln_Name) < 0)
            break;
    /* insert before `at` */
    Insert(&s->modes, &n->node, at->ln_Pred);
    s->nmodes++;
}

static void sm_build_modes(struct SMSession *s)
{
    struct LXAScreenModeRequester *sm = s->sm;
    ULONG id = INVALID_ID;

    NEWLIST(&s->modes);
    s->nmodes = 0;

    while ((id = NextDisplayInfo(id)) != INVALID_ID)
    {
        struct DisplayInfo di;
        struct DimensionInfo dims;
        struct SMNode *n;

        if ((id & MONITOR_ID_MASK) == DEFAULT_MONITOR_ID)
            continue;
        if (GetDisplayInfoData(NULL, (UBYTE *)&di, sizeof(di), DTAG_DISP, id) < sizeof(struct QueryHeader))
            continue;
        if (di.NotAvailable)
            continue;
        if (GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS, id) < sizeof(struct QueryHeader))
            continue;
        if (!sm_accept(sm, id, di.PropertyFlags, &dims))
            continue;
        n = (struct SMNode *)AllocVec(sizeof(*n), MEMF_PUBLIC | MEMF_CLEAR);
        if (!n)
            break;
        n->id = id;
        n->props = di.PropertyFlags;
        CopyMem(&dims, &n->dims, sizeof(dims));
        sm_build_name(n);
        sm_insert_sorted(s, n);
    }

    if (sm->sm_CustomSMList)
    {
        struct DisplayMode *dm;

        for (dm = (struct DisplayMode *)sm->sm_CustomSMList->lh_Head; dm->dm_Node.ln_Succ;
             dm = (struct DisplayMode *)dm->dm_Node.ln_Succ)
        {
            struct SMNode *n;
            WORD i;

            if (!sm_accept(sm, dm->dm_DimensionInfo.Header.DisplayID, dm->dm_PropertyFlags, &dm->dm_DimensionInfo))
                continue;
            n = (struct SMNode *)AllocVec(sizeof(*n), MEMF_PUBLIC | MEMF_CLEAR);
            if (!n)
                break;
            n->id = dm->dm_DimensionInfo.Header.DisplayID;
            n->props = dm->dm_PropertyFlags;
            CopyMem(&dm->dm_DimensionInfo, &n->dims, sizeof(n->dims));
            for (i = 0; dm->dm_Node.ln_Name && dm->dm_Node.ln_Name[i] && i < (WORD)sizeof(n->name) - 1; i++)
                n->name[i] = dm->dm_Node.ln_Name[i];
            n->name[i] = 0;
            sm_insert_sorted(s, n);
        }
    }
}

static void sm_free_modes(struct SMSession *s)
{
    struct Node *n;

    while ((n = RemHead(&s->modes)) != NULL)
        FreeVec(n);
}

static struct SMNode *sm_find_mode(struct SMSession *s, ULONG id, LONG *idx)
{
    struct Node *n;
    LONG i = 0;

    for (n = s->modes.lh_Head; n->ln_Succ; n = n->ln_Succ, i++)
        if (((struct SMNode *)n)->id == id)
        {
            *idx = i;
            return (struct SMNode *)n;
        }
    *idx = -1;
    return NULL;
}

static struct SMNode *sm_mode_at(struct SMSession *s, LONG idx)
{
    struct Node *n;
    LONG i = 0;

    for (n = s->modes.lh_Head; n->ln_Succ; n = n->ln_Succ, i++)
        if (i == idx)
            return (struct SMNode *)n;
    return NULL;
}

static UWORD sm_max_depth(struct SMSession *s)
{
    UWORD d = s->sel ? s->sel->dims.MaxDepth : 8;

    if (d > s->sm->sm_MaxDepth)
        d = s->sm->sm_MaxDepth;
    if (d < 1)
        d = 1;
    return d;
}

static UWORD sm_min_depth(struct SMSession *s)
{
    UWORD d = s->sm->sm_MinDepth ? s->sm->sm_MinDepth : 1;
    UWORD mx = sm_max_depth(s);

    /* HAM and EHB need six planes (the Colors slider of 3.1: HAM 6..8,
     * EHB 6..6) */
    if (s->sel && (s->sel->props & (DIPF_IS_HAM | DIPF_IS_EXTRAHALFBRITE)) && d < 6)
        d = 6;

    return d > mx ? mx : d;
}

static UWORD sm_shown_depth(struct SMSession *s)
{
    UWORD d = s->depth, mn = sm_min_depth(s), mx = sm_max_depth(s);

    if (d < mn)
        d = mn;
    if (d > mx)
        d = mx;
    return d;
}

/* "Colors:" - 2^depth, 64 for EHB, 4,096 for HAM, with a thousands separator */
static void sm_format_colors(struct SMSession *s)
{
    ULONG v;
    UBYTE tmp[16];
    WORD n = 0, i, pos = 0;

    if (s->sel && (s->sel->props & DIPF_IS_HAM))
        v = 1UL << (3 * (sm_shown_depth(s) - 2));     /* HAM6 4,096, HAM8 262,144 */
    else if (s->sel && (s->sel->props & DIPF_IS_EXTRAHALFBRITE))
        v = 64;
    else
        v = 1UL << sm_shown_depth(s);
    do
    {
        if (n == 3 || n == 7)
            tmp[n++] = ',';
        tmp[n++] = (UBYTE)('0' + v % 10);
        v /= 10;
    } while (v && n < 15);
    for (i = n - 1; i >= 0; i--)
        s->colors[pos++] = tmp[i];
    s->colors[pos] = 0;
}

static void sm_query_oscan(struct SMSession *s)
{
    struct Rectangle r;
    UWORD t = s->oscan ? s->oscan : OSCAN_TEXT;

    if (s->sel && QueryOverscan(s->sel->id, &r, t))
    {
        s->width = r.MaxX - r.MinX + 1;
        s->height = r.MaxY - r.MinY + 1;
    }
}

/* the property list: one line per property, then the scan rates */
static void sm_build_props(struct SMSession *s)
{
    struct MonitorInfo mi;
    WORD n = 0, pos = 0;
    ULONG p;

    NEWLIST(&s->props);
    if (!s->sel)
        return;
    p = s->sel->props;
#define SM_PROP(str) do { s->prop_node[n].ln_Name = (char *)(str); AddTail(&s->props, &s->prop_node[n]); n++; } while (0)
    if (p & DIPF_IS_LACE)
        SM_PROP("Interlaced");
    if (p & DIPF_IS_EXTRAHALFBRITE)
        SM_PROP("Extra-HalfBright");
    if (p & DIPF_IS_HAM)
        SM_PROP("Hold & Modify");
    if (p & DIPF_IS_ECS)
        SM_PROP("Requires ECS");
    SM_PROP((p & DIPF_IS_WB) ? "Supports Workbench" : "Does not support Workbench");
    if (p & DIPF_IS_GENLOCK)
        SM_PROP("Supports genlock");
    if (p & DIPF_IS_DRAGGABLE)
        SM_PROP("Draggable");
    if (p & DIPF_IS_DUALPF)
        SM_PROP((p & DIPF_IS_PF2PRI) ? "DualPlayfield Priority 2" : "DualPlayfield");
    if (GetDisplayInfoData(NULL, (UBYTE *)&mi, sizeof(mi), DTAG_MNTR, s->sel->id) > sizeof(struct QueryHeader) &&
        mi.TotalRows && mi.TotalColorClocks)
    {
        ULONG v = 1000000000UL / ((ULONG)mi.TotalRows * mi.TotalColorClocks * 280UL);
        ULONG h = v * mi.TotalRows;

        sm_putnum(s->freq, &pos, v);
        sm_putstr(s->freq, &pos, sizeof(s->freq), (CONST_STRPTR)"Hz, ");
        sm_putnum(s->freq, &pos, h / 1000);
        s->freq[pos++] = '.';
        s->freq[pos++] = (UBYTE)('0' + (h % 1000) / 100);
        s->freq[pos++] = (UBYTE)('0' + (h % 100) / 10);
        sm_putstr(s->freq, &pos, sizeof(s->freq), (CONST_STRPTR)"kHz");
        s->freq[pos] = 0;
        SM_PROP(s->freq);
    }
#undef SM_PROP
}

/* ---- layout ------------------------------------------------------------ */

struct SMLayout
{
    WORD label_col;     /* widest label + 8 */
    WORD nominal;       /* widest row content (default widths) */
    WORD minimal;       /* widest row content (minimum widths) */
    WORD rows_h;        /* the option rows, 2 pixels apart */
    WORD list_min;      /* list space at the minimum window height */
    WORD int_w, int2_w, num_w, slider_w, cycle_w, check_w, check_h, row_h;
};

static WORD sm_tlen(struct SMSession *s, CONST_STRPTR str)
{
    struct RastPort rp;
    WORD len = 0;

    while (str[len])
        len++;
    InitRastPort(&rp);
    SetFont(&rp, s->font);
    return TextLength(&rp, (STRPTR)str, len);
}

static void sm_measure(struct SMSession *s, struct SMLayout *l)
{
    struct LXAScreenModeRequester *sm = s->sm;
    WORD lw = 0, w, nrows = 0, i;

    memset(l, 0, sizeof(*l));
    l->row_h = s->fh + 6;
    l->check_h = s->fh + 5;
    l->check_w = 26;
    l->int_w = 7 * s->fw;               /* 56 with topaz 8 */
    l->int2_w = 7 * s->fw + 1;          /* the Height gadget is one wider */
    l->num_w = 8 * s->fw;               /* "Colors:" value field */
    l->slider_w = 10 * s->fw + 1;       /* 81 */
    for (i = 0; g_sm_oscan_labels[i]; i++)
    {
        w = sm_tlen(s, g_sm_oscan_labels[i]);
        if (w > l->cycle_w)
            l->cycle_w = w;
    }
    l->cycle_w += 36;

    if (sm->sm_DoOverscanType)
    {
        w = sm_tlen(s, (CONST_STRPTR)"Overscan:");
        if (w > lw) lw = w;
        if (l->cycle_w > l->nominal) l->nominal = l->cycle_w;
        if (l->cycle_w > l->minimal) l->minimal = l->cycle_w;
        l->rows_h += l->row_h + 2;
        nrows++;
    }
    if (sm->sm_DoWidth || sm->sm_DoHeight)
    {
        WORD hl = sm_tlen(s, (CONST_STRPTR)"Height:");

        if (sm->sm_DoWidth)
        {
            w = sm_tlen(s, (CONST_STRPTR)"Width:");
            if (w > lw) lw = w;
        }
        if (sm->sm_DoHeight && hl > lw) lw = hl;
        if (sm->sm_DoWidth && sm->sm_DoHeight)
        {
            w = l->int_w + 7 + hl + 8 + l->int2_w;
            if (w > l->nominal) l->nominal = w;
            w = 2 * (l->int_w - 6) + hl + 16;
            if (w > l->minimal) l->minimal = w;
        }
        else
        {
            w = l->int_w;           /* one gadget alone: 56 */
            if (w > l->nominal) l->nominal = w;
            if (l->int_w - 6 > l->minimal) l->minimal = l->int_w - 6;
        }
        l->rows_h += l->row_h + 2;
        nrows++;
    }
    if (sm->sm_DoDepth)
    {
        w = sm_tlen(s, (CONST_STRPTR)"Colors:");
        if (w > lw) lw = w;
        w = l->num_w + 3 + l->slider_w;
        if (w > l->nominal) l->nominal = w;
        w = l->num_w + 3 + l->slider_w - 14;
        if (w > l->minimal) l->minimal = w;
        l->rows_h += l->row_h + 2;
        nrows++;
    }
    if (sm->sm_DoAutoScroll)
    {
        w = sm_tlen(s, (CONST_STRPTR)"AutoScroll:");
        if (w > lw) lw = w;
        if (l->check_w > l->nominal) l->nominal = l->check_w;
        if (l->check_w > l->minimal) l->minimal = l->check_w;
        l->rows_h += l->check_h + 2;
        nrows++;
    }
    l->label_col = nrows ? lw + 8 : 0;

    /* the list space at the minimum height depends on the topmost row
     * (measured: Overscan 32, Width 34, Height 36, Colors 38,
     * AutoScroll 42, no options 46 - with topaz 8) */
    if (sm->sm_DoOverscanType)
        l->list_min = 32;
    else if (sm->sm_DoWidth)
        l->list_min = 34;
    else if (sm->sm_DoHeight)
        l->list_min = 36;
    else if (sm->sm_DoDepth)
        l->list_min = 38;
    else if (sm->sm_DoAutoScroll)
        l->list_min = 42;
    else
        l->list_min = 46;
}

static WORD sm_button_w(struct SMSession *s, CONST_STRPTR text)
{
    WORD w = sm_tlen(s, text) + 12;
    return w > 60 ? w : 60;
}

static CONST_STRPTR sm_ok_text(struct SMSession *s)
{
    return s->sm->sm_OkText ? (CONST_STRPTR)s->sm->sm_OkText : (CONST_STRPTR)"OK";
}

static CONST_STRPTR sm_cancel_text(struct SMSession *s)
{
    return s->sm->sm_CancelText ? (CONST_STRPTR)s->sm->sm_CancelText : (CONST_STRPTR)"Cancel";
}

static void sm_min_size(struct SMSession *s, struct SMLayout *l, WORD bl, WORD bt, WORD br, WORD bb)
{
    WORD w, bh = s->fh + 6;

    /* OK may shrink to its text + 20, Cancel keeps its width */
    s->minw = bl + 4 + (sm_tlen(s, sm_ok_text(s)) + 20) + 4 + sm_button_w(s, sm_cancel_text(s)) + 4 + br;
    w = bl + br + 16 + l->label_col + l->minimal;
    if (l->label_col && w > s->minw)
        s->minw = w;
    s->minh = bt + 2 + l->list_min + l->rows_h + 4 + bh + 2 + bb;
}

static struct Gadget *sm_create_gadgets(struct SMSession *s, struct Window *win)
{
    struct LXAScreenModeRequester *sm = s->sm;
    struct SMLayout l;
    struct NewGadget ng;
    struct Gadget *gad;
    WORD W = win->Width, H = win->Height;
    WORD bl = win->BorderLeft, bt = win->BorderTop, br = win->BorderRight, bb = win->BorderBottom;
    WORD bh = s->fh + 6, btop, okw, cw, rowt, L, block, x0, list_bottom;

    sm_measure(s, &l);
    s->g_list = s->g_width = s->g_height = s->g_colors = s->g_slider = s->g_oscan = s->g_auto = NULL;
    s->glist = NULL;
    gad = CreateContext(&s->glist);
    if (!gad)
        return NULL;

    memset(&ng, 0, sizeof(ng));
    ng.ng_TextAttr = &s->ta;
    ng.ng_VisualInfo = s->vi;

    /* OK and Cancel */
    btop = H - bb - 2 - bh;
    cw = sm_button_w(s, sm_cancel_text(s));
    okw = sm_button_w(s, sm_ok_text(s));
    if (okw > W - bl - br - 8 - 4 - cw)
        okw = W - bl - br - 8 - 4 - cw;
    ng.ng_LeftEdge = bl + 4;
    ng.ng_TopEdge = btop;
    ng.ng_Width = okw;
    ng.ng_Height = bh;
    ng.ng_GadgetText = (UBYTE *)sm_ok_text(s);
    ng.ng_GadgetID = SMG_OK;
    ng.ng_Flags = PLACETEXT_IN;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);
    ng.ng_LeftEdge = W - br - 4 - cw;
    ng.ng_Width = cw;
    ng.ng_GadgetText = (UBYTE *)sm_cancel_text(s);
    ng.ng_GadgetID = SMG_CANCEL;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

    /* the option block, centred, bottom row 4 pixels above the buttons */
    block = l.label_col + l.nominal;
    x0 = (W - block + 1) / 2;
    if (x0 < bl + 4)
        x0 = bl + 4;
    L = x0 + l.label_col;
    rowt = btop - 4 - l.rows_h + 2;
    list_bottom = l.rows_h ? rowt - 2 : btop - 4;

    if (sm->sm_DoOverscanType)
    {
        UWORD o = s->oscan ? s->oscan : OSCAN_TEXT;
        ng.ng_LeftEdge = L;
        ng.ng_TopEdge = rowt;
        ng.ng_Width = l.cycle_w;
        ng.ng_Height = l.row_h;
        ng.ng_GadgetText = (UBYTE *)"Overscan:";
        ng.ng_GadgetID = SMG_OVERSCAN;
        ng.ng_Flags = PLACETEXT_LEFT;
        gad = CreateGadget(CYCLE_KIND, gad, &ng,
                           GTCY_Labels, (ULONG)g_sm_oscan_labels,
                           GTCY_Active, (ULONG)((o >= 1 && o <= 4) ? o - 1 : 0),
                           TAG_END);
        s->g_oscan = gad;
        rowt += l.row_h + 2;
    }
    if (sm->sm_DoWidth || sm->sm_DoHeight)
    {
        WORD x = L;
        ng.ng_TopEdge = rowt;
        ng.ng_Height = l.row_h;
        ng.ng_Flags = PLACETEXT_LEFT;
        if (sm->sm_DoWidth)
        {
            ng.ng_LeftEdge = x;
            ng.ng_Width = l.int_w;
            ng.ng_GadgetText = (UBYTE *)"Width:";
            ng.ng_GadgetID = SMG_WIDTH;
            gad = CreateGadget(INTEGER_KIND, gad, &ng,
                               GTIN_Number, s->width, GTIN_MaxChars, 4, TAG_END);
            s->g_width = gad;
            x += l.int_w + 7 + sm_tlen(s, (CONST_STRPTR)"Height:") + 8;
        }
        if (sm->sm_DoHeight)
        {
            ng.ng_LeftEdge = x;
            ng.ng_Width = sm->sm_DoWidth ? (L + l.nominal - x) : l.int_w;
            ng.ng_GadgetText = (UBYTE *)"Height:";
            ng.ng_GadgetID = SMG_HEIGHT;
            gad = CreateGadget(INTEGER_KIND, gad, &ng,
                               GTIN_Number, s->height, GTIN_MaxChars, 4, TAG_END);
            s->g_height = gad;
        }
        rowt += l.row_h + 2;
    }
    if (sm->sm_DoDepth)
    {
        sm_format_colors(s);
        ng.ng_LeftEdge = L;
        ng.ng_TopEdge = rowt;
        ng.ng_Width = l.num_w;
        ng.ng_Height = s->fh + 3;
        ng.ng_GadgetText = (UBYTE *)"Colors:";
        ng.ng_GadgetID = SMG_DEPTH;
        ng.ng_Flags = PLACETEXT_LEFT;
        gad = CreateGadget(TEXT_KIND, gad, &ng,
                           GTTX_Text, (ULONG)s->colors, GTTX_Justification, GTJ_RIGHT, TAG_END);
        s->g_colors = gad;
        ng.ng_LeftEdge = L + l.num_w + 3;
        ng.ng_Width = L + l.nominal - ng.ng_LeftEdge;
        ng.ng_GadgetText = NULL;
        gad = CreateGadget(SLIDER_KIND, gad, &ng,
                           GTSL_Min, sm_min_depth(s), GTSL_Max, sm_max_depth(s),
                           GTSL_Level, sm_shown_depth(s), GA_RelVerify, TRUE, TAG_END);
        s->g_slider = gad;
        rowt += l.row_h + 2;
    }
    if (sm->sm_DoAutoScroll)
    {
        ng.ng_LeftEdge = L;
        ng.ng_TopEdge = rowt;
        ng.ng_Width = l.check_w;
        ng.ng_Height = l.check_h;
        ng.ng_GadgetText = (UBYTE *)"AutoScroll:";
        ng.ng_GadgetID = SMG_AUTOSCROLL;
        ng.ng_Flags = PLACETEXT_LEFT;
        gad = CreateGadget(CHECKBOX_KIND, gad, &ng,
                           GTCB_Checked, (ULONG)s->autoscroll, GTCB_Scaled, TRUE, TAG_END);
        s->g_auto = gad;
    }

    /* the mode list fills the space above */
    ng.ng_LeftEdge = bl + 4;
    ng.ng_TopEdge = bt + 2;
    ng.ng_Width = W - bl - br - 8;
    ng.ng_Height = list_bottom - (bt + 2);
    ng.ng_GadgetText = NULL;
    ng.ng_GadgetID = SMG_LIST;
    ng.ng_Flags = 0;
    gad = CreateGadget(LISTVIEW_KIND, gad, &ng,
                       GTLV_Labels, (ULONG)&s->modes,
                       GTLV_ShowSelected, 0,
                       GTLV_Selected, (ULONG)s->selidx,
                       GTLV_MakeVisible, (ULONG)(s->selidx >= 0 ? s->selidx : 0),
                       GTLV_ScrollWidth, 18,
                       LAYOUTA_Spacing, 1,
                       TAG_END);
    s->g_list = gad;

    return gad;
}

static BOOL sm_add_gadgets(struct SMSession *s)
{
    if (!sm_create_gadgets(s, s->win))
    {
        if (s->glist)
            FreeGadgets(s->glist);
        s->glist = NULL;
        return FALSE;
    }
    AddGList(s->win, s->glist, (UWORD)~0, -1, NULL);
    RefreshGList(s->glist, s->win, NULL, -1);
    GT_RefreshWindow(s->win, NULL);
    return TRUE;
}

static void sm_relayout(struct SMSession *s)
{
    struct Window *w = s->win;

    if (s->glist)
    {
        RemoveGList(w, s->glist, -1);
        FreeGadgets(s->glist);
        s->glist = NULL;
    }
    SetAPen(w->RPort, 0);
    if (w->Width - w->BorderRight - 1 >= w->BorderLeft && w->Height - w->BorderBottom - 1 >= w->BorderTop)
        RectFill(w->RPort, w->BorderLeft, w->BorderTop, w->Width - w->BorderRight - 1,
                 w->Height - w->BorderBottom - 1);
    sm_add_gadgets(s);
    RefreshWindowFrame(w);
}

/* ---- the property window ------------------------------------------------ */

static void sm_info_update(struct SMSession *s)
{
    if (!s->info || !s->g_infolist)
        return;
    GT_SetGadgetAttrs(s->g_infolist, s->info, NULL, GTLV_Labels, ~0, TAG_END);
    sm_build_props(s);
    GT_SetGadgetAttrs(s->g_infolist, s->info, NULL, GTLV_Labels, (ULONG)&s->props, GTLV_Top, 0, TAG_END);
}

static void sm_info_close(struct SMSession *s, BOOL remember)
{
    struct IntuiMessage *m;
    struct Node *succ;

    if (!s->info)
        return;
    if (remember)
    {
        s->sm->sm_InfoLeftEdge = s->info->LeftEdge - s->win->LeftEdge;
        s->sm->sm_InfoTopEdge = s->info->TopEdge - s->win->TopEdge;
    }
    /* the window shares the requester's port: drop its messages first */
    Forbid();
    for (m = (struct IntuiMessage *)s->win->UserPort->mp_MsgList.lh_Head;
         (succ = m->ExecMessage.mn_Node.ln_Succ) != NULL; m = (struct IntuiMessage *)succ)
    {
        if (m->IDCMPWindow == s->info)
        {
            Remove((struct Node *)m);
            ReplyMsg((struct Message *)m);
        }
    }
    s->info->UserPort = NULL;
    ModifyIDCMP(s->info, 0);
    Permit();
    CloseWindow(s->info);
    s->info = NULL;
    if (s->info_glist)
        FreeGadgets(s->info_glist);
    s->info_glist = NULL;
    s->g_infolist = NULL;
}

static void sm_info_open(struct SMSession *s)
{
    struct NewGadget ng;
    struct Gadget *gad;
    WORD w = s->sm->sm_InfoWidth > 0 ? s->sm->sm_InfoWidth : 280;
    WORD h = (s->sm->sm_InfoHeight > 0 ? s->sm->sm_InfoHeight : 84) + 4;
    WORD bt = s->scr->WBorTop + s->fh + 1;

    if (s->info)
        return;
    sm_build_props(s);
    s->info_glist = NULL;
    gad = CreateContext(&s->info_glist);
    memset(&ng, 0, sizeof(ng));
    ng.ng_TextAttr = &s->ta;
    ng.ng_VisualInfo = s->vi;
    ng.ng_LeftEdge = s->scr->WBorLeft + 4;
    ng.ng_TopEdge = bt + 4;
    ng.ng_Width = w - 2 * (s->scr->WBorLeft + 4);
    ng.ng_Height = h - (bt + 4) - 6;
    ng.ng_GadgetID = SMG_INFOLIST;
    gad = CreateGadget(LISTVIEW_KIND, gad, &ng,
                       GTLV_Labels, (ULONG)&s->props, GTLV_ReadOnly, TRUE,
                       LAYOUTA_Spacing, 1, TAG_END);
    s->g_infolist = gad;
    if (!gad)
    {
        FreeGadgets(s->info_glist);
        s->info_glist = NULL;
        return;
    }
    s->info = OpenWindowTags(NULL,
        WA_Left, s->win->LeftEdge + s->sm->sm_InfoLeftEdge,
        WA_Top, s->win->TopEdge + s->sm->sm_InfoTopEdge,
        WA_Width, w,
        WA_Height, h,
        WA_Title, (ULONG)"Mode Properties",
        WA_CustomScreen, (ULONG)s->scr,
        WA_DragBar, TRUE,
        WA_DepthGadget, TRUE,
        WA_CloseGadget, TRUE,
        WA_SimpleRefresh, TRUE,
        WA_NewLookMenus, TRUE,
        WA_AutoAdjust, TRUE,
        WA_Gadgets, (ULONG)s->info_glist,
        TAG_END);
    if (!s->info)
    {
        FreeGadgets(s->info_glist);
        s->info_glist = NULL;
        s->g_infolist = NULL;
        return;
    }
    s->info->UserPort = s->win->UserPort;
    ModifyIDCMP(s->info, IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_GADGETDOWN |
                         IDCMP_GADGETUP | IDCMP_CLOSEWINDOW | IDCMP_INTUITICKS);
    GT_RefreshWindow(s->info, NULL);
}

/* ---- state changes --------------------------------------------------------- */

static void sm_update_values(struct SMSession *s)
{
    struct Window *w = s->win;

    if (s->g_width)
        GT_SetGadgetAttrs(s->g_width, w, NULL, GTIN_Number, s->width, TAG_END);
    if (s->g_height)
        GT_SetGadgetAttrs(s->g_height, w, NULL, GTIN_Number, s->height, TAG_END);
    if (s->g_oscan)
        GT_SetGadgetAttrs(s->g_oscan, w, NULL, GTCY_Active, (ULONG)(s->oscan >= 1 && s->oscan <= 4 ? s->oscan - 1 : 0),
                          TAG_END);
    if (s->g_auto)
        GT_SetGadgetAttrs(s->g_auto, w, NULL, GTCB_Checked, (ULONG)s->autoscroll, TAG_END);
    if (s->g_slider)
        GT_SetGadgetAttrs(s->g_slider, w, NULL, GTSL_Min, sm_min_depth(s), GTSL_Max, sm_max_depth(s),
                          GTSL_Level, sm_shown_depth(s), TAG_END);
    if (s->g_colors)
    {
        sm_format_colors(s);
        GT_SetGadgetAttrs(s->g_colors, w, NULL, GTTX_Text, (ULONG)s->colors, TAG_END);
    }
}

static void sm_select(struct SMSession *s, LONG idx, BOOL load_size)
{
    struct SMNode *n = sm_mode_at(s, idx);

    if (!n)
        return;
    s->sel = n;
    s->selidx = idx;
    s->id = n->id;
    if (load_size)
        sm_query_oscan(s);
    GT_SetGadgetAttrs(s->g_list, s->win, NULL, GTLV_Selected, (ULONG)idx, GTLV_MakeVisible, (ULONG)idx, TAG_END);
    sm_update_values(s);
    sm_info_update(s);
}

static void sm_restore(struct SMSession *s)
{
    LONG idx;

    s->id = s->i_id;
    s->width = s->i_width;
    s->height = s->i_height;
    s->depth = s->i_depth;
    s->oscan = s->i_oscan;
    s->autoscroll = s->i_autoscroll;
    s->sel = sm_find_mode(s, s->id, &idx);
    s->selidx = idx;
    GT_SetGadgetAttrs(s->g_list, s->win, NULL, GTLV_Selected, (ULONG)idx,
                      GTLV_MakeVisible, (ULONG)(idx >= 0 ? idx : 0), TAG_END);
    sm_update_values(s);
    sm_info_update(s);
}

static void sm_read_integers(struct SMSession *s)
{
    if (s->g_width)
        s->width = ((struct StringInfo *)s->g_width->SpecialInfo)->LongInt;
    if (s->g_height)
        s->height = ((struct StringInfo *)s->g_height->SpecialInfo)->LongInt;
}

static void sm_finish(struct SMSession *s, BOOL ok)
{
    struct LXAScreenModeRequester *sm = s->sm;

    s->done = TRUE;
    s->result = ok;
    if (!ok)
        return;
    sm_read_integers(s);
    sm->sm_DisplayID = s->id;
    sm->sm_DisplayWidth = s->width;
    sm->sm_DisplayHeight = s->height;
    sm->sm_DisplayDepth = s->sel ? sm_shown_depth(s) : s->depth;
    sm->sm_OverscanType = s->oscan;
    sm->sm_AutoScroll = s->autoscroll;
    sm->sm_BitMapHeight = s->height;
}

static void sm_handle(struct SMSession *s, ULONG cl, UWORD code, struct Gadget *g, struct Window *iw,
                      ULONG secs, ULONG mics)
{
    if (iw == s->info && s->info)
    {
        if (cl == IDCMP_CLOSEWINDOW)
            sm_info_close(s, TRUE);
        else if (cl == IDCMP_REFRESHWINDOW)
        {
            GT_BeginRefresh(s->info);
            GT_EndRefresh(s->info, TRUE);
        }
        return;
    }

    switch (cl)
    {
        case IDCMP_CLOSEWINDOW:
            sm_finish(s, FALSE);
            break;

        case IDCMP_REFRESHWINDOW:
            GT_BeginRefresh(s->win);
            GT_EndRefresh(s->win, TRUE);
            break;

        case IDCMP_NEWSIZE:
            sm_relayout(s);
            break;

        case IDCMP_GADGETUP:
            if (!g)
                break;
            switch (g->GadgetID)
            {
                case SMG_OK:
                    sm_finish(s, TRUE);
                    break;
                case SMG_CANCEL:
                    sm_finish(s, FALSE);
                    break;
                case SMG_LIST:
                {
                    BOOL dbl = (s->click_idx == (LONG)code) && DoubleClick(s->click_secs, s->click_mics, secs, mics);

                    s->click_idx = code;
                    s->click_secs = secs;
                    s->click_mics = mics;
                    sm_select(s, code, TRUE);
                    if (dbl)
                        sm_finish(s, TRUE);
                    break;
                }
                case SMG_OVERSCAN:
                    s->oscan = code + 1;
                    sm_query_oscan(s);
                    sm_update_values(s);
                    break;
                case SMG_WIDTH:
                case SMG_HEIGHT:
                    sm_read_integers(s);
                    break;
                case SMG_DEPTH:
                    s->depth = code;
                    sm_update_values(s);
                    break;
                case SMG_AUTOSCROLL:
                    s->autoscroll = (g->Flags & GFLG_SELECTED) ? TRUE : FALSE;
                    break;
            }
            break;

        case IDCMP_MOUSEMOVE:
            if (g && g == s->g_slider && s->g_colors)
            {
                s->depth = code;
                sm_format_colors(s);
                GT_SetGadgetAttrs(s->g_colors, s->win, NULL, GTTX_Text, (ULONG)s->colors, TAG_END);
            }
            break;

        case IDCMP_MENUPICK:
        {
            UWORD num = code;

            while (num != MENUNULL && !s->done)
            {
                struct MenuItem *item = ItemAddress(s->win->MenuStrip, num);

                if (MENUNUM(num) == 0)
                {
                    switch (ITEMNUM(num))
                    {
                        case SM_MENU_LAST:
                            if (s->nmodes)
                                sm_select(s, s->selidx > 0 ? s->selidx - 1 : (s->selidx < 0 ? 0 : s->nmodes - 1), FALSE);
                            break;
                        case SM_MENU_NEXT:
                            if (s->nmodes)
                                sm_select(s, (s->selidx + 1) % s->nmodes, FALSE);
                            break;
                        case SM_MENU_PROPS:
                            if (s->info)
                                sm_info_close(s, TRUE);
                            else
                                sm_info_open(s);
                            break;
                        case SM_MENU_RESTORE:
                            sm_restore(s);
                            break;
                        case SM_MENU_OK:
                            sm_finish(s, TRUE);
                            break;
                        case SM_MENU_CANCEL:
                            sm_finish(s, FALSE);
                            break;
                    }
                }
                if (!item)
                    break;
                num = item->NextSelect;
            }
            break;
        }
    }
}

static BOOL do_screenmode_request(struct LXAScreenModeRequester *sm)
{
    struct SMSession *s;
    struct SMLayout l;
    struct Screen *scr = NULL;
    BOOL result = FALSE;
    LONG idx;
    WORD left, top, w, h, bl, bt, br, bb;

    s = (struct SMSession *)AllocVec(sizeof(*s), MEMF_PUBLIC | MEMF_CLEAR);
    if (!s)
        return FALSE;
    s->sm = sm;
    s->click_idx = -1;
    NEWLIST(&s->props);

    /* the screen: ASLSM_Screen, else the public screen, else the
     * parent window's, else the default public screen */
    if (sm->sm_Screen)
        scr = sm->sm_Screen;
    else if (sm->sm_PubScreenName && (scr = LockPubScreen(sm->sm_PubScreenName)) != NULL)
        s->pub_locked = TRUE;
    else if (sm->sm_Window)
        scr = sm->sm_Window->WScreen;
    if (!scr && (scr = LockPubScreen(NULL)) != NULL)
        s->pub_locked = TRUE;
    if (!scr)
        goto out;
    s->scr = scr;

    if (sm->sm_TextAttr)
        s->ta = *sm->sm_TextAttr;
    else
        s->ta = *scr->Font;
    s->font = OpenFont(&s->ta);
    if (!s->font)
    {
        s->ta.ta_Name = (STRPTR)"topaz.font";
        s->ta.ta_YSize = 8;
        s->ta.ta_Style = 0;
        s->ta.ta_Flags = 0;
        s->font = OpenFont(&s->ta);
    }
    if (!s->font)
        goto out;
    s->fh = s->font->tf_YSize;
    s->fw = s->font->tf_XSize;

    s->vi = GetVisualInfoA(scr, NULL);
    if (!s->vi)
        goto out;

    /* the initial values; a mode of the default monitor means the same
     * mode on the monitor the system runs on */
    s->id = sm->sm_DisplayID;
    if ((s->id & MONITOR_ID_MASK) == DEFAULT_MONITOR_ID)
        s->id |= (GfxBase->DisplayFlags & PAL) ? PAL_MONITOR_ID : NTSC_MONITOR_ID;
    sm->sm_DisplayID = s->id;
    s->width = sm->sm_DisplayWidth;
    s->height = sm->sm_DisplayHeight;
    s->depth = sm->sm_DisplayDepth;
    s->oscan = sm->sm_OverscanType ? sm->sm_OverscanType : OSCAN_TEXT;
    s->autoscroll = sm->sm_AutoScroll;
    s->i_id = s->id;
    s->i_width = s->width;
    s->i_height = s->height;
    s->i_depth = s->depth;
    s->i_oscan = s->oscan;
    s->i_autoscroll = s->autoscroll;

    sm_build_modes(s);
    s->sel = sm_find_mode(s, s->id, &idx);
    s->selidx = idx;

    /* window geometry: the requester's box, at least the minimum size,
     * moved onto the screen */
    bl = scr->WBorLeft;
    br = scr->WBorRight;
    bt = scr->WBorTop + scr->Font->ta_YSize + 1;
    bb = 10;        /* the size gadget sits in the bottom border */
    sm_measure(s, &l);
    sm_min_size(s, &l, bl, bt, br, bb);
    w = sm->sm_Width;
    h = sm->sm_Height;
    if (w < s->minw)
        w = s->minw;
    if (h < s->minh)
        h = s->minh;
    if (w > scr->Width)
        w = scr->Width;
    if (h > scr->Height)
        h = scr->Height;
    left = sm->sm_LeftEdge;
    top = sm->sm_TopEdge;
    if (left + w > scr->Width)
        left = scr->Width - w;
    if (top + h > scr->Height)
        top = scr->Height - h;
    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;

    {
        WORD zoom[4];
        struct TagItem wtags[] =
        {
            { WA_Left, 0 }, { WA_Top, 0 }, { WA_Width, 0 }, { WA_Height, 0 },
            { WA_MinWidth, 0 }, { WA_MinHeight, 0 }, { WA_MaxWidth, 1024 }, { WA_MaxHeight, 1024 },
            { WA_Title, 0 }, { WA_CustomScreen, 0 }, { WA_Zoom, 0 },
            { WA_SizeGadget, TRUE }, { WA_SizeBBottom, TRUE }, { WA_DragBar, TRUE },
            { WA_DepthGadget, TRUE }, { WA_CloseGadget, TRUE }, { WA_SimpleRefresh, TRUE },
            { WA_Activate, TRUE }, { WA_NewLookMenus, TRUE }, { WA_IDCMP, 0 },
            { TAG_DONE, 0 }
        };
        ULONG idcmp = IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE |
                      IDCMP_GADGETDOWN | IDCMP_GADGETUP | IDCMP_MENUPICK | IDCMP_CLOSEWINDOW |
                      IDCMP_INTUITICKS;

        s->shared_port = (sm->sm_Window && !sm->sm_PrivateIDCMP && sm->sm_Window->UserPort);
        zoom[0] = left;
        zoom[1] = top;
        zoom[2] = s->minw;
        zoom[3] = s->minh;
        wtags[0].ti_Data = left;
        wtags[1].ti_Data = top;
        wtags[2].ti_Data = w;
        wtags[3].ti_Data = h;
        wtags[4].ti_Data = s->minw;
        wtags[5].ti_Data = s->minh;
        wtags[8].ti_Data = (ULONG)(sm->sm_Title ? sm->sm_Title : (STRPTR)"Select Screen Mode");
        wtags[9].ti_Data = (ULONG)scr;
        wtags[10].ti_Data = (ULONG)zoom;
        wtags[19].ti_Data = s->shared_port ? 0 : idcmp;
        s->win = OpenWindowTagList(NULL, wtags);
        if (!s->win)
            goto out_modes;
        if (s->shared_port)
        {
            s->win->UserPort = sm->sm_Window->UserPort;
            ModifyIDCMP(s->win, idcmp);
        }
    }

    s->menu = CreateMenus(g_sm_newmenu, TAG_END);
    if (s->menu)
    {
        struct MenuItem *it;
        WORD i = 0;

        /* the OK/Cancel items carry the gadget texts */
        for (it = s->menu->FirstItem; it; it = it->NextItem, i++)
        {
            if (i == SM_MENU_OK && sm->sm_OkText)
                ((struct IntuiText *)it->ItemFill)->IText = (UBYTE *)sm->sm_OkText;
            if (i == SM_MENU_CANCEL && sm->sm_CancelText)
                ((struct IntuiText *)it->ItemFill)->IText = (UBYTE *)sm->sm_CancelText;
        }
        if (LayoutMenus(s->menu, s->vi, GTMN_NewLookMenus, TRUE, GTMN_TextAttr, (ULONG)&s->ta, TAG_END))
            SetMenuStrip(s->win, s->menu);
    }

    if (sm->sm_SleepWindow && sm->sm_Window)
    {
        InitRequester(&s->sleep_req);
        if (Request(&s->sleep_req, sm->sm_Window))
        {
            s->sleeping = TRUE;
            SetWindowPointer(sm->sm_Window, WA_BusyPointer, TRUE, TAG_END);
        }
    }

    if (!sm_add_gadgets(s))
        goto out_win;
    if (sm->sm_InfoOpened)
        sm_info_open(s);

    while (!s->done)
    {
        struct MsgPort *port = s->win->UserPort;
        struct IntuiMessage *im;

        WaitPort(port);
        while (!s->done && (im = GT_GetIMsg(port)) != NULL)
        {
            ULONG cl = im->Class;
            UWORD code = im->Code;
            struct Gadget *g = (struct Gadget *)im->IAddress;
            struct Window *iw = im->IDCMPWindow;
            ULONG secs = im->Seconds, mics = im->Micros;

            if (iw != s->win && iw != s->info)
            {
                /* a message of the parent window (shared port) */
                if (sm->sm_IntuiMsgFunc)
                    CallHookPkt(sm->sm_IntuiMsgFunc, sm, im);
                GT_ReplyIMsg(im);
                continue;
            }
            if (cl != IDCMP_GADGETUP && cl != IDCMP_GADGETDOWN && cl != IDCMP_MOUSEMOVE)
                g = NULL;
            GT_ReplyIMsg(im);
            sm_handle(s, cl, code, g, iw, secs, mics);
        }
    }
    result = s->result;

    /* the requester's window box, and the property window's */
    sm->sm_LeftEdge = s->win->LeftEdge;
    sm->sm_TopEdge = s->win->TopEdge;
    sm->sm_Width = s->win->Width;
    sm->sm_Height = s->win->Height;
    sm->sm_InfoOpened = s->info ? TRUE : FALSE;
    if (s->info)
    {
        sm->sm_InfoLeftEdge = s->info->LeftEdge - s->win->LeftEdge;
        sm->sm_InfoTopEdge = s->info->TopEdge - s->win->TopEdge;
        sm->sm_InfoWidth = s->win->Width;
        sm->sm_InfoHeight = s->win->Height;
        sm_info_close(s, FALSE);
    }

out_win:
    if (s->sleeping)
    {
        EndRequest(&s->sleep_req, sm->sm_Window);
        SetWindowPointer(sm->sm_Window, TAG_END);
    }
    if (s->menu)
    {
        ClearMenuStrip(s->win);
        FreeMenus(s->menu);
    }
    if (s->shared_port)
    {
        struct IntuiMessage *m;
        struct Node *succ;

        Forbid();
        for (m = (struct IntuiMessage *)s->win->UserPort->mp_MsgList.lh_Head;
             (succ = m->ExecMessage.mn_Node.ln_Succ) != NULL; m = (struct IntuiMessage *)succ)
        {
            if (m->IDCMPWindow == s->win)
            {
                Remove((struct Node *)m);
                ReplyMsg((struct Message *)m);
            }
        }
        s->win->UserPort = NULL;
        ModifyIDCMP(s->win, 0);
        Permit();
    }
    CloseWindow(s->win);
    if (s->glist)
        FreeGadgets(s->glist);
out_modes:
    sm_free_modes(s);
out:
    if (s->vi)
        FreeVisualInfo(s->vi);
    if (s->font)
        CloseFont(s->font);
    if (s->pub_locked)
        UnlockPubScreen(NULL, s->scr);
    FreeVec(s);
    return result;
}


/*
 * ASL Functions (V36+)
 */

/* AllocFileRequest - Obsolete, use AllocAslRequest instead */
APTR _asl_AllocFileRequest ( register struct AslBase *AslBase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_asl: AllocFileRequest() (obsolete) -> AllocAslRequest(ASL_FileRequest)\n");
    
    struct LXAFileRequester *req = asl_alloc_request(ASL_FileRequest, sizeof(struct LXAFileRequester));
    if (req) {
        req->fr_LeftEdge = -1;
        req->fr_TopEdge = -1;
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
        asl_clear_file_arglist(fr);
        if (fr->fr_File) asl_strfree(fr->fr_File);
        if (fr->fr_Drawer) asl_strfree(fr->fr_Drawer);
        if (fr->fr_Pattern) asl_strfree(fr->fr_Pattern);
        asl_free_request_memory(fr);
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
            struct LXAFileRequester *fr = asl_alloc_request(reqType, sizeof(struct LXAFileRequester));
            if (fr) {
                fr->fr_LeftEdge = -1;  /* Sentinel: not set by app */
                fr->fr_TopEdge = -1;   /* Sentinel: not set by app */
                fr->fr_Width = 300;
                fr->fr_Height = 200;
                parse_fr_tags(fr, tagList);
            }
            return fr;
        }
        
        case ASL_FontRequest: {
            struct LXAFontRequester *fo = asl_alloc_request(reqType, sizeof(struct LXAFontRequester));
            if (fo) {
                fo->fo_LeftEdge = -1;   /* Sentinel: not set by app */
                fo->fo_TopEdge = -1;    /* Sentinel: not set by app */
                fo->fo_Width = 300;
                fo->fo_Height = 200;
                fo->fo_MinHeight = 5;   /* Default per RKRM */
                fo->fo_MaxHeight = 24;  /* Default per RKRM */
                fo->fo_InitialSize = 8; /* Default font size */
                parse_fo_tags(fo, tagList);
                asl_set_font_selection(fo,
                    fo->fo_InitialName ? fo->fo_InitialName : (CONST_STRPTR)"topaz.font",
                    fo->fo_InitialSize,
                    fo->fo_InitialStyle,
                    fo->fo_InitialFlags ? fo->fo_InitialFlags : (FPF_ROMFONT | FPF_DESIGNED));
            }
            return fo;
        }
        
        case ASL_ScreenModeRequest: {
            struct LXAScreenModeRequester *sm = asl_alloc_request(reqType, sizeof(struct LXAScreenModeRequester));
            if (sm) {
                sm_init_defaults(sm);
                parse_sm_tags(sm, tagList);
            }
            return sm;
        }

        default:
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

    switch (asl_request_type(requester)) {
    case ASL_FileRequest: {
        struct LXAFileRequester *fr = (struct LXAFileRequester *)requester;
        asl_clear_file_arglist(fr);
        if (fr->fr_File) asl_strfree(fr->fr_File);
        if (fr->fr_Drawer) asl_strfree(fr->fr_Drawer);
        if (fr->fr_Pattern) asl_strfree(fr->fr_Pattern);
        asl_free_request_memory(fr);
        break;
    }
    case ASL_FontRequest: {
        struct LXAFontRequester *fo = (struct LXAFontRequester *)requester;
        if (fo->fo_Attr.ta_Name) asl_strfree(fo->fo_Attr.ta_Name);
        asl_free_request_memory(fo);
        break;
    }
    case ASL_ScreenModeRequest:
        asl_free_request_memory(requester);
        break;
    default:
        break;
    }
}

/* AslRequest - Display a requester */
BOOL _asl_AslRequest ( register struct AslBase *AslBase __asm("a6"),
                       register APTR requester __asm("a0"),
                       register struct TagItem *tagList __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_asl: AslRequest() requester=0x%08lx\n", (ULONG)requester);
    
    if (!requester)
        return FALSE;
    
    /* Apply any tags passed to AslRequest */
    if (asl_request_type(requester) == ASL_FileRequest) {
        struct LXAFileRequester *fr = (struct LXAFileRequester *)requester;
        parse_fr_tags(fr, tagList);
        return do_file_request(fr);
    } else if (asl_request_type(requester) == ASL_FontRequest) {
        struct LXAFontRequester *fo = (struct LXAFontRequester *)requester;
        parse_fo_tags(fo, tagList);
        return do_font_request(fo);
    } else if (asl_request_type(requester) == ASL_ScreenModeRequest) {
        struct LXAScreenModeRequester *sm = (struct LXAScreenModeRequester *)requester;
        parse_sm_tags(sm, tagList);
        return do_screenmode_request(sm);
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
