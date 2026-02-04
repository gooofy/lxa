/*
 * lxa workbench.library implementation
 *
 * Provides the Workbench API for interacting with the Workbench.
 * This is a stub implementation - most functions are no-ops since
 * lxa doesn't have a traditional Workbench environment.
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

#include <utility/tagitem.h>

#include "util.h"

#define VERSION    44
#define REVISION   1
#define EXLIBNAME  "workbench"
#define EXLIBVER   " 44.1 (2025/02/02)"

char __aligned _g_workbench_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_workbench_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_workbench_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_workbench_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* WorkbenchBase structure */
struct WorkbenchBase {
    struct Library lib;
    BPTR           SegList;
};

/*
 * Library init/open/close/expunge
 */

struct WorkbenchBase * __g_lxa_workbench_InitLib ( register struct WorkbenchBase *wbb __asm("d0"),
                                                   register BPTR                seglist __asm("a0"),
                                                   register struct ExecBase    *sysb    __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_workbench: InitLib() called\n");
    wbb->SegList = seglist;
    return wbb;
}

BPTR __g_lxa_workbench_ExpungeLib ( register struct WorkbenchBase *wbb __asm("a6") )
{
    return 0;
}

struct WorkbenchBase * __g_lxa_workbench_OpenLib ( register struct WorkbenchBase *wbb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_workbench: OpenLib() called, wbb=0x%08lx\n", (ULONG)wbb);
    wbb->lib.lib_OpenCnt++;
    wbb->lib.lib_Flags &= ~LIBF_DELEXP;
    return wbb;
}

BPTR __g_lxa_workbench_CloseLib ( register struct WorkbenchBase *wbb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_workbench: CloseLib() called, wbb=0x%08lx\n", (ULONG)wbb);
    wbb->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_workbench_ExtFuncLib ( void )
{
    return 0;
}

/*
 * Workbench Functions (V36+)
 */

/* UpdateWorkbench - Notify Workbench of changes to an object */
void _workbench_UpdateWorkbench ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                  register CONST_STRPTR name __asm("a0"),
                                  register BPTR lock __asm("a1"),
                                  register LONG action __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_workbench: UpdateWorkbench() name='%s', action=%ld (stub)\n",
             name ? (char *)name : "(null)", action);
    /* No-op - we don't have a running Workbench */
}

/* AddAppWindowA - Add an AppWindow (app that receives dropped icons) */
APTR _workbench_AddAppWindowA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                register ULONG id __asm("d0"),
                                register ULONG userdata __asm("d1"),
                                register struct Window *window __asm("a0"),
                                register struct MsgPort *msgport __asm("a1"),
                                register struct TagItem *taglist __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_workbench: AddAppWindowA() id=%ld, window=0x%08lx (stub)\n",
             id, (ULONG)window);
    /* Return non-NULL to indicate success, but we don't actually do anything */
    return (APTR)0x12345678;
}

/* RemoveAppWindow - Remove an AppWindow */
BOOL _workbench_RemoveAppWindow ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                  register APTR appWindow __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_workbench: RemoveAppWindow() appWindow=0x%08lx (stub)\n", (ULONG)appWindow);
    return TRUE;
}

/* AddAppIconA - Add an AppIcon to Workbench */
APTR _workbench_AddAppIconA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                              register ULONG id __asm("d0"),
                              register ULONG userdata __asm("d1"),
                              register CONST_STRPTR text __asm("a0"),
                              register struct MsgPort *msgport __asm("a1"),
                              register BPTR lock __asm("a2"),
                              register APTR diskobj __asm("a3"),
                              register struct TagItem *taglist __asm("a4") )
{
    DPRINTF (LOG_DEBUG, "_workbench: AddAppIconA() id=%ld, text='%s' (stub)\n",
             id, text ? (char *)text : "(null)");
    /* Return non-NULL to indicate success */
    return (APTR)0x12345679;
}

/* RemoveAppIcon - Remove an AppIcon */
BOOL _workbench_RemoveAppIcon ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                register APTR appIcon __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_workbench: RemoveAppIcon() appIcon=0x%08lx (stub)\n", (ULONG)appIcon);
    return TRUE;
}

/* AddAppMenuItemA - Add an item to Workbench Tools menu */
APTR _workbench_AddAppMenuItemA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                  register ULONG id __asm("d0"),
                                  register ULONG userdata __asm("d1"),
                                  register CONST_STRPTR text __asm("a0"),
                                  register struct MsgPort *msgport __asm("a1"),
                                  register struct TagItem *taglist __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_workbench: AddAppMenuItemA() id=%ld, text='%s' (stub)\n",
             id, text ? (char *)text : "(null)");
    /* Return non-NULL to indicate success */
    return (APTR)0x1234567a;
}

