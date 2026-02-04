/*
 * lxa reqtools.library implementation
 *
 * Provides the ReqTools requester API (popular third-party library).
 * This is a stub implementation - requesters will fail but apps
 * should handle this gracefully.
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
#include <intuition/intuition.h>
#include <graphics/text.h>

#include "util.h"

#define VERSION    38
#define REVISION   1
#define EXLIBNAME  "reqtools"
#define EXLIBVER   " 38.1 (2025/06/23)"

char __aligned _g_reqtools_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_reqtools_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_reqtools_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_reqtools_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* ReqToolsBase structure */
struct ReqToolsBase
{
    struct Library lib;
    BPTR           SegList;
};

/* Request types */
#define RT_FILEREQ     0
#define RT_REQINFO     1
#define RT_FONTREQ     2
#define RT_SCREENMODEREQ 3

/*
 * Library init/open/close/expunge
 */

struct ReqToolsBase * __g_lxa_reqtools_InitLib ( register struct ReqToolsBase *rtbase __asm("d0"),
                                                  register BPTR                 seglist __asm("a0"),
                                                  register struct ExecBase     *sysb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_reqtools: InitLib() called\n");
    rtbase->SegList = seglist;
    return rtbase;
}

BPTR __g_lxa_reqtools_ExpungeLib ( register struct ReqToolsBase *rtbase __asm("a6") )
{
    return 0;
}

struct ReqToolsBase * __g_lxa_reqtools_OpenLib ( register struct ReqToolsBase *rtbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: OpenLib() called, rtbase=0x%08lx\n", (ULONG)rtbase);
    rtbase->lib.lib_OpenCnt++;
    rtbase->lib.lib_Flags &= ~LIBF_DELEXP;
    return rtbase;
}

BPTR __g_lxa_reqtools_CloseLib ( register struct ReqToolsBase *rtbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: CloseLib() called, rtbase=0x%08lx\n", (ULONG)rtbase);
    rtbase->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_reqtools_ExtFuncLib ( void )
{
    return 0;
}

/*
 * ReqTools Functions
 */

/* rtAllocRequestA - Allocate a requester structure */
APTR _reqtools_rtAllocRequestA ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                 register ULONG type __asm("d0"),
                                 register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtAllocRequestA() type=%ld (stub, returns NULL)\n", type);
    /* Return NULL - we don't support ReqTools requesters */
    return NULL;
}

/* rtFreeRequest - Free a requester structure */
void _reqtools_rtFreeRequest ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                               register APTR req __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtFreeRequest() req=0x%08lx (stub)\n", (ULONG)req);
}

/* rtFreeReqBuffer - Free requester buffer */
void _reqtools_rtFreeReqBuffer ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                 register APTR req __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtFreeReqBuffer() req=0x%08lx (stub)\n", (ULONG)req);
}

/* rtChangeReqAttrA - Change requester attributes */
LONG _reqtools_rtChangeReqAttrA ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                  register APTR req __asm("a1"),
                                  register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtChangeReqAttrA() req=0x%08lx (stub, returns 0)\n", (ULONG)req);
    return 0;
}

/* rtFileRequestA - Display a file requester */
APTR _reqtools_rtFileRequestA ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                register APTR filereq __asm("a1"),
                                register char *file __asm("a2"),
                                register char *title __asm("a3"),
                                register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtFileRequestA() title=%s (stub, returns NULL)\n",
             STRORNULL(title));
    return NULL;
}

/* rtFreeFileList - Free a file list */
void _reqtools_rtFreeFileList ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                register APTR filelist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtFreeFileList() (stub)\n");
}

/* rtEZRequestA - Display an easy requester */
ULONG _reqtools_rtEZRequestA ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                               register char *bodyfmt __asm("a1"),
                               register char *gadfmt __asm("a2"),
                               register APTR reqinfo __asm("a3"),
                               register APTR argarray __asm("a4"),
                               register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtEZRequestA() body=%s (stub, returns 0)\n",
             STRORNULL(bodyfmt));
    return 0;
}

/* rtGetStringA - Get a string from user */
ULONG _reqtools_rtGetStringA ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                               register UBYTE *buffer __asm("a1"),
                               register ULONG maxchars __asm("d0"),
                               register char *title __asm("a2"),
                               register APTR reqinfo __asm("a3"),
                               register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtGetStringA() title=%s maxchars=%ld (stub, returns 0)\n",
             STRORNULL(title), maxchars);
    return 0;
}

/* rtGetLongA - Get a long value from user */
ULONG _reqtools_rtGetLongA ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                             register ULONG *longptr __asm("a1"),
                             register char *title __asm("a2"),
                             register APTR reqinfo __asm("a3"),
                             register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtGetLongA() title=%s (stub, returns 0)\n",
             STRORNULL(title));
    return 0;
}

