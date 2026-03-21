/*
 * lxa amigaguide.library implementation
 *
 * Provides the AmigaGuide API as a stub library.
 * All functions return safe failure values so that applications
 * which open amigaguide.library but don't strictly require it
 * (e.g., Devpac, HiSoft BASIC) can continue to run.
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

#include <libraries/amigaguide.h>
#include <utility/tagitem.h>

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "amigaguide"
#define EXLIBVER   " 40.1 (2026/03/21)"

char __aligned _g_amigaguide_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_amigaguide_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_amigaguide_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_amigaguide_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/*
 * Library init/open/close/expunge
 */

struct Library * __g_lxa_amigaguide_InitLib ( register struct Library   *libbase __asm("d0"),
                                              register BPTR              seglist __asm("a0"),
                                              register struct ExecBase  *sysb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_amigaguide: InitLib() called\n");
    return libbase;
}

BPTR __g_lxa_amigaguide_ExpungeLib ( register struct Library *libbase __asm("a6") )
{
    return 0;
}

struct Library * __g_lxa_amigaguide_OpenLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: OpenLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt++;
    libbase->lib_Flags &= ~LIBF_DELEXP;
    return libbase;
}

BPTR __g_lxa_amigaguide_CloseLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: CloseLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_amigaguide_ExtFuncLib ( void )
{
    PRIVATE_FUNCTION_ERROR("_amigaguide", "ExtFuncLib");
    return 0;
}

/* Reserved / private function stub */
ULONG _amigaguide_Reserved ( void )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: Reserved/Private function called (stub)\n");
    return 0;
}

/*
 * Public API stubs
 *
 * FD offsets from amigaguide_lib.fd / amigaguide_lib.sfd (NDK 3.2):
 *
 * Bias -30:  AGARexxHost           (private)
 * Bias -36:  LockAmigaGuideBase    (a0)
 * Bias -42:  UnlockAmigaGuideBase  (d0)
 * Bias -48:  ExpungeDataBases      (private)
 * Bias -54:  OpenAmigaGuideA       (a0,a1)
 * Bias -60:  OpenAmigaGuideAsyncA  (a0,d0)
 * Bias -66:  CloseAmigaGuide       (a0)
 * Bias -72:  AmigaGuideSignal      (a0)
 * Bias -78:  GetAmigaGuideMsg      (a0)
 * Bias -84:  ReplyAmigaGuideMsg    (a0)
 * Bias -90:  SetAmigaGuideContextA (a0,d0,d1)
 * Bias -96:  SendAmigaGuideContextA(a0,d0)
 * Bias -102: SendAmigaGuideCmdA    (a0,d0,d1)
 * Bias -108: SetAmigaGuideAttrsA   (a0,a1)
 * Bias -114: GetAmigaGuideAttr     (d0,a0,a1)
 * Bias -120: SetAmigaGuideHook     (private)
 * Bias -126: LoadXRef              (a0,a1)
 * Bias -132: ExpungeXRef           ()
 * Bias -138: AddAmigaGuideHostA    (a0,d0,a1)
 * Bias -144: RemoveAmigaGuideHostA (a0,a1)
 * Bias -150: OpenE                 (private)
 * Bias -156: LockE                 (private)
 * Bias -162: CopyPathList          (private)
 * Bias -168: AddPathEntries        (private)
 * Bias -174: FreePathList          (private)
 * Bias -180: ParsePathString       (private)
 * Bias -186: OpenDataBase          (private)
 * Bias -192: LoadNode              (private)
 * Bias -198: UnloadNode            (private)
 * Bias -204: CloseDataBase         (private)
 * Bias -210: GetAmigaGuideString   (d0)
 */

/* -36: LockAmigaGuideBase */
LONG _amigaguide_LockAmigaGuideBase ( register struct Library *AmigaGuideBase __asm("a6"),
                                      register APTR handle __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: LockAmigaGuideBase() handle=0x%08lx (stub, returns 0)\n", (ULONG)handle);
    return 0;
}

/* -42: UnlockAmigaGuideBase */
void _amigaguide_UnlockAmigaGuideBase ( register struct Library *AmigaGuideBase __asm("a6"),
                                        register LONG key __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: UnlockAmigaGuideBase() key=%ld (stub)\n", key);
}

/* -54: OpenAmigaGuideA */
APTR _amigaguide_OpenAmigaGuideA ( register struct Library *AmigaGuideBase __asm("a6"),
                                   register struct NewAmigaGuide *nag __asm("a0"),
                                   register struct TagItem *tags __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: OpenAmigaGuideA() nag=0x%08lx (stub, returns NULL)\n", (ULONG)nag);
    return NULL;
}

/* -60: OpenAmigaGuideAsyncA */
APTR _amigaguide_OpenAmigaGuideAsyncA ( register struct Library *AmigaGuideBase __asm("a6"),
                                        register struct NewAmigaGuide *nag __asm("a0"),
                                        register ULONG attrs __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: OpenAmigaGuideAsyncA() nag=0x%08lx (stub, returns NULL)\n", (ULONG)nag);
    return NULL;
}

