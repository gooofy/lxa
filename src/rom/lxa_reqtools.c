/*
 * lxa reqtools.library implementation
 *
 * Provides the ReqTools API as a stub library.
 * reqtools.library was a third-party enhanced requester library by
 * Nico Francois, widely used by late-era Amiga applications.
 * All functions return safe failure values so that applications
 * which open reqtools.library but don't strictly require it
 * can continue to run.
 *
 * Function biases and register conventions from VBCC inline headers.
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

#include <intuition/intuition.h>
#include <utility/tagitem.h>

#include "util.h"

#define VERSION    38
#define REVISION   1
#define EXLIBNAME  "reqtools"
#define EXLIBVER   " 38.1 (2026/03/22)"

char __aligned _g_reqtools_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_reqtools_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_reqtools_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_reqtools_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/*
 * Library init/open/close/expunge
 */

struct Library * __g_lxa_reqtools_InitLib ( register struct Library   *libbase __asm("d0"),
                                           register BPTR              seglist __asm("a0"),
                                           register struct ExecBase  *sysb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_reqtools: InitLib() called\n");
    return libbase;
}

BPTR __g_lxa_reqtools_ExpungeLib ( register struct Library *libbase __asm("a6") )
{
    return 0;
}

struct Library * __g_lxa_reqtools_OpenLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: OpenLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt++;
    libbase->lib_Flags &= ~LIBF_DELEXP;
    return libbase;
}

BPTR __g_lxa_reqtools_CloseLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: CloseLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_reqtools_ExtFuncLib ( void )
{
    PRIVATE_FUNCTION_ERROR("_reqtools", "ExtFuncLib");
    return 0;
}

/* Reserved / private function stub */
ULONG _reqtools_Reserved ( void )
{
    DPRINTF (LOG_DEBUG, "_reqtools: Reserved/Private function called (stub)\n");
    return 0;
}

/*
 * Public API stubs
 *
 * reqtools.library function biases (from VBCC inline reqtools_protos.h):
 *
 * Bias -30:  rtAllocRequestA     (d0,a0)
 * Bias -36:  rtFreeRequest       (a1)
 * Bias -42:  rtFreeReqBuffer     (a1)
 * Bias -48:  rtChangeReqAttrA    (a1,a0)
 * Bias -54:  rtFileRequestA      (a1,a2,a3,a0)
 * Bias -60:  rtFreeFileList      (a0)
 * Bias -66:  rtEZRequestA        (a1,a2,a3,a4,a0)
 * Bias -72:  rtGetStringA        (a1,d0,a2,a3,a0)
 * Bias -78:  rtGetLongA          (a1,a2,a3,a0)
 * Bias -84:  (private)
 * Bias -90:  (private)
 * Bias -96:  rtFontRequestA      (a1,a3,a0)
 * Bias -102: rtPaletteRequestA   (a2,a3,a0)
 * Bias -108: rtReqHandlerA       (a1,d0,a0)
 * Bias -114: rtSetWaitPointer    (a0)
 * Bias -120: rtGetVScreenSize    (a0,a1,a2)
 * Bias -126: rtSetReqPosition    (d0,a0,a1,a2)
 * Bias -132: rtSpread            (a0,a1,d0,d1,d2,d3)
 * Bias -138: rtScreenToFrontSafely (a0)
 * Bias -144: rtScreenModeRequestA  (a1,a3,a0)
 * Bias -150: rtCloseWindowSafely   (a0)
 * Bias -156: rtLockWindow          (a0)
 * Bias -162: rtUnlockWindow        (a0,a1)
 */

/* -30: rtAllocRequestA */
APTR _reqtools_rtAllocRequestA ( register struct Library *ReqToolsBase __asm("a6"),
                                 register ULONG type __asm("d0"),
                                 register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtAllocRequestA() type=%ld (stub, returns NULL)\n", type);
    return NULL;
}

/* -36: rtFreeRequest */
void _reqtools_rtFreeRequest ( register struct Library *ReqToolsBase __asm("a6"),
                               register APTR req __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtFreeRequest() req=0x%08lx (stub)\n", (ULONG)req);
}

/* -42: rtFreeReqBuffer */
void _reqtools_rtFreeReqBuffer ( register struct Library *ReqToolsBase __asm("a6"),
                                 register APTR req __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtFreeReqBuffer() req=0x%08lx (stub)\n", (ULONG)req);
}

/* -48: rtChangeReqAttrA */
LONG _reqtools_rtChangeReqAttrA ( register struct Library *ReqToolsBase __asm("a6"),
                                  register APTR req __asm("a1"),
                                  register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtChangeReqAttrA() req=0x%08lx (stub, returns 0)\n", (ULONG)req);
    return 0;
}

