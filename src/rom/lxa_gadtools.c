/*
 * lxa gadtools.library implementation
 *
 * Provides the GadTools API for creating standard AmigaOS 2.0+ gadgets.
 * This is a stub implementation that provides basic functionality.
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

#include <libraries/gadtools.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <utility/tagitem.h>

#include "util.h"

#define VERSION    39
#define REVISION   1
#define EXLIBNAME  "gadtools"
#define EXLIBVER   " 39.1 (2025/02/02)"

char __aligned _g_gadtools_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_gadtools_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_gadtools_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_gadtools_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* GadToolsBase structure */
struct GadToolsBase {
    struct Library lib;
    BPTR           SegList;
};

/* VisualInfo structure - opaque handle for screen visual information */
struct VisualInfo {
    struct Screen *vi_Screen;
    struct DrawInfo *vi_DrawInfo;
};

/* GadgetContext - internal context for gadget list creation */
struct GadgetContext {
    struct Gadget *gc_Last;
};

/*
 * Library init/open/close/expunge
 */

struct GadToolsBase * __g_lxa_gadtools_InitLib ( register struct GadToolsBase *gtb    __asm("a6"),
                                                 register BPTR                seglist __asm("a0"),
                                                 register struct ExecBase    *sysb    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_gadtools: InitLib() called\n");
    gtb->SegList = seglist;
    return gtb;
}

BPTR __g_lxa_gadtools_ExpungeLib ( register struct GadToolsBase *gtb __asm("a6") )
{
    return 0;
}

struct GadToolsBase * __g_lxa_gadtools_OpenLib ( register struct GadToolsBase *gtb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: OpenLib() called, gtb=0x%08lx\n", (ULONG)gtb);
    gtb->lib.lib_OpenCnt++;
    gtb->lib.lib_Flags &= ~LIBF_DELEXP;
    return gtb;
}

BPTR __g_lxa_gadtools_CloseLib ( register struct GadToolsBase *gtb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: CloseLib() called, gtb=0x%08lx\n", (ULONG)gtb);
    gtb->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_gadtools_ExtFuncLib ( void )
{
    return 0;
}

/*
 * Gadget Functions
 */

/* CreateGadgetA - Create a GadTools gadget
 * kind: gadget kind (BUTTON_KIND, STRING_KIND, etc.)
 * gad: previous gadget in list (or context gadget)
 * ng: NewGadget structure
 * taglist: additional tags
 */
struct Gadget * _gadtools_CreateGadgetA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                          register ULONG kind __asm("d0"),
                                          register struct Gadget *gad __asm("a0"),
                                          register struct NewGadget *ng __asm("a1"),
                                          register struct TagItem *taglist __asm("a2") )
{
    struct Gadget *newgad;

    DPRINTF (LOG_DEBUG, "_gadtools: CreateGadgetA() kind=%ld, prevgad=0x%08lx, ng=0x%08lx\n",
             kind, (ULONG)gad, (ULONG)ng);

    if (!ng)
        return NULL;

    /* Allocate gadget structure */
    newgad = AllocMem(sizeof(struct Gadget), MEMF_CLEAR | MEMF_PUBLIC);
    if (!newgad)
        return NULL;

    /* Fill in basic gadget fields from NewGadget */
    newgad->LeftEdge = ng->ng_LeftEdge;
    newgad->TopEdge = ng->ng_TopEdge;
    newgad->Width = ng->ng_Width;
    newgad->Height = ng->ng_Height;
    newgad->GadgetID = ng->ng_GadgetID;
    newgad->UserData = ng->ng_UserData;

    /* Set activation based on gadget kind */
    switch (kind) {
        case BUTTON_KIND:
            newgad->GadgetType = GTYP_BOOLGADGET;
            newgad->Activation = GACT_RELVERIFY;
            newgad->Flags = GFLG_GADGHCOMP;
            break;
        case STRING_KIND:
        case INTEGER_KIND:
            newgad->GadgetType = GTYP_STRGADGET;
            newgad->Activation = GACT_RELVERIFY;
            newgad->Flags = GFLG_GADGHCOMP;
            break;
        case CHECKBOX_KIND:
            newgad->GadgetType = GTYP_BOOLGADGET;
            newgad->Activation = GACT_RELVERIFY | GACT_TOGGLESELECT;
            newgad->Flags = GFLG_GADGHCOMP;
            break;
        case CYCLE_KIND:
        case MX_KIND:
        case SLIDER_KIND:
        case SCROLLER_KIND:
        case LISTVIEW_KIND:
        case PALETTE_KIND:
            newgad->GadgetType = GTYP_PROPGADGET;
            newgad->Activation = GACT_RELVERIFY;
            newgad->Flags = GFLG_GADGHCOMP;
            break;
        case TEXT_KIND:
        case NUMBER_KIND:
            newgad->GadgetType = GTYP_BOOLGADGET;
            newgad->Flags = GFLG_GADGHNONE;
            break;
        default:
            newgad->GadgetType = GTYP_BOOLGADGET;
            newgad->Flags = GFLG_GADGHCOMP;
            break;
    }

    /* Link to previous gadget */
    if (gad) {
        gad->NextGadget = newgad;
    }

    DPRINTF (LOG_DEBUG, "_gadtools: CreateGadgetA() -> 0x%08lx\n", (ULONG)newgad);
    return newgad;
}

