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

#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>

#include <libraries/gadtools.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

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
extern struct GfxBase  *GfxBase;
extern struct UtilityBase *UtilityBase;

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

struct GadToolsBase * __g_lxa_gadtools_InitLib ( register struct GadToolsBase *gtb    __asm("d0"),
                                                 register BPTR                seglist __asm("a0"),
                                                 register struct ExecBase    *sysb    __asm("a6"))
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
        {
            struct StringInfo *si;
            STRPTR buf;
            ULONG maxchars;
            STRPTR initstr;
            WORD len;

            newgad->GadgetType = GTYP_STRGADGET;
            newgad->Activation = GACT_RELVERIFY;
            newgad->Flags = GFLG_GADGHCOMP;

            /* Determine max chars from tags (default 128 per GadTools convention) */
            if (kind == INTEGER_KIND)
                maxchars = GetTagData(GTIN_MaxChars, 10, taglist);
            else
                maxchars = GetTagData(GTST_MaxChars, 128, taglist);

            /* Allocate StringInfo structure */
            si = (struct StringInfo *)AllocMem(sizeof(struct StringInfo), MEMF_CLEAR | MEMF_PUBLIC);
            if (!si)
            {
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            /* Allocate buffer (+1 for NUL already included in MaxChars convention) */
            buf = (STRPTR)AllocMem(maxchars, MEMF_CLEAR | MEMF_PUBLIC);
            if (!buf)
            {
                FreeMem(si, sizeof(struct StringInfo));
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            si->Buffer = buf;
            si->MaxChars = (WORD)maxchars;

            /* Copy initial string into buffer */
            if (kind == INTEGER_KIND)
            {
                LONG num = (LONG)GetTagData(GTIN_Number, 0, taglist);
                si->LongInt = num;
                /* Convert number to string in buffer */
                {
                    LONG n = num;
                    WORD i = 0;
                    WORD start, end;
                    if (n < 0)
                    {
                        buf[i++] = '-';
                        n = -n;
                    }
                    if (n == 0)
                    {
                        buf[i++] = '0';
                    }
                    else
                    {
                        /* Write digits in reverse, then reverse them */
                        start = i;
                        while (n > 0 && i < maxchars - 1)
                        {
                            buf[i++] = '0' + (UBYTE)(n % 10);
                            n /= 10;
                        }
                        /* Reverse the digit portion */
                        end = i - 1;
                        while (start < end)
                        {
                            UBYTE tmp = buf[start];
                            buf[start] = buf[end];
                            buf[end] = tmp;
                            start++;
                            end--;
                        }
                    }
                    buf[i] = '\0';
                    si->NumChars = i;
                }
            }
            else
            {
                initstr = (STRPTR)GetTagData(GTST_String, 0, taglist);
                if (initstr)
                {
                    len = 0;
                    while (initstr[len] != '\0' && len < maxchars - 1)
                    {
                        buf[len] = initstr[len];
                        len++;
                    }
                    buf[len] = '\0';
                    si->NumChars = len;
                }
                else
                {
                    buf[0] = '\0';
                    si->NumChars = 0;
                }
            }

            si->BufferPos = si->NumChars;
            newgad->SpecialInfo = (APTR)si;

            DPRINTF (LOG_DEBUG, "_gadtools: CreateGadgetA() STRING/INTEGER: si=0x%08lx, buf='%s', maxchars=%ld\n",
                     (ULONG)si, STRORNULL(buf), maxchars);
            break;
        }
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

        /* Free StringInfo and buffer for string gadgets */
        if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET && gad->SpecialInfo)
        {
            struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
            if (si->Buffer)
                FreeMem(si->Buffer, si->MaxChars);
            FreeMem(si, sizeof(struct StringInfo));
        }

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

/* Forward declaration */
static void FreeMenuItems(struct MenuItem *item);
void _gadtools_FreeMenus ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                           register struct Menu *menu __asm("a0") );