/* -54: rtFileRequestA */
APTR _reqtools_rtFileRequestA ( register struct Library *ReqToolsBase __asm("a6"),
                                register APTR filereq __asm("a1"),
                                register STRPTR file __asm("a2"),
                                register STRPTR title __asm("a3"),
                                register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtFileRequestA() filereq=0x%08lx title=%s (stub, returns NULL)\n",
             (ULONG)filereq, STRORNULL(title));
    return NULL;
}

/* -60: rtFreeFileList */
void _reqtools_rtFreeFileList ( register struct Library *ReqToolsBase __asm("a6"),
                                register APTR filelist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtFreeFileList() filelist=0x%08lx (stub)\n", (ULONG)filelist);
}

/* -66: rtEZRequestA */
ULONG _reqtools_rtEZRequestA ( register struct Library *ReqToolsBase __asm("a6"),
                               register STRPTR bodyfmt __asm("a1"),
                               register STRPTR gadfmt __asm("a2"),
                               register APTR reqinfo __asm("a3"),
                               register APTR argarray __asm("a4"),
                               register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtEZRequestA() body=%s (stub, returns 0)\n",
             STRORNULL(bodyfmt));
    return 0;
}

/* -72: rtGetStringA */
ULONG _reqtools_rtGetStringA ( register struct Library *ReqToolsBase __asm("a6"),
                               register UBYTE *buffer __asm("a1"),
                               register ULONG maxchars __asm("d0"),
                               register STRPTR title __asm("a2"),
                               register APTR reqinfo __asm("a3"),
                               register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtGetStringA() title=%s maxchars=%ld (stub, returns 0)\n",
             STRORNULL(title), maxchars);
    return 0;
}

/* -78: rtGetLongA */
ULONG _reqtools_rtGetLongA ( register struct Library *ReqToolsBase __asm("a6"),
                             register ULONG *longptr __asm("a1"),
                             register STRPTR title __asm("a2"),
                             register APTR reqinfo __asm("a3"),
                             register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtGetLongA() title=%s (stub, returns 0)\n",
             STRORNULL(title));
    return 0;
}

/* -96: rtFontRequestA */
ULONG _reqtools_rtFontRequestA ( register struct Library *ReqToolsBase __asm("a6"),
                                 register APTR fontreq __asm("a1"),
                                 register STRPTR title __asm("a3"),
                                 register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtFontRequestA() fontreq=0x%08lx title=%s (stub, returns 0)\n",
             (ULONG)fontreq, STRORNULL(title));
    return 0;
}

/* -102: rtPaletteRequestA */
LONG _reqtools_rtPaletteRequestA ( register struct Library *ReqToolsBase __asm("a6"),
                                   register STRPTR title __asm("a2"),
                                   register APTR reqinfo __asm("a3"),
                                   register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtPaletteRequestA() title=%s (stub, returns -1)\n",
             STRORNULL(title));
    return -1;
}

/* -108: rtReqHandlerA */
ULONG _reqtools_rtReqHandlerA ( register struct Library *ReqToolsBase __asm("a6"),
                                register APTR handlerinfo __asm("a1"),
                                register ULONG sigs __asm("d0"),
                                register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtReqHandlerA() handlerinfo=0x%08lx (stub, returns 0)\n",
             (ULONG)handlerinfo);
    return 0;
}

/* -114: rtSetWaitPointer */
void _reqtools_rtSetWaitPointer ( register struct Library *ReqToolsBase __asm("a6"),
                                  register struct Window *window __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtSetWaitPointer() window=0x%08lx (stub)\n", (ULONG)window);
}

/* -120: rtGetVScreenSize */
ULONG _reqtools_rtGetVScreenSize ( register struct Library *ReqToolsBase __asm("a6"),
                                   register APTR screen __asm("a0"),
                                   register ULONG *widthptr __asm("a1"),
                                   register ULONG *heightptr __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtGetVScreenSize() screen=0x%08lx (stub, returns 0)\n", (ULONG)screen);
    if (widthptr)
        *widthptr = 640;
    if (heightptr)
        *heightptr = 256;
    return 0;
}

/* -126: rtSetReqPosition */
void _reqtools_rtSetReqPosition ( register struct Library *ReqToolsBase __asm("a6"),
                                  register ULONG reqpos __asm("d0"),
                                  register APTR newwindow __asm("a0"),
                                  register APTR screen __asm("a1"),
                                  register struct Window *window __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtSetReqPosition() reqpos=%ld (stub)\n", reqpos);
}

/* -132: rtSpread */
void _reqtools_rtSpread ( register struct Library *ReqToolsBase __asm("a6"),
                          register ULONG *posarray __asm("a0"),
                          register ULONG *sizearray __asm("a1"),
                          register ULONG length __asm("d0"),
                          register ULONG min __asm("d1"),
                          register ULONG max __asm("d2"),
                          register ULONG num __asm("d3") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtSpread() length=%ld (stub)\n", length);
}