/* FreeGadgets - Free a list of gadgets created by CreateGadgetA */
void _gadtools_FreeGadgets ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                             register struct Gadget *gad __asm("a0") )
{
    struct Gadget *next;

    DPRINTF (LOG_DEBUG, "_gadtools: FreeGadgets() gad=0x%08lx\n", (ULONG)gad);

    while (gad) {
        next = gad->NextGadget;
        FreeMem(gad, sizeof(struct Gadget));
        gad = next;
    }
}

/* GT_SetGadgetAttrsA - Set gadget attributes */
void _gadtools_GT_SetGadgetAttrsA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                    register struct Gadget *gad __asm("a0"),
                                    register struct Window *win __asm("a1"),
                                    register struct Requester *req __asm("a2"),
                                    register struct TagItem *taglist __asm("a3") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_SetGadgetAttrsA() gad=0x%08lx\n", (ULONG)gad);
    /* Stub - would need to update gadget based on tags */
}

/*
 * Menu Functions
 */

/* CreateMenusA - Create menus from NewMenu array */
struct Menu * _gadtools_CreateMenusA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                       register struct NewMenu *newmenu __asm("a0"),
                                       register struct TagItem *taglist __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: CreateMenusA() newmenu=0x%08lx\n", (ULONG)newmenu);
    /* Stub - return NULL for now, apps should handle this gracefully */
    return NULL;
}

/* FreeMenus - Free menus created by CreateMenusA */
void _gadtools_FreeMenus ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                           register struct Menu *menu __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: FreeMenus() menu=0x%08lx\n", (ULONG)menu);
    /* Stub - nothing to free if CreateMenusA returns NULL */
}

/* LayoutMenuItemsA - Layout menu items */
BOOL _gadtools_LayoutMenuItemsA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                  register struct MenuItem *firstitem __asm("a0"),
                                  register APTR vi __asm("a1"),
                                  register struct TagItem *taglist __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: LayoutMenuItemsA()\n");
    return TRUE;
}

/* LayoutMenusA - Layout entire menu structure */
BOOL _gadtools_LayoutMenusA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                              register struct Menu *firstmenu __asm("a0"),
                              register APTR vi __asm("a1"),
                              register struct TagItem *taglist __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: LayoutMenusA()\n");
    return TRUE;
}

/*
 * Event Handling Functions
 */

/* GT_GetIMsg - Get an IntuiMessage, filtering GadTools messages */
struct IntuiMessage * _gadtools_GT_GetIMsg ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                             register struct MsgPort *iport __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_GetIMsg() iport=0x%08lx\n", (ULONG)iport);
    /* Just call GetMsg - no filtering needed for stub */
    return (struct IntuiMessage *)GetMsg(iport);
}

/* GT_ReplyIMsg - Reply to an IntuiMessage from GT_GetIMsg */
void _gadtools_GT_ReplyIMsg ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                              register struct IntuiMessage *imsg __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_ReplyIMsg() imsg=0x%08lx\n", (ULONG)imsg);
    if (imsg) {
        ReplyMsg((struct Message *)imsg);
    }
}

/* GT_RefreshWindow - Refresh all GadTools gadgets in a window */
void _gadtools_GT_RefreshWindow ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                  register struct Window *win __asm("a0"),
                                  register struct Requester *req __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_RefreshWindow() win=0x%08lx\n", (ULONG)win);
    /* Stub - would need to refresh gadgets */
}