/* Reserved slots (84, 90) */
ULONG _reqtools_Reserved ( void )
{
    return 0;
}

/* rtFontRequestA - Display a font requester */
ULONG _reqtools_rtFontRequestA ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                 register APTR fontreq __asm("a1"),
                                 register char *title __asm("a3"),
                                 register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtFontRequestA() title=%s (stub, returns 0)\n",
             STRORNULL(title));
    return 0;
}

/* rtPaletteRequestA - Display a palette requester */
LONG _reqtools_rtPaletteRequestA ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                   register char *title __asm("a2"),
                                   register APTR reqinfo __asm("a3"),
                                   register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtPaletteRequestA() title=%s (stub, returns -1)\n",
             STRORNULL(title));
    return -1;
}

/* rtReqHandlerA - Handle requester input */
ULONG _reqtools_rtReqHandlerA ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                register APTR handlerinfo __asm("a1"),
                                register ULONG sigs __asm("d0"),
                                register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtReqHandlerA() (stub, returns 0)\n");
    return 0;
}

/* rtSetWaitPointer - Set wait pointer on window */
void _reqtools_rtSetWaitPointer ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                  register struct Window *window __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtSetWaitPointer() window=0x%08lx (stub)\n", (ULONG)window);
}

/* rtGetVScreenSize - Get virtual screen size */
ULONG _reqtools_rtGetVScreenSize ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                   register struct Screen *screen __asm("a0"),
                                   register ULONG *widthptr __asm("a1"),
                                   register ULONG *heightptr __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtGetVScreenSize() screen=0x%08lx (stub)\n", (ULONG)screen);
    if (widthptr) *widthptr = 640;
    if (heightptr) *heightptr = 256;
    return TRUE;
}

/* rtSetReqPosition - Set requester position */
void _reqtools_rtSetReqPosition ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                  register ULONG reqpos __asm("d0"),
                                  register struct NewWindow *newwindow __asm("a0"),
                                  register struct Screen *screen __asm("a1"),
                                  register struct Window *window __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtSetReqPosition() (stub)\n");
}

/* rtSpread - Spread gadgets evenly */
void _reqtools_rtSpread ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                          register ULONG *posarray __asm("a0"),
                          register ULONG *sizearray __asm("a1"),
                          register ULONG length __asm("d0"),
                          register ULONG min __asm("d1"),
                          register ULONG max __asm("d2"),
                          register ULONG num __asm("d3") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtSpread() (stub)\n");
}

/* rtScreenToFrontSafely - Bring screen to front safely */
void _reqtools_rtScreenToFrontSafely ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                       register struct Screen *screen __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtScreenToFrontSafely() screen=0x%08lx (stub)\n", (ULONG)screen);
}

/* rtScreenModeRequestA - Display a screenmode requester */
ULONG _reqtools_rtScreenModeRequestA ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                       register APTR screenmodereq __asm("a1"),
                                       register char *title __asm("a3"),
                                       register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtScreenModeRequestA() title=%s (stub, returns 0)\n",
             STRORNULL(title));
    return 0;
}

/* rtCloseWindowSafely - Close window safely */
void _reqtools_rtCloseWindowSafely ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                     register struct Window *win __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtCloseWindowSafely() win=0x%08lx (stub)\n", (ULONG)win);
}

/* rtLockWindow - Lock a window */
APTR _reqtools_rtLockWindow ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                              register struct Window *win __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtLockWindow() win=0x%08lx (stub, returns NULL)\n", (ULONG)win);
    return NULL;
}

/* rtUnlockWindow - Unlock a window */
void _reqtools_rtUnlockWindow ( register struct ReqToolsBase *ReqToolsBase __asm("a6"),
                                register struct Window *win __asm("a0"),
                                register APTR winlock __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtUnlockWindow() win=0x%08lx (stub)\n", (ULONG)win);
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

extern APTR              __g_lxa_reqtools_FuncTab [];
extern struct MyDataInit __g_lxa_reqtools_DataTab;
extern struct InitTable  __g_lxa_reqtools_InitTab;
extern APTR              __g_lxa_reqtools_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                       // UWORD rt_MatchWord
    &ROMTag,                             // struct Resident *rt_MatchTag
    &__g_lxa_reqtools_EndResident,       // APTR  rt_EndSkip
    RTF_AUTOINIT,                        // UBYTE rt_Flags
    VERSION,                             // UBYTE rt_Version
    NT_LIBRARY,                          // UBYTE rt_Type
    0,                                   // BYTE  rt_Pri
    &_g_reqtools_ExLibName[0],           // char  *rt_Name
    &_g_reqtools_ExLibID[0],             // char  *rt_IdString
    &__g_lxa_reqtools_InitTab            // APTR  rt_Init
};