/* RemoveAppMenuItem - Remove a menu item */
BOOL _workbench_RemoveAppMenuItem ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                    register APTR appMenuItem __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_workbench: RemoveAppMenuItem() appMenuItem=0x%08lx (stub)\n", (ULONG)appMenuItem);
    return TRUE;
}

/*
 * Workbench Functions (V39+)
 */

/* WBInfo - Display info requester for an object */
ULONG _workbench_WBInfo ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                          register BPTR lock __asm("a0"),
                          register CONST_STRPTR name __asm("a1"),
                          register struct Screen *screen __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_workbench: WBInfo() name='%s' (stub)\n",
             name ? (char *)name : "(null)");
    return 0;  /* Return 0 = failure/not available */
}

/*
 * Workbench Functions (V44+)
 */

/* OpenWorkbenchObjectA - Open an object as Workbench would */
BOOL _workbench_OpenWorkbenchObjectA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                       register CONST_STRPTR name __asm("a0"),
                                       register struct TagItem *tags __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_workbench: OpenWorkbenchObjectA() name='%s' (stub)\n",
             name ? (char *)name : "(null)");
    return FALSE;  /* Not implemented */
}

/* CloseWorkbenchObjectA - Close an opened Workbench object */
BOOL _workbench_CloseWorkbenchObjectA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                        register CONST_STRPTR name __asm("a0"),
                                        register struct TagItem *tags __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_workbench: CloseWorkbenchObjectA() name='%s' (stub)\n",
             name ? (char *)name : "(null)");
    return FALSE;  /* Not implemented */
}

/* Reserved/Private functions */
static ULONG _workbench_Private1 ( void ) { return 0; }
static ULONG _workbench_Private2 ( void ) { return 0; }
static ULONG _workbench_Private3 ( void ) { return 0; }
static ULONG _workbench_Private4 ( void ) { return 0; }

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

extern APTR              __g_lxa_workbench_FuncTab [];
extern struct MyDataInit __g_lxa_workbench_DataTab;
extern struct InitTable  __g_lxa_workbench_InitTab;
extern APTR              __g_lxa_workbench_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                        // UWORD rt_MatchWord
    &ROMTag,                              // struct Resident *rt_MatchTag
    &__g_lxa_workbench_EndResident,       // APTR  rt_EndSkip
    RTF_AUTOINIT,                         // UBYTE rt_Flags
    VERSION,                              // UBYTE rt_Version
    NT_LIBRARY,                           // UBYTE rt_Type
    0,                                    // BYTE  rt_Pri
    &_g_workbench_ExLibName[0],           // char  *rt_Name
    &_g_workbench_ExLibID[0],             // char  *rt_IdString
    &__g_lxa_workbench_InitTab            // APTR  rt_Init
};

APTR __g_lxa_workbench_EndResident;
struct Resident *__lxa_workbench_ROMTag = &ROMTag;

struct InitTable __g_lxa_workbench_InitTab =
{
    (ULONG)               sizeof(struct WorkbenchBase),
    (APTR              *) &__g_lxa_workbench_FuncTab[0],
    (APTR)                &__g_lxa_workbench_DataTab,
    (APTR)                __g_lxa_workbench_InitLib
};

/* Function table - based on wb_protos.h
 * Standard library functions at -6 through -24
 * First real function at -30
 */
APTR __g_lxa_workbench_FuncTab [] =
{
    __g_lxa_workbench_OpenLib,           // -6   Standard
    __g_lxa_workbench_CloseLib,          // -12  Standard
    __g_lxa_workbench_ExpungeLib,        // -18  Standard
    __g_lxa_workbench_ExtFuncLib,        // -24  Standard (reserved)
    _workbench_Private1,                 // -30  Private (V36)
    _workbench_UpdateWorkbench,          // -36  UpdateWorkbench (V36)
    _workbench_Private2,                 // -42  Private (V36)
    _workbench_AddAppWindowA,            // -48  AddAppWindowA (V36)
    _workbench_RemoveAppWindow,          // -54  RemoveAppWindow (V36)
    _workbench_AddAppIconA,              // -60  AddAppIconA (V36)
    _workbench_RemoveAppIcon,            // -66  RemoveAppIcon (V36)
    _workbench_AddAppMenuItemA,          // -72  AddAppMenuItemA (V36)
    _workbench_RemoveAppMenuItem,        // -78  RemoveAppMenuItem (V36)
    _workbench_Private3,                 // -84  Private (V39)
    _workbench_WBInfo,                   // -90  WBInfo (V39)
    _workbench_Private4,                 // -96  Private (V44)
    _workbench_OpenWorkbenchObjectA,     // -102 OpenWorkbenchObjectA (V44)
    _workbench_CloseWorkbenchObjectA,    // -108 CloseWorkbenchObjectA (V44)
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_workbench_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_workbench_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_workbench_ExLibID[0],
    (ULONG) 0
};