/* GT_BeginRefresh - Begin a refresh operation */
void _gadtools_GT_BeginRefresh ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                 register struct Window *win __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_BeginRefresh() win=0x%08lx\n", (ULONG)win);
    /* Stub - call BeginRefresh if available */
}

/* GT_EndRefresh - End a refresh operation */
void _gadtools_GT_EndRefresh ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                               register struct Window *win __asm("a0"),
                               register BOOL complete __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_EndRefresh() win=0x%08lx, complete=%d\n", (ULONG)win, complete);
    /* Stub - call EndRefresh if available */
}

/* GT_FilterIMsg - Filter an IntuiMessage */
struct IntuiMessage * _gadtools_GT_FilterIMsg ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                                register struct IntuiMessage *imsg __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_FilterIMsg() imsg=0x%08lx\n", (ULONG)imsg);
    return imsg;  /* No filtering in stub */
}

/* GT_PostFilterIMsg - Post-filter an IntuiMessage */
struct IntuiMessage * _gadtools_GT_PostFilterIMsg ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                                    register struct IntuiMessage *imsg __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_PostFilterIMsg() imsg=0x%08lx\n", (ULONG)imsg);
    return imsg;  /* No filtering in stub */
}

/* CreateContext - Create a gadget list context */
struct Gadget * _gadtools_CreateContext ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                          register struct Gadget **glistptr __asm("a0") )
{
    struct Gadget *context;

    DPRINTF (LOG_DEBUG, "_gadtools: CreateContext() glistptr=0x%08lx\n", (ULONG)glistptr);

    if (!glistptr)
        return NULL;

    /* Allocate a dummy context gadget */
    context = AllocMem(sizeof(struct Gadget), MEMF_CLEAR | MEMF_PUBLIC);
    if (!context)
        return NULL;

    /* The context gadget is a placeholder, not a real gadget */
    context->GadgetType = GTYP_CUSTOMGADGET;
    context->Flags = GFLG_GADGHNONE;

    *glistptr = context;

    DPRINTF (LOG_DEBUG, "_gadtools: CreateContext() -> 0x%08lx\n", (ULONG)context);
    return context;
}

/*
 * Rendering Functions
 */

/* DrawBevelBoxA - Draw a 3D beveled box */
void _gadtools_DrawBevelBoxA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                               register struct RastPort *rport __asm("a0"),
                               register WORD left __asm("d0"),
                               register WORD top __asm("d1"),
                               register WORD width __asm("d2"),
                               register WORD height __asm("d3"),
                               register struct TagItem *taglist __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: DrawBevelBoxA() at %d,%d size %dx%d\n",
             left, top, width, height);
    /* Stub - would draw a 3D box if we had proper rendering */
}

/*
 * VisualInfo Functions
 */

/* GetVisualInfoA - Get visual info for a screen */
APTR _gadtools_GetVisualInfoA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                register struct Screen *screen __asm("a0"),
                                register struct TagItem *taglist __asm("a1") )
{
    struct VisualInfo *vi;

    DPRINTF (LOG_DEBUG, "_gadtools: GetVisualInfoA() screen=0x%08lx\n", (ULONG)screen);

    vi = AllocMem(sizeof(struct VisualInfo), MEMF_CLEAR | MEMF_PUBLIC);
    if (!vi)
        return NULL;

    vi->vi_Screen = screen;
    vi->vi_DrawInfo = NULL;  /* Would get from GetScreenDrawInfo() */

    DPRINTF (LOG_DEBUG, "_gadtools: GetVisualInfoA() -> 0x%08lx\n", (ULONG)vi);
    return vi;
}

/* FreeVisualInfo - Free visual info */
void _gadtools_FreeVisualInfo ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                register APTR vi __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: FreeVisualInfo() vi=0x%08lx\n", (ULONG)vi);
    if (vi) {
        FreeMem(vi, sizeof(struct VisualInfo));
    }
}

/*
 * Private/Reserved Functions
 */

static void _gadtools_Private1 ( void ) { }
static void _gadtools_Private2 ( void ) { }
static void _gadtools_Private3 ( void ) { }
static void _gadtools_Private4 ( void ) { }
static void _gadtools_Private5 ( void ) { }
static void _gadtools_Private6 ( void ) { }

