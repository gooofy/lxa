/*
 * lxa powerpacker.library implementation
 *
 * Provides the PowerPacker API as a stub library.
 * powerpacker.library was a third-party decompression library by
 * Nico Francois.  Very common — many apps ship PowerPacked executables
 * or data files.  All functions return safe failure values so that
 * applications which open powerpacker.library can fall back to
 * uncompressed loading.
 *
 * ppLoadData returns PP_OPENERR (1) to indicate file open failure,
 * which is the expected error path when PowerPacker is not available.
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

/* PowerPacker error codes */
#define PP_OPENERR   1
#define PP_READERR   2
#define PP_NOMEMORY  3
#define PP_CRYPTED   4
#define PP_PASSERR   5

#define VERSION    36
#define REVISION   10
#define EXLIBNAME  "powerpacker"
#define EXLIBVER   " 36.10 (2026/03/22)"

char __aligned _g_powerpacker_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_powerpacker_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_powerpacker_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_powerpacker_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/*
 * Library init/open/close/expunge
 */

struct Library * __g_lxa_powerpacker_InitLib ( register struct Library   *libbase __asm("d0"),
                                              register BPTR              seglist __asm("a0"),
                                              register struct ExecBase  *sysb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_powerpacker: InitLib() called\n");
    return libbase;
}

BPTR __g_lxa_powerpacker_ExpungeLib ( register struct Library *libbase __asm("a6") )
{
    return 0;
}

struct Library * __g_lxa_powerpacker_OpenLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: OpenLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt++;
    libbase->lib_Flags &= ~LIBF_DELEXP;
    return libbase;
}

BPTR __g_lxa_powerpacker_CloseLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: CloseLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_powerpacker_ExtFuncLib ( void )
{
    PRIVATE_FUNCTION_ERROR("_powerpacker", "ExtFuncLib");
    return 0;
}

/*
 * Public API stubs
 *
 * powerpacker.library function biases (Nico Francois, PPBase):
 *
 * Bias -30:  ppLoadData          (a0,d0,d1,a1,a2,a3)
 * Bias -36:  ppDecrunchBuffer    (a0,a1,d0)
 * Bias -42:  ppCalcChecksum      (a0)
 * Bias -48:  ppCalcPasskey       (a0)
 * Bias -54:  ppDecrypt           (a0,d0,d1)
 * Bias -60:  ppAllocCrunchInfo   (d0,d1,a0,d2)
 * Bias -66:  ppFreeCrunchInfo    (a0)
 * Bias -72:  ppCrunchBuffer      (a0,a1,d0)
 * Bias -78:  ppWriteDataHeader   (a0,d0,d1,d2)
 * Bias -84:  ppEnterPassword     (a0,d0)
 * Bias -90:  ppErrorMessage      (d0)
 */

/* -30: ppLoadData */
ULONG _powerpacker_ppLoadData ( register struct Library *PPBase __asm("a6"),
                                register STRPTR fileName __asm("a0"),
                                register ULONG colorType __asm("d0"),
                                register ULONG memType __asm("d1"),
                                register UBYTE **buffer __asm("a1"),
                                register ULONG *length __asm("a2"),
                                register APTR callback __asm("a3") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: ppLoadData() fileName=%s (stub, returns PP_OPENERR)\n",
             STRORNULL(fileName));
    if (buffer)
        *buffer = NULL;
    if (length)
        *length = 0;
    return PP_OPENERR;
}

/* -36: ppDecrunchBuffer */
void _powerpacker_ppDecrunchBuffer ( register struct Library *PPBase __asm("a6"),
                                     register UBYTE *endCrunched __asm("a0"),
                                     register UBYTE *endBuffer __asm("a1"),
                                     register ULONG efficiency __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: ppDecrunchBuffer() (stub, no-op)\n");
}

/* -42: ppCalcChecksum */
UWORD _powerpacker_ppCalcChecksum ( register struct Library *PPBase __asm("a6"),
                                    register UBYTE *buffer __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: ppCalcChecksum() buffer=0x%08lx (stub, returns 0)\n", (ULONG)buffer);
    return 0;
}

/* -48: ppCalcPasskey */
ULONG _powerpacker_ppCalcPasskey ( register struct Library *PPBase __asm("a6"),
                                   register STRPTR password __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: ppCalcPasskey() password=%s (stub, returns 0)\n",
             STRORNULL(password));
    return 0;
}

/* -54: ppDecrypt */
void _powerpacker_ppDecrypt ( register struct Library *PPBase __asm("a6"),
                              register UBYTE *buffer __asm("a0"),
                              register ULONG length __asm("d0"),
                              register ULONG passkey __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: ppDecrypt() buffer=0x%08lx length=%ld (stub, no-op)\n",
             (ULONG)buffer, length);
}