/* -138: rtScreenToFrontSafely */
void _reqtools_rtScreenToFrontSafely ( register struct Library *ReqToolsBase __asm("a6"),
                                       register APTR screen __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtScreenToFrontSafely() screen=0x%08lx (stub)\n", (ULONG)screen);
}

/* -144: rtScreenModeRequestA */
ULONG _reqtools_rtScreenModeRequestA ( register struct Library *ReqToolsBase __asm("a6"),
                                       register APTR screenmodereq __asm("a1"),
                                       register STRPTR title __asm("a3"),
                                       register struct TagItem *taglist __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtScreenModeRequestA() title=%s (stub, returns 0)\n",
             STRORNULL(title));
    return 0;
}

/* -150: rtCloseWindowSafely */
void _reqtools_rtCloseWindowSafely ( register struct Library *ReqToolsBase __asm("a6"),
                                     register struct Window *win __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtCloseWindowSafely() win=0x%08lx (stub)\n", (ULONG)win);
}

/* -156: rtLockWindow */
APTR _reqtools_rtLockWindow ( register struct Library *ReqToolsBase __asm("a6"),
                              register struct Window *win __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqtools: rtLockWindow() win=0x%08lx (stub, returns NULL)\n", (ULONG)win);
    return NULL;
}

/* -162: rtUnlockWindow */
void _reqtools_rtUnlockWindow ( register struct Library *ReqToolsBase __asm("a6"),
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
    RTC_MATCHWORD,                        // UWORD rt_MatchWord
    &ROMTag,                              // struct Resident *rt_MatchTag
    &__g_lxa_reqtools_EndResident,        // APTR  rt_EndSkip
    RTF_AUTOINIT,                         // UBYTE rt_Flags
    VERSION,                              // UBYTE rt_Version
    NT_LIBRARY,                           // UBYTE rt_Type
    0,                                    // BYTE  rt_Pri
    &_g_reqtools_ExLibName[0],            // char  *rt_Name
    &_g_reqtools_ExLibID[0],              // char  *rt_IdString
    &__g_lxa_reqtools_InitTab             // APTR  rt_Init
};

APTR __g_lxa_reqtools_EndResident;
struct Resident *__lxa_reqtools_ROMTag = &ROMTag;

struct InitTable __g_lxa_reqtools_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_reqtools_FuncTab[0],
    (APTR)                &__g_lxa_reqtools_DataTab,
    (APTR)                __g_lxa_reqtools_InitLib
};

/*
 * Function table
 *
 * Standard library functions at -6 through -24
 * Then API functions starting at -30
 * Last entry at -162: rtUnlockWindow
 * Total: 4 standard + 23 API (including 2 private) = 27 vectors
 */
APTR __g_lxa_reqtools_FuncTab [] =
{
    __g_lxa_reqtools_OpenLib,             // -6   Open
    __g_lxa_reqtools_CloseLib,            // -12  Close
    __g_lxa_reqtools_ExpungeLib,          // -18  Expunge
    __g_lxa_reqtools_ExtFuncLib,          // -24  Reserved
    /* API functions starting at -30 */
    _reqtools_rtAllocRequestA,            // -30  rtAllocRequestA
    _reqtools_rtFreeRequest,              // -36  rtFreeRequest
    _reqtools_rtFreeReqBuffer,            // -42  rtFreeReqBuffer
    _reqtools_rtChangeReqAttrA,           // -48  rtChangeReqAttrA
    _reqtools_rtFileRequestA,             // -54  rtFileRequestA
    _reqtools_rtFreeFileList,             // -60  rtFreeFileList
    _reqtools_rtEZRequestA,               // -66  rtEZRequestA
    _reqtools_rtGetStringA,               // -72  rtGetStringA
    _reqtools_rtGetLongA,                 // -78  rtGetLongA
    _reqtools_Reserved,                   // -84  (private)
    _reqtools_Reserved,                   // -90  (private)
    _reqtools_rtFontRequestA,             // -96  rtFontRequestA
    _reqtools_rtPaletteRequestA,          // -102 rtPaletteRequestA
    _reqtools_rtReqHandlerA,              // -108 rtReqHandlerA
    _reqtools_rtSetWaitPointer,           // -114 rtSetWaitPointer
    _reqtools_rtGetVScreenSize,           // -120 rtGetVScreenSize
    _reqtools_rtSetReqPosition,           // -126 rtSetReqPosition
    _reqtools_rtSpread,                   // -132 rtSpread
    _reqtools_rtScreenToFrontSafely,      // -138 rtScreenToFrontSafely
    _reqtools_rtScreenModeRequestA,       // -144 rtScreenModeRequestA
    _reqtools_rtCloseWindowSafely,        // -150 rtCloseWindowSafely
    _reqtools_rtLockWindow,               // -156 rtLockWindow
    _reqtools_rtUnlockWindow,             // -162 rtUnlockWindow
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