/* CreateMenusA - Create menus from NewMenu array
 *
 * Parses the NewMenu array and creates the corresponding Menu and MenuItem
 * structures. The menu hierarchy is:
 *   Menu (title bar entry) -> MenuItem (dropdown item) -> SubItem (submenu item)
 */
struct Menu * _gadtools_CreateMenusA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                       register struct NewMenu *newmenu __asm("a0"),
                                       register struct TagItem *taglist __asm("a1") )
{
    struct Menu *firstMenu = NULL;
    struct Menu *currentMenu = NULL;
    struct Menu *lastMenu = NULL;
    struct MenuItem *currentItem = NULL;
    struct MenuItem *lastItem = NULL;
    struct MenuItem *lastSubItem = NULL;
    struct IntuiText *itext;
    struct NewMenu *nm;
    WORD menuLeft = 0;

    DPRINTF (LOG_DEBUG, "_gadtools: CreateMenusA() newmenu=0x%08lx\n", (ULONG)newmenu);

    if (!newmenu)
        return NULL;

    /* First pass: count entries and validate */
    for (nm = newmenu; nm->nm_Type != NM_END; nm++) {
        if (nm->nm_Type == NM_IGNORE)
            continue;
        /* Just validate we have at least one menu title */
        if (nm->nm_Type == NM_TITLE && firstMenu == NULL) {
            /* Will create first menu */
        }
    }

    /* Second pass: create the menu structures */
    for (nm = newmenu; nm->nm_Type != NM_END; nm++) {
        UBYTE type = nm->nm_Type & ~MENU_IMAGE;  /* Strip image flag */

        if (nm->nm_Type == NM_IGNORE)
            continue;

        DPRINTF (LOG_DEBUG, "_gadtools: CreateMenusA: type=%d label='%s'\n",
                 type, nm->nm_Label ? (nm->nm_Label == (STRPTR)-1 ? "(bar)" : (char*)nm->nm_Label) : "(null)");

        switch (type) {
            case NM_TITLE: {
                /* Create a new Menu structure */
                struct Menu *menu = AllocMem(sizeof(struct Menu), MEMF_CLEAR | MEMF_PUBLIC);
                if (!menu) {
                    /* Out of memory - free what we have and return NULL */
                    if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                menu->LeftEdge = menuLeft;
                menu->TopEdge = 0;
                menu->Width = 80;  /* Will be adjusted by LayoutMenusA */
                menu->Height = 10;
                menu->Flags = MENUENABLED;
                menu->MenuName = nm->nm_Label;
                menu->FirstItem = NULL;
                menu->NextMenu = NULL;

                /* Apply flags from NewMenu */
                if (nm->nm_Flags & NM_MENUDISABLED)
                    menu->Flags &= ~MENUENABLED;

                /* Estimate width for next menu position */
                if (nm->nm_Label) {
                    int len = 0;
                    CONST_STRPTR s = nm->nm_Label;
                    while (*s++) len++;
                    menu->Width = (len + 2) * 8;  /* Rough estimate */
                }
                menuLeft += menu->Width;

                /* Link to menu chain */
                if (!firstMenu) {
                    firstMenu = menu;
                } else if (lastMenu) {
                    lastMenu->NextMenu = menu;
                }
                lastMenu = menu;
                currentMenu = menu;
                currentItem = NULL;
                lastItem = NULL;
                lastSubItem = NULL;
                break;
            }

            case NM_ITEM: {
                /* Create a MenuItem for the current menu */
                struct MenuItem *item;

                if (!currentMenu) {
                    DPRINTF (LOG_ERROR, "_gadtools: CreateMenusA: NM_ITEM without NM_TITLE!\n");
                    continue;
                }

                item = AllocMem(sizeof(struct MenuItem), MEMF_CLEAR | MEMF_PUBLIC);
                if (!item) {
                    if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                item->LeftEdge = 0;
                item->TopEdge = lastItem ? (lastItem->TopEdge + lastItem->Height) : 0;
                item->Width = 150;  /* Will be adjusted by LayoutMenuItemsA */
                item->Height = 10;
                item->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
                item->MutualExclude = nm->nm_MutualExclude;
                item->NextItem = NULL;
                item->SubItem = NULL;
                item->Command = 0;

                /* Handle bar label (separator) */
                if (nm->nm_Label == NM_BARLABEL) {
                    /* Create a simple separator - we'll use a minimal IntuiText */
                    itext = AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
                    if (itext) {
                        itext->FrontPen = 1;
                        itext->BackPen = 0;
                        itext->DrawMode = JAM1;
                        itext->LeftEdge = 0;
                        itext->TopEdge = 0;
                        itext->ITextFont = NULL;
                        itext->IText = (STRPTR)"----------------";
                        itext->NextText = NULL;
                    }
                    item->ItemFill = itext;
                    item->Flags &= ~ITEMENABLED;  /* Separators are not selectable */
                    item->Height = 6;  /* Shorter height for separators */
                } else {
                    /* Create IntuiText for the label */
                    itext = AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
                    if (itext) {
                        itext->FrontPen = 0;  /* Will use screen colors */
                        itext->BackPen = 1;
                        itext->DrawMode = JAM1;
                        itext->LeftEdge = 2;
                        itext->TopEdge = 1;
                        itext->ITextFont = NULL;
                        itext->IText = (STRPTR)nm->nm_Label;
                        itext->NextText = NULL;
                    }
                    item->ItemFill = itext;
                }

                /* Handle command key */
                if (nm->nm_CommKey && nm->nm_CommKey[0]) {
                    item->Flags |= COMMSEQ;
                    item->Command = nm->nm_CommKey[0];
                }

                /* Handle checkmark */
                if (nm->nm_Flags & CHECKIT) {
                    item->Flags |= CHECKIT;
                    if (nm->nm_Flags & CHECKED)
                        item->Flags |= CHECKED;
                    if (nm->nm_Flags & MENUTOGGLE)
                        item->Flags |= MENUTOGGLE;
                }

                /* Handle disabled items */
                if (nm->nm_Flags & NM_ITEMDISABLED)
                    item->Flags &= ~ITEMENABLED;

                /* Store user data */
                /* Note: On real AmigaOS, UserData is stored in an extended structure */

                /* Link to menu */
                if (!currentMenu->FirstItem) {
                    currentMenu->FirstItem = item;
                } else if (lastItem) {
                    lastItem->NextItem = item;
                }
                lastItem = item;
                currentItem = item;
                lastSubItem = NULL;
                break;
            }

            case NM_SUB: {
                /* Create a sub-menu item for the current item */
                struct MenuItem *subitem;

                if (!currentItem) {
                    DPRINTF (LOG_ERROR, "_gadtools: CreateMenusA: NM_SUB without NM_ITEM!\n");
                    continue;
                }

                subitem = AllocMem(sizeof(struct MenuItem), MEMF_CLEAR | MEMF_PUBLIC);
                if (!subitem) {
                    if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                subitem->LeftEdge = currentItem->Width;
                subitem->TopEdge = lastSubItem ? (lastSubItem->TopEdge + lastSubItem->Height) : 0;
                subitem->Width = 120;
                subitem->Height = 10;
                subitem->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
                subitem->MutualExclude = nm->nm_MutualExclude;
                subitem->NextItem = NULL;
                subitem->SubItem = NULL;
                subitem->Command = 0;

                /* Handle bar label (separator) */
                if (nm->nm_Label == NM_BARLABEL) {
                    itext = AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
                    if (itext) {
                        itext->FrontPen = 1;
                        itext->BackPen = 0;
                        itext->DrawMode = JAM1;
                        itext->IText = (STRPTR)"------------";
                    }
                    subitem->ItemFill = itext;
                    subitem->Flags &= ~ITEMENABLED;
                    subitem->Height = 6;
                } else {
                    itext = AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
                    if (itext) {
                        itext->FrontPen = 0;
                        itext->BackPen = 1;
                        itext->DrawMode = JAM1;
                        itext->LeftEdge = 2;
                        itext->TopEdge = 1;
                        itext->IText = (STRPTR)nm->nm_Label;
                    }
                    subitem->ItemFill = itext;
                }

                /* Handle command key */
                if (nm->nm_CommKey && nm->nm_CommKey[0]) {
                    subitem->Flags |= COMMSEQ;
                    subitem->Command = nm->nm_CommKey[0];
                }

                /* Handle checkmark and flags */
                if (nm->nm_Flags & CHECKIT) {
                    subitem->Flags |= CHECKIT;
                    if (nm->nm_Flags & CHECKED)
                        subitem->Flags |= CHECKED;
                    if (nm->nm_Flags & MENUTOGGLE)
                        subitem->Flags |= MENUTOGGLE;
                }

                if (nm->nm_Flags & NM_ITEMDISABLED)
                    subitem->Flags &= ~ITEMENABLED;

                /* Link to parent item */
                if (!currentItem->SubItem) {
                    currentItem->SubItem = subitem;
                } else if (lastSubItem) {
                    lastSubItem->NextItem = subitem;
                }
                lastSubItem = subitem;
                break;
            }
        }
    }

    DPRINTF (LOG_DEBUG, "_gadtools: CreateMenusA() -> 0x%08lx\n", (ULONG)firstMenu);
    return firstMenu;
}

/* Helper: Free a chain of menu items recursively */
static void FreeMenuItems(struct MenuItem *item)
{
    while (item) {
        struct MenuItem *next = item->NextItem;

        /* Free sub-items first */
        if (item->SubItem) {
            FreeMenuItems(item->SubItem);
        }

        /* Free the IntuiText if we created one */
        if ((item->Flags & ITEMTEXT) && item->ItemFill) {
            FreeMem(item->ItemFill, sizeof(struct IntuiText));
        }

        FreeMem(item, sizeof(struct MenuItem));
        item = next;
    }
}

/* FreeMenus - Free menus created by CreateMenusA */
void _gadtools_FreeMenus ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                           register struct Menu *menu __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: FreeMenus() menu=0x%08lx\n", (ULONG)menu);

    while (menu) {
        struct Menu *nextMenu = menu->NextMenu;

        /* Free all items in this menu */
        if (menu->FirstItem) {
            FreeMenuItems(menu->FirstItem);
        }

        /* Free the menu itself */
        FreeMem(menu, sizeof(struct Menu));
        menu = nextMenu;
    }
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

/* DrawBevelBoxA - Draw a 3D beveled box
 *
 * Supported tags:
 *   GTBB_Recessed  - Draw recessed (sunken) instead of raised box
 *   GT_VisualInfo  - VisualInfo for pen colors
 *   GTBB_FrameType - Frame type (BBFT_BUTTON, BBFT_RIDGE, etc.)
 */
void _gadtools_DrawBevelBoxA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                               register struct RastPort *rport __asm("a0"),
                               register WORD left __asm("d0"),
                               register WORD top __asm("d1"),
                               register WORD width __asm("d2"),
                               register WORD height __asm("d3"),
                               register struct TagItem *taglist __asm("a1") )
{
    struct VisualInfo *vi = NULL;
    BOOL recessed = FALSE;
    ULONG frameType = BBFT_BUTTON;
    UBYTE shiPen = 2;    /* Default shine pen */
    UBYTE shaPen = 1;    /* Default shadow pen */
    WORD x0, y0, x1, y1;
    UBYTE topPen, botPen;
    UBYTE savedAPen;
    struct TagItem *tag;
    
    DPRINTF (LOG_DEBUG, "_gadtools: DrawBevelBoxA() at %d,%d size %dx%d\n",
             left, top, width, height);
    
    if (!rport || width <= 0 || height <= 0)
        return;
    
    /* Parse tags manually without NextTagItem to avoid UtilityBase dependency */
    if (taglist) {
        for (tag = taglist; tag->ti_Tag != TAG_DONE; tag++) {
            if (tag->ti_Tag == TAG_SKIP) {
                tag += tag->ti_Data;
                continue;
            }
            if (tag->ti_Tag == TAG_IGNORE)
                continue;
            if (tag->ti_Tag == TAG_MORE) {
                tag = (struct TagItem *)tag->ti_Data;
                if (!tag) break;
                tag--;  /* Will be incremented by loop */
                continue;
            }
            
            switch (tag->ti_Tag) {
                case GTBB_Recessed:
                    recessed = (BOOL)tag->ti_Data;
                    break;
                case GT_VisualInfo:
                    vi = (struct VisualInfo *)tag->ti_Data;
                    break;
                case GTBB_FrameType:
                    frameType = tag->ti_Data;
                    break;
            }
        }
    }
    
    /* Get pen colors from VisualInfo if available */
    if (vi && vi->vi_DrawInfo && vi->vi_DrawInfo->dri_Pens) {
        shiPen = vi->vi_DrawInfo->dri_Pens[SHINEPEN];
        shaPen = vi->vi_DrawInfo->dri_Pens[SHADOWPEN];
    }
    
    /* Save current pen */
    savedAPen = rport->FgPen;
    
    /* Calculate corners */
    x0 = left;
    y0 = top;
    x1 = left + width - 1;
    y1 = top + height - 1;
    
    /* Determine which pen goes where based on recessed state */
    topPen = recessed ? shaPen : shiPen;
    botPen = recessed ? shiPen : shaPen;
    
    switch (frameType) {
        case BBFT_RIDGE:
            /* Double border: outer raised, inner recessed */
            /* Outer frame - raised */
            SetAPen(rport, shiPen);
            Move(rport, x0, y1);
            Draw(rport, x0, y0);
            Draw(rport, x1, y0);
            SetAPen(rport, shaPen);
            Draw(rport, x1, y1);
            Draw(rport, x0, y1);
            
            /* Inner frame - recessed (inset by 1 pixel) */
            if (width > 2 && height > 2) {
                SetAPen(rport, shaPen);
                Move(rport, x0 + 1, y1 - 1);
                Draw(rport, x0 + 1, y0 + 1);
                Draw(rport, x1 - 1, y0 + 1);
                SetAPen(rport, shiPen);
                Draw(rport, x1 - 1, y1 - 1);
                Draw(rport, x0 + 1, y1 - 1);
            }
            break;
            
        case BBFT_ICONDROPBOX:
            /* Similar to ridge but with different appearance */
            /* Fall through to default for now */
        case BBFT_DISPLAY:
            /* Display box - usually recessed */
            topPen = shaPen;
            botPen = shiPen;
            /* Fall through */
        case BBFT_BUTTON:
        default:
            /* Standard single bevel box */
            /* Top and left edges (light or dark based on recessed) */
            SetAPen(rport, topPen);
            Move(rport, x0, y1 - 1);  /* Start at bottom-left, one pixel up */
            Draw(rport, x0, y0);       /* Draw up to top-left */
            Draw(rport, x1 - 1, y0);   /* Draw across to top-right (minus corner) */
            
            /* Bottom and right edges */
            SetAPen(rport, botPen);
            Move(rport, x1, y0);       /* Start at top-right */
            Draw(rport, x1, y1);       /* Draw down to bottom-right */
            Draw(rport, x0, y1);       /* Draw across to bottom-left */
            break;
    }
    
    /* Restore pen */
    SetAPen(rport, savedAPen);
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
    _gadtools_Private1,                 // -138 SetDesignFontA (V47) - stub
    _gadtools_Private2,                 // -144 ScaleGadgetRectA (V47) - stub
    _gadtools_Private3,                 // -150 gadtoolsPrivate1
    _gadtools_Private4,                 // -156 gadtoolsPrivate2
    _gadtools_Private5,                 // -162 gadtoolsPrivate3
    _gadtools_Private6,                 // -168 gadtoolsPrivate4
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