/* -60: ppAllocCrunchInfo */
APTR _powerpacker_ppAllocCrunchInfo ( register struct Library *PPBase __asm("a6"),
                                      register ULONG efficiency __asm("d0"),
                                      register ULONG speedup __asm("d1"),
                                      register APTR callback __asm("a0"),
                                      register ULONG memType __asm("d2") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: ppAllocCrunchInfo() efficiency=%ld (stub, returns NULL)\n", efficiency);
    return NULL;
}

/* -66: ppFreeCrunchInfo */
void _powerpacker_ppFreeCrunchInfo ( register struct Library *PPBase __asm("a6"),
                                     register APTR crunchInfo __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: ppFreeCrunchInfo() crunchInfo=0x%08lx (stub)\n", (ULONG)crunchInfo);
}

/* -72: ppCrunchBuffer */
ULONG _powerpacker_ppCrunchBuffer ( register struct Library *PPBase __asm("a6"),
                                    register APTR crunchInfo __asm("a0"),
                                    register UBYTE *buffer __asm("a1"),
                                    register ULONG length __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: ppCrunchBuffer() length=%ld (stub, returns 0)\n", length);
    return 0;
}

/* -78: ppWriteDataHeader */
LONG _powerpacker_ppWriteDataHeader ( register struct Library *PPBase __asm("a6"),
                                      register APTR crunchInfo __asm("a0"),
                                      register BPTR fh __asm("d0"),
                                      register ULONG efficiency __asm("d1"),
                                      register ULONG crypt __asm("d2") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: ppWriteDataHeader() (stub, returns -1)\n");
    return -1;
}

/* -84: ppEnterPassword */
LONG _powerpacker_ppEnterPassword ( register struct Library *PPBase __asm("a6"),
                                    register APTR screen __asm("a0"),
                                    register UBYTE *passwordBuffer __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: ppEnterPassword() (stub, returns -1)\n");
    return -1;
}

/* -90: ppErrorMessage */
STRPTR _powerpacker_ppErrorMessage ( register struct Library *PPBase __asm("a6"),
                                     register ULONG errorCode __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_powerpacker: ppErrorMessage() errorCode=%ld (stub, returns NULL)\n", errorCode);
    return NULL;
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

extern APTR              __g_lxa_powerpacker_FuncTab [];
extern struct MyDataInit __g_lxa_powerpacker_DataTab;
extern struct InitTable  __g_lxa_powerpacker_InitTab;
extern APTR              __g_lxa_powerpacker_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                           // UWORD rt_MatchWord
    &ROMTag,                                 // struct Resident *rt_MatchTag
    &__g_lxa_powerpacker_EndResident,        // APTR  rt_EndSkip
    RTF_AUTOINIT,                            // UBYTE rt_Flags
    VERSION,                                 // UBYTE rt_Version
    NT_LIBRARY,                              // UBYTE rt_Type
    0,                                       // BYTE  rt_Pri
    &_g_powerpacker_ExLibName[0],            // char  *rt_Name
    &_g_powerpacker_ExLibID[0],              // char  *rt_IdString
    &__g_lxa_powerpacker_InitTab             // APTR  rt_Init
};

APTR __g_lxa_powerpacker_EndResident;
struct Resident *__lxa_powerpacker_ROMTag = &ROMTag;

struct InitTable __g_lxa_powerpacker_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_powerpacker_FuncTab[0],
    (APTR)                &__g_lxa_powerpacker_DataTab,
    (APTR)                __g_lxa_powerpacker_InitLib
};

/*
 * Function table
 *
 * Standard library functions at -6 through -24
 * Then API functions starting at -30
 * Last entry at -90: ppErrorMessage
 * Total: 4 standard + 11 API = 15 vectors
 */
APTR __g_lxa_powerpacker_FuncTab [] =
{
    __g_lxa_powerpacker_OpenLib,             // -6   Open
    __g_lxa_powerpacker_CloseLib,            // -12  Close
    __g_lxa_powerpacker_ExpungeLib,          // -18  Expunge
    __g_lxa_powerpacker_ExtFuncLib,          // -24  Reserved
    /* API functions starting at -30 */
    _powerpacker_ppLoadData,                 // -30  ppLoadData
    _powerpacker_ppDecrunchBuffer,           // -36  ppDecrunchBuffer
    _powerpacker_ppCalcChecksum,             // -42  ppCalcChecksum
    _powerpacker_ppCalcPasskey,              // -48  ppCalcPasskey
    _powerpacker_ppDecrypt,                  // -54  ppDecrypt
    _powerpacker_ppAllocCrunchInfo,          // -60  ppAllocCrunchInfo
    _powerpacker_ppFreeCrunchInfo,           // -66  ppFreeCrunchInfo
    _powerpacker_ppCrunchBuffer,             // -72  ppCrunchBuffer
    _powerpacker_ppWriteDataHeader,          // -78  ppWriteDataHeader
    _powerpacker_ppEnterPassword,            // -84  ppEnterPassword
    _powerpacker_ppErrorMessage,             // -90  ppErrorMessage
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_powerpacker_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_powerpacker_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_powerpacker_ExLibID[0],
    (ULONG) 0
};