APTR __g_lxa_reqtools_EndResident;
struct Resident *__lxa_reqtools_ROMTag = &ROMTag;

struct InitTable __g_lxa_reqtools_InitTab =
{
    (ULONG)               sizeof(struct ReqToolsBase),
    (APTR              *) &__g_lxa_reqtools_FuncTab[0],
    (APTR)                &__g_lxa_reqtools_DataTab,
    (APTR)                __g_lxa_reqtools_InitLib
};

/* Function table - from inline/reqtools_protos.h
 * Standard library functions at -6 through -24
 * First real function at -30
 *
 * Bias 30:  rtAllocRequestA      (d0/a0)
 * Bias 36:  rtFreeRequest        (a1)
 * Bias 42:  rtFreeReqBuffer      (a1)
 * Bias 48:  rtChangeReqAttrA     (a1/a0)
 * Bias 54:  rtFileRequestA       (a1/a2/a3/a0)
 * Bias 60:  rtFreeFileList       (a0)
 * Bias 66:  rtEZRequestA         (a1/a2/a3/a4/a0)
 * Bias 72:  rtGetStringA         (a1,d0/a2/a3/a0)
 * Bias 78:  rtGetLongA           (a1/a2/a3/a0)
 * Bias 84:  Reserved
 * Bias 90:  Reserved
 * Bias 96:  rtFontRequestA       (a1/a3/a0)
 * Bias 102: rtPaletteRequestA    (a2/a3/a0)
 * Bias 108: rtReqHandlerA        (a1,d0/a0)
 * Bias 114: rtSetWaitPointer     (a0)
 * Bias 120: rtGetVScreenSize     (a0/a1/a2)
 * Bias 126: rtSetReqPosition     (d0/a0/a1/a2)
 * Bias 132: rtSpread             (a0/a1,d0/d1/d2/d3)
 * Bias 138: rtScreenToFrontSafely (a0)
 * Bias 144: rtScreenModeRequestA (a1/a3/a0)
 * Bias 150: rtCloseWindowSafely  (a0)
 * Bias 156: rtLockWindow         (a0)
 * Bias 162: rtUnlockWindow       (a0/a1)
 */
APTR __g_lxa_reqtools_FuncTab [] =
{
    __g_lxa_reqtools_OpenLib,            // -6   Standard
    __g_lxa_reqtools_CloseLib,           // -12  Standard
    __g_lxa_reqtools_ExpungeLib,         // -18  Standard
    __g_lxa_reqtools_ExtFuncLib,         // -24  Standard (reserved)
    _reqtools_rtAllocRequestA,           // -30  rtAllocRequestA
    _reqtools_rtFreeRequest,             // -36  rtFreeRequest
    _reqtools_rtFreeReqBuffer,           // -42  rtFreeReqBuffer
    _reqtools_rtChangeReqAttrA,          // -48  rtChangeReqAttrA
    _reqtools_rtFileRequestA,            // -54  rtFileRequestA
    _reqtools_rtFreeFileList,            // -60  rtFreeFileList
    _reqtools_rtEZRequestA,              // -66  rtEZRequestA
    _reqtools_rtGetStringA,              // -72  rtGetStringA
    _reqtools_rtGetLongA,                // -78  rtGetLongA
    _reqtools_Reserved,                  // -84  Reserved
    _reqtools_Reserved,                  // -90  Reserved
    _reqtools_rtFontRequestA,            // -96  rtFontRequestA
    _reqtools_rtPaletteRequestA,         // -102 rtPaletteRequestA
    _reqtools_rtReqHandlerA,             // -108 rtReqHandlerA
    _reqtools_rtSetWaitPointer,          // -114 rtSetWaitPointer
    _reqtools_rtGetVScreenSize,          // -120 rtGetVScreenSize
    _reqtools_rtSetReqPosition,          // -126 rtSetReqPosition
    _reqtools_rtSpread,                  // -132 rtSpread
    _reqtools_rtScreenToFrontSafely,     // -138 rtScreenToFrontSafely
    _reqtools_rtScreenModeRequestA,      // -144 rtScreenModeRequestA
    _reqtools_rtCloseWindowSafely,       // -150 rtCloseWindowSafely
    _reqtools_rtLockWindow,              // -156 rtLockWindow
    _reqtools_rtUnlockWindow,            // -162 rtUnlockWindow
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_reqtools_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_reqtools_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_reqtools_ExLibID[0],
    (ULONG) 0
};