/* -66: CloseAmigaGuide */
void _amigaguide_CloseAmigaGuide ( register struct Library *AmigaGuideBase __asm("a6"),
                                   register APTR handle __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: CloseAmigaGuide() handle=0x%08lx (stub)\n", (ULONG)handle);
}

/* -72: AmigaGuideSignal */
ULONG _amigaguide_AmigaGuideSignal ( register struct Library *AmigaGuideBase __asm("a6"),
                                     register APTR handle __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: AmigaGuideSignal() handle=0x%08lx (stub, returns 0)\n", (ULONG)handle);
    return 0;
}

/* -78: GetAmigaGuideMsg */
struct AmigaGuideMsg * _amigaguide_GetAmigaGuideMsg ( register struct Library *AmigaGuideBase __asm("a6"),
                                                       register APTR handle __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: GetAmigaGuideMsg() handle=0x%08lx (stub, returns NULL)\n", (ULONG)handle);
    return NULL;
}

/* -84: ReplyAmigaGuideMsg */
void _amigaguide_ReplyAmigaGuideMsg ( register struct Library *AmigaGuideBase __asm("a6"),
                                      register struct AmigaGuideMsg *msg __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: ReplyAmigaGuideMsg() msg=0x%08lx (stub)\n", (ULONG)msg);
}

/* -90: SetAmigaGuideContextA */
LONG _amigaguide_SetAmigaGuideContextA ( register struct Library *AmigaGuideBase __asm("a6"),
                                         register APTR handle __asm("a0"),
                                         register ULONG context __asm("d0"),
                                         register ULONG attrs __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: SetAmigaGuideContextA() handle=0x%08lx context=%ld (stub, returns 0)\n",
             (ULONG)handle, context);
    return 0;
}

/* -96: SendAmigaGuideContextA */
LONG _amigaguide_SendAmigaGuideContextA ( register struct Library *AmigaGuideBase __asm("a6"),
                                          register APTR handle __asm("a0"),
                                          register ULONG attrs __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: SendAmigaGuideContextA() handle=0x%08lx (stub, returns 0)\n", (ULONG)handle);
    return 0;
}

/* -102: SendAmigaGuideCmdA */
LONG _amigaguide_SendAmigaGuideCmdA ( register struct Library *AmigaGuideBase __asm("a6"),
                                      register APTR handle __asm("a0"),
                                      register STRPTR cmd __asm("d0"),
                                      register ULONG attrs __asm("d1") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: SendAmigaGuideCmdA() handle=0x%08lx cmd=%s (stub, returns 0)\n",
             (ULONG)handle, STRORNULL((char *)(ULONG)cmd));
    return 0;
}

/* -108: SetAmigaGuideAttrsA */
LONG _amigaguide_SetAmigaGuideAttrsA ( register struct Library *AmigaGuideBase __asm("a6"),
                                       register APTR handle __asm("a0"),
                                       register struct TagItem *tags __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: SetAmigaGuideAttrsA() handle=0x%08lx (stub, returns 0)\n", (ULONG)handle);
    return 0;
}

/* -114: GetAmigaGuideAttr */
LONG _amigaguide_GetAmigaGuideAttr ( register struct Library *AmigaGuideBase __asm("a6"),
                                     register ULONG tag __asm("d0"),
                                     register APTR handle __asm("a0"),
                                     register ULONG *storage __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: GetAmigaGuideAttr() tag=0x%08lx handle=0x%08lx (stub, returns 0)\n",
             tag, (ULONG)handle);
    if (storage)
        *storage = 0;
    return 0;
}

/* -126: LoadXRef */
LONG _amigaguide_LoadXRef ( register struct Library *AmigaGuideBase __asm("a6"),
                            register BPTR lock __asm("a0"),
                            register STRPTR name __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: LoadXRef() name=%s (stub, returns 0)\n", STRORNULL(name));
    return 0;
}

/* -132: ExpungeXRef */
void _amigaguide_ExpungeXRef ( register struct Library *AmigaGuideBase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: ExpungeXRef() (stub)\n");
}

/* -138: AddAmigaGuideHostA */
APTR _amigaguide_AddAmigaGuideHostA ( register struct Library *AmigaGuideBase __asm("a6"),
                                      register struct Hook *hook __asm("a0"),
                                      register ULONG name __asm("d0"),
                                      register struct TagItem *tags __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: AddAmigaGuideHostA() hook=0x%08lx (stub, returns NULL)\n", (ULONG)hook);
    return NULL;
}

/* -144: RemoveAmigaGuideHostA */
LONG _amigaguide_RemoveAmigaGuideHostA ( register struct Library *AmigaGuideBase __asm("a6"),
                                         register APTR handle __asm("a0"),
                                         register struct TagItem *tags __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: RemoveAmigaGuideHostA() handle=0x%08lx (stub, returns 0)\n", (ULONG)handle);
    return 0;
}

