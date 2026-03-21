/*
 * lxa req.library implementation
 *
 * Provides the req.library API as a stub library.
 * req.library was a third-party file/font/string requester library by
 * Colin Fox / Bruce Dawson, used by many older Amiga applications
 * (KickPascal, older tools).  All functions return safe failure values
 * so that apps which open req.library but don't strictly require it
 * can continue to run.
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

#include "util.h"

#define VERSION    1
#define REVISION   3
#define EXLIBNAME  "req"
#define EXLIBVER   " 1.3 (2026/03/22)"

char __aligned _g_reqlib_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_reqlib_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_reqlib_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_reqlib_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/*
 * Library init/open/close/expunge
 */

struct Library * __g_lxa_reqlib_InitLib ( register struct Library   *libbase __asm("d0"),
                                         register BPTR              seglist __asm("a0"),
                                         register struct ExecBase  *sysb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_reqlib: InitLib() called\n");
    return libbase;
}

BPTR __g_lxa_reqlib_ExpungeLib ( register struct Library *libbase __asm("a6") )
{
    return 0;
}

struct Library * __g_lxa_reqlib_OpenLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_reqlib: OpenLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt++;
    libbase->lib_Flags &= ~LIBF_DELEXP;
    return libbase;
}

BPTR __g_lxa_reqlib_CloseLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_reqlib: CloseLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_reqlib_ExtFuncLib ( void )
{
    PRIVATE_FUNCTION_ERROR("_reqlib", "ExtFuncLib");
    return 0;
}

/*
 * Public API stubs
 *
 * req.library function biases (third-party, Colin Fox / Bruce Dawson):
 *
 * Bias -30:  FileRequester    (a0)
 * Bias -36:  ColorRequester   (d0)
 * Bias -42:  GetLong          (a0)
 * Bias -48:  GetString        (a0)
 * Bias -54:  TextRequest      (a0)
 * Bias -60:  SimpleRequest    (a0)
 * Bias -66:  TwoGadRequest    (a0)
 * Bias -72:  PurgeFiles       (a0)
 */

/* -30: FileRequester */
BOOL _reqlib_FileRequester ( register struct Library *ReqBase __asm("a6"),
                             register APTR freq __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqlib: FileRequester() freq=0x%08lx (stub, returns FALSE)\n", (ULONG)freq);
    return FALSE;
}

/* -36: ColorRequester */
LONG _reqlib_ColorRequester ( register struct Library *ReqBase __asm("a6"),
                              register LONG initialColor __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_reqlib: ColorRequester() initialColor=%ld (stub, returns -1)\n", initialColor);
    return -1;
}

/* -42: GetLong */
BOOL _reqlib_GetLong ( register struct Library *ReqBase __asm("a6"),
                       register APTR gls __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqlib: GetLong() gls=0x%08lx (stub, returns FALSE)\n", (ULONG)gls);
    return FALSE;
}

/* -48: GetString */
BOOL _reqlib_GetString ( register struct Library *ReqBase __asm("a6"),
                         register APTR gss __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqlib: GetString() gss=0x%08lx (stub, returns FALSE)\n", (ULONG)gss);
    return FALSE;
}

/* -54: TextRequest */
LONG _reqlib_TextRequest ( register struct Library *ReqBase __asm("a6"),
                           register APTR tr __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqlib: TextRequest() tr=0x%08lx (stub, returns 0)\n", (ULONG)tr);
    return 0;
}

/* -60: SimpleRequest */
void _reqlib_SimpleRequest ( register struct Library *ReqBase __asm("a6"),
                             register APTR tr __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqlib: SimpleRequest() tr=0x%08lx (stub)\n", (ULONG)tr);
}

/* -66: TwoGadRequest */
BOOL _reqlib_TwoGadRequest ( register struct Library *ReqBase __asm("a6"),
                             register APTR tr __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqlib: TwoGadRequest() tr=0x%08lx (stub, returns FALSE)\n", (ULONG)tr);
    return FALSE;
}

/* -72: PurgeFiles */
void _reqlib_PurgeFiles ( register struct Library *ReqBase __asm("a6"),
                          register APTR freq __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_reqlib: PurgeFiles() freq=0x%08lx (stub)\n", (ULONG)freq);
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

extern APTR              __g_lxa_reqlib_FuncTab [];
extern struct MyDataInit __g_lxa_reqlib_DataTab;
extern struct InitTable  __g_lxa_reqlib_InitTab;
extern APTR              __g_lxa_reqlib_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                        // UWORD rt_MatchWord
    &ROMTag,                              // struct Resident *rt_MatchTag
    &__g_lxa_reqlib_EndResident,          // APTR  rt_EndSkip
    RTF_AUTOINIT,                         // UBYTE rt_Flags
    VERSION,                              // UBYTE rt_Version
    NT_LIBRARY,                           // UBYTE rt_Type
    0,                                    // BYTE  rt_Pri
    &_g_reqlib_ExLibName[0],              // char  *rt_Name
    &_g_reqlib_ExLibID[0],                // char  *rt_IdString
    &__g_lxa_reqlib_InitTab               // APTR  rt_Init
};

APTR __g_lxa_reqlib_EndResident;
struct Resident *__lxa_reqlib_ROMTag = &ROMTag;

struct InitTable __g_lxa_reqlib_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_reqlib_FuncTab[0],
    (APTR)                &__g_lxa_reqlib_DataTab,
    (APTR)                __g_lxa_reqlib_InitLib
};

/*
 * Function table
 *
 * Standard library functions at -6 through -24
 * Then API functions starting at -30
 * Last entry at -72: PurgeFiles
 * Total: 4 standard + 8 API = 12 vectors
 */
APTR __g_lxa_reqlib_FuncTab [] =
{
    __g_lxa_reqlib_OpenLib,               // -6   Open
    __g_lxa_reqlib_CloseLib,              // -12  Close
    __g_lxa_reqlib_ExpungeLib,            // -18  Expunge
    __g_lxa_reqlib_ExtFuncLib,            // -24  Reserved
    /* API functions starting at -30 */
    _reqlib_FileRequester,                // -30  FileRequester
    _reqlib_ColorRequester,               // -36  ColorRequester
    _reqlib_GetLong,                      // -42  GetLong
    _reqlib_GetString,                    // -48  GetString
    _reqlib_TextRequest,                  // -54  TextRequest
    _reqlib_SimpleRequest,                // -60  SimpleRequest
    _reqlib_TwoGadRequest,                // -66  TwoGadRequest
    _reqlib_PurgeFiles,                   // -72  PurgeFiles
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_reqlib_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_reqlib_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_reqlib_ExLibID[0],
    (ULONG) 0
};