/* GT_GetGadgetAttrsA - Get gadget attributes (V39+) */
LONG _gadtools_GT_GetGadgetAttrsA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                    register struct Gadget *gad __asm("a0"),
                                    register struct Window *win __asm("a1"),
                                    register struct Requester *req __asm("a2"),
                                    register struct TagItem *taglist __asm("a3") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_GetGadgetAttrsA() gad=0x%08lx\n", (ULONG)gad);
    return 0;  /* Return 0 = no tags processed */
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

extern APTR              __g_lxa_gadtools_FuncTab [];
extern struct MyDataInit __g_lxa_gadtools_DataTab;
extern struct InitTable  __g_lxa_gadtools_InitTab;
extern APTR              __g_lxa_gadtools_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                      // UWORD rt_MatchWord
    &ROMTag,                            // struct Resident *rt_MatchTag
    &__g_lxa_gadtools_EndResident,      // APTR  rt_EndSkip
    RTF_AUTOINIT,                       // UBYTE rt_Flags
    VERSION,                            // UBYTE rt_Version
    NT_LIBRARY,                         // UBYTE rt_Type
    0,                                  // BYTE  rt_Pri
    &_g_gadtools_ExLibName[0],          // char  *rt_Name
    &_g_gadtools_ExLibID[0],            // char  *rt_IdString
    &__g_lxa_gadtools_InitTab           // APTR  rt_Init
};

APTR __g_lxa_gadtools_EndResident;
struct Resident *__lxa_gadtools_ROMTag = &ROMTag;

struct InitTable __g_lxa_gadtools_InitTab =
{
    (ULONG)               sizeof(struct GadToolsBase),
    (APTR              *) &__g_lxa_gadtools_FuncTab[0],
    (APTR)                &__g_lxa_gadtools_DataTab,
    (APTR)                __g_lxa_gadtools_InitLib
};

/* Function table - from gadtools_lib.fd
 * Bias starts at -30 (first function after standard lib funcs)
 */
APTR __g_lxa_gadtools_FuncTab [] =
{
    __g_lxa_gadtools_OpenLib,           // -6   Standard
    __g_lxa_gadtools_CloseLib,          // -12  Standard
    __g_lxa_gadtools_ExpungeLib,        // -18  Standard
    __g_lxa_gadtools_ExtFuncLib,        // -24  Standard (reserved)
    _gadtools_CreateGadgetA,            // -30  CreateGadgetA
    _gadtools_FreeGadgets,              // -36  FreeGadgets
    _gadtools_GT_SetGadgetAttrsA,       // -42  GT_SetGadgetAttrsA
    _gadtools_CreateMenusA,             // -48  CreateMenusA
    _gadtools_FreeMenus,                // -54  FreeMenus
    _gadtools_LayoutMenuItemsA,         // -60  LayoutMenuItemsA
    _gadtools_LayoutMenusA,             // -66  LayoutMenusA
    _gadtools_GT_GetIMsg,               // -72  GT_GetIMsg
    _gadtools_GT_ReplyIMsg,             // -78  GT_ReplyIMsg
    _gadtools_GT_RefreshWindow,         // -84  GT_RefreshWindow
    _gadtools_GT_BeginRefresh,          // -90  GT_BeginRefresh
    _gadtools_GT_EndRefresh,            // -96  GT_EndRefresh
    _gadtools_GT_FilterIMsg,            // -102 GT_FilterIMsg
    _gadtools_GT_PostFilterIMsg,        // -108 GT_PostFilterIMsg
    _gadtools_CreateContext,            // -114 CreateContext
    _gadtools_DrawBevelBoxA,            // -120 DrawBevelBoxA
    _gadtools_GetVisualInfoA,           // -126 GetVisualInfoA
    _gadtools_FreeVisualInfo,           // -132 FreeVisualInfo
    _gadtools_Private1,                 // -138 Private1
    _gadtools_Private2,                 // -144 Private2
    _gadtools_Private3,                 // -150 Private3
    _gadtools_Private4,                 // -156 Private4
    _gadtools_Private5,                 // -162 Private5
    _gadtools_Private6,                 // -168 Private6
    _gadtools_GT_GetGadgetAttrsA,       // -174 GT_GetGadgetAttrsA (V39)
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_gadtools_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_gadtools_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_gadtools_ExLibID[0],
    (ULONG) 0
};