/* -210: GetAmigaGuideString */
STRPTR _amigaguide_GetAmigaGuideString ( register struct Library *AmigaGuideBase __asm("a6"),
                                         register ULONG id __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_amigaguide: GetAmigaGuideString() id=%ld (stub, returns NULL)\n", id);
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

extern APTR              __g_lxa_amigaguide_FuncTab [];
extern struct MyDataInit __g_lxa_amigaguide_DataTab;
extern struct InitTable  __g_lxa_amigaguide_InitTab;
extern APTR              __g_lxa_amigaguide_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                        // UWORD rt_MatchWord
    &ROMTag,                              // struct Resident *rt_MatchTag
    &__g_lxa_amigaguide_EndResident,      // APTR  rt_EndSkip
    RTF_AUTOINIT,                         // UBYTE rt_Flags
    VERSION,                              // UBYTE rt_Version
    NT_LIBRARY,                           // UBYTE rt_Type
    0,                                    // BYTE  rt_Pri
    &_g_amigaguide_ExLibName[0],          // char  *rt_Name
    &_g_amigaguide_ExLibID[0],            // char  *rt_IdString
    &__g_lxa_amigaguide_InitTab           // APTR  rt_Init
};

APTR __g_lxa_amigaguide_EndResident;
struct Resident *__lxa_amigaguide_ROMTag = &ROMTag;

struct InitTable __g_lxa_amigaguide_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_amigaguide_FuncTab[0],
    (APTR)                &__g_lxa_amigaguide_DataTab,
    (APTR)                __g_lxa_amigaguide_InitLib
};

/*
 * Function table
 *
 * Standard library functions at -6 through -24
 * Then API functions starting at -30
 * Last entry at -210: GetAmigaGuideString
 * Total: 4 standard + 31 API = 35 vectors
 */
APTR __g_lxa_amigaguide_FuncTab [] =
{
    __g_lxa_amigaguide_OpenLib,              // -6   Open
    __g_lxa_amigaguide_CloseLib,             // -12  Close
    __g_lxa_amigaguide_ExpungeLib,           // -18  Expunge
    __g_lxa_amigaguide_ExtFuncLib,           // -24  Reserved
    /* API functions starting at -30 */
    _amigaguide_Reserved,                    // -30  AGARexxHost (private)
    _amigaguide_LockAmigaGuideBase,          // -36  LockAmigaGuideBase
    _amigaguide_UnlockAmigaGuideBase,        // -42  UnlockAmigaGuideBase
    _amigaguide_Reserved,                    // -48  ExpungeDataBases (private)
    _amigaguide_OpenAmigaGuideA,             // -54  OpenAmigaGuideA
    _amigaguide_OpenAmigaGuideAsyncA,        // -60  OpenAmigaGuideAsyncA
    _amigaguide_CloseAmigaGuide,             // -66  CloseAmigaGuide
    _amigaguide_AmigaGuideSignal,            // -72  AmigaGuideSignal
    _amigaguide_GetAmigaGuideMsg,            // -78  GetAmigaGuideMsg
    _amigaguide_ReplyAmigaGuideMsg,          // -84  ReplyAmigaGuideMsg
    _amigaguide_SetAmigaGuideContextA,       // -90  SetAmigaGuideContextA
    _amigaguide_SendAmigaGuideContextA,      // -96  SendAmigaGuideContextA
    _amigaguide_SendAmigaGuideCmdA,          // -102 SendAmigaGuideCmdA
    _amigaguide_SetAmigaGuideAttrsA,         // -108 SetAmigaGuideAttrsA
    _amigaguide_GetAmigaGuideAttr,           // -114 GetAmigaGuideAttr
    _amigaguide_Reserved,                    // -120 SetAmigaGuideHook (private)
    _amigaguide_LoadXRef,                    // -126 LoadXRef
    _amigaguide_ExpungeXRef,                 // -132 ExpungeXRef
    _amigaguide_AddAmigaGuideHostA,          // -138 AddAmigaGuideHostA
    _amigaguide_RemoveAmigaGuideHostA,       // -144 RemoveAmigaGuideHostA
    _amigaguide_Reserved,                    // -150 OpenE (private)
    _amigaguide_Reserved,                    // -156 LockE (private)
    _amigaguide_Reserved,                    // -162 CopyPathList (private)
    _amigaguide_Reserved,                    // -168 AddPathEntries (private)
    _amigaguide_Reserved,                    // -174 FreePathList (private)
    _amigaguide_Reserved,                    // -180 ParsePathString (private)
    _amigaguide_Reserved,                    // -186 OpenDataBase (private)
    _amigaguide_Reserved,                    // -192 LoadNode (private)
    _amigaguide_Reserved,                    // -198 UnloadNode (private)
    _amigaguide_Reserved,                    // -204 CloseDataBase (private)
    _amigaguide_GetAmigaGuideString,         // -210 GetAmigaGuideString
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_amigaguide_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_amigaguide_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_amigaguide_ExLibID[0],
    (ULONG) 0
};
