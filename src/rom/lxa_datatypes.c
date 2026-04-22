/*
 * lxa datatypes.library stub
 *
 * Provides the datatypes.library API as a disk library stub.
 *
 * NOTE: datatypes.library is an AmigaOS 3.x **system** library.
 * Per AGENTS.md §1, system libraries must be fully implemented, not
 * stubbed. This file is intentionally a TEMPORARY MINIMAL STUB to
 * unblock Phase 136 (Typeface) startup, which calls OpenLibrary on
 * datatypes.library at init and crashes on the resulting NULL base.
 *
 * **Tech debt**: Phase 142 (see roadmap.md "Active Development") is
 * scheduled to replace this stub with a full RKRM-correct
 * implementation. Do not extend this file with additional stub logic
 * — invest the time in Phase 142 instead.
 *
 * All public functions return safe failure values so that apps which
 * open datatypes.library but don't strictly require its functionality
 * (e.g. Typeface, which opens it during startup but tolerates NULL
 * results) can continue to run without crashing.
 *
 * Function biases follow the official AmigaOS NDK datatypes_protos.h.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <utility/tagitem.h>

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "datatypes"
#define EXLIBVER   " 40.1 (2026/04/22)"

char __aligned _g_datatypes_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_datatypes_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_datatypes_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_datatypes_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/*
 * Library init/open/close/expunge
 */

struct Library * __g_lxa_datatypes_InitLib ( register struct Library   *libbase __asm("d0"),
                                             register BPTR              seglist __asm("a0"),
                                             register struct ExecBase  *sysb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_datatypes: InitLib() called\n");
    return libbase;
}

BPTR __g_lxa_datatypes_ExpungeLib ( register struct Library *libbase __asm("a6") )
{
    return 0;
}

struct Library * __g_lxa_datatypes_OpenLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: OpenLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt++;
    libbase->lib_Flags &= ~LIBF_DELEXP;
    return libbase;
}

BPTR __g_lxa_datatypes_CloseLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: CloseLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_datatypes_ExtFuncLib ( void )
{
    PRIVATE_FUNCTION_ERROR("_datatypes", "ExtFuncLib");
    return 0;
}

/* Reserved / private function stub */
ULONG _datatypes_Reserved ( void )
{
    DPRINTF (LOG_DEBUG, "_datatypes: Reserved/Private function called (stub)\n");
    return 0;
}

/*
 * Public API stubs
 *
 * Function biases per NDK datatypes_protos.h / datatypes_lib.fd:
 *
 * Bias -30:  (private)
 * Bias -36:  ObtainDataTypeA       (d0,a0,a1)
 * Bias -42:  ReleaseDataType       (a0)
 * Bias -48:  NewDTObjectA          (a0,a1)
 * Bias -54:  DisposeDTObject       (a0)
 * Bias -60:  SetDTAttrsA           (a0,a1,a2)
 * Bias -66:  GetDTAttrsA           (a0,a1)
 * Bias -72:  AddDTObject           (a0,a1,a2,d0)
 * Bias -78:  RefreshDTObjectA      (a0,a1,a2,a3)
 * Bias -84:  DoAsyncLayout         (a0,a1)
 * Bias -90:  DoDTMethodA           (a0,a1,a2,a3)
 * Bias -96:  RemoveDTObject        (a0,a1)
 * Bias -102: GetDTMethods          (a0)
 * Bias -108: GetDTTriggerMethods   (a0)
 * Bias -114: PrintDTObjectA        (a0,a1,a2,a3)
 * Bias -120: (private)
 * Bias -126: (private)
 * Bias -132: (private)
 * Bias -138: GetDTString           (d0)
 */

/* -36: ObtainDataTypeA */
APTR _datatypes_ObtainDataTypeA ( register struct Library *DataTypesBase __asm("a6"),
                                  register ULONG type __asm("d0"),
                                  register APTR source __asm("a0"),
                                  register struct TagItem *tags __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: ObtainDataTypeA() type=%ld source=0x%08lx (stub, returns NULL)\n",
             type, (ULONG)source);
    return NULL;
}

/* -42: ReleaseDataType */
void _datatypes_ReleaseDataType ( register struct Library *DataTypesBase __asm("a6"),
                                  register APTR dt __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: ReleaseDataType() dt=0x%08lx (stub)\n", (ULONG)dt);
}

/* -48: NewDTObjectA */
APTR _datatypes_NewDTObjectA ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR name __asm("a0"),
                               register struct TagItem *tags __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: NewDTObjectA() name=0x%08lx (stub, returns NULL)\n", (ULONG)name);
    return NULL;
}

/* -54: DisposeDTObject */
void _datatypes_DisposeDTObject ( register struct Library *DataTypesBase __asm("a6"),
                                  register APTR object __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: DisposeDTObject() object=0x%08lx (stub)\n", (ULONG)object);
}

/* -60: SetDTAttrsA */
ULONG _datatypes_SetDTAttrsA ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR object __asm("a0"),
                               register APTR window __asm("a1"),
                               register struct TagItem *tags __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: SetDTAttrsA() object=0x%08lx (stub, returns 0)\n", (ULONG)object);
    return 0;
}

/* -66: GetDTAttrsA */
ULONG _datatypes_GetDTAttrsA ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR object __asm("a0"),
                               register struct TagItem *tags __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: GetDTAttrsA() object=0x%08lx (stub, returns 0)\n", (ULONG)object);
    return 0;
}

/* -72: AddDTObject */
ULONG _datatypes_AddDTObject ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR window __asm("a0"),
                               register APTR requester __asm("a1"),
                               register APTR object __asm("a2"),
                               register LONG position __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: AddDTObject() object=0x%08lx (stub, returns 0)\n", (ULONG)object);
    return 0;
}

/* -78: RefreshDTObjectA */
void _datatypes_RefreshDTObjectA ( register struct Library *DataTypesBase __asm("a6"),
                                   register APTR object __asm("a0"),
                                   register APTR window __asm("a1"),
                                   register APTR requester __asm("a2"),
                                   register struct TagItem *tags __asm("a3") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: RefreshDTObjectA() object=0x%08lx (stub)\n", (ULONG)object);
}

/* -84: DoAsyncLayout */
ULONG _datatypes_DoAsyncLayout ( register struct Library *DataTypesBase __asm("a6"),
                                 register APTR object __asm("a0"),
                                 register APTR gpl __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: DoAsyncLayout() object=0x%08lx (stub, returns 0)\n", (ULONG)object);
    return 0;
}

/* -90: DoDTMethodA */
ULONG _datatypes_DoDTMethodA ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR object __asm("a0"),
                               register APTR window __asm("a1"),
                               register APTR requester __asm("a2"),
                               register APTR msg __asm("a3") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: DoDTMethodA() object=0x%08lx msg=0x%08lx (stub, returns 0)\n",
             (ULONG)object, (ULONG)msg);
    return 0;
}

/* -96: RemoveDTObject */
ULONG _datatypes_RemoveDTObject ( register struct Library *DataTypesBase __asm("a6"),
                                  register APTR window __asm("a0"),
                                  register APTR object __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: RemoveDTObject() object=0x%08lx (stub, returns 0)\n", (ULONG)object);
    return 0;
}

/* -102: GetDTMethods */
APTR _datatypes_GetDTMethods ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR object __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: GetDTMethods() object=0x%08lx (stub, returns NULL)\n", (ULONG)object);
    return NULL;
}

/* -108: GetDTTriggerMethods */
APTR _datatypes_GetDTTriggerMethods ( register struct Library *DataTypesBase __asm("a6"),
                                      register APTR object __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: GetDTTriggerMethods() object=0x%08lx (stub, returns NULL)\n", (ULONG)object);
    return NULL;
}

/* -114: PrintDTObjectA */
ULONG _datatypes_PrintDTObjectA ( register struct Library *DataTypesBase __asm("a6"),
                                  register APTR object __asm("a0"),
                                  register APTR window __asm("a1"),
                                  register APTR requester __asm("a2"),
                                  register struct TagItem *tags __asm("a3") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: PrintDTObjectA() object=0x%08lx (stub, returns 0)\n", (ULONG)object);
    return 0;
}

/* -138: GetDTString */
STRPTR _datatypes_GetDTString ( register struct Library *DataTypesBase __asm("a6"),
                                register ULONG id __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: GetDTString() id=%ld (stub, returns NULL)\n", id);
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

extern APTR              __g_lxa_datatypes_FuncTab [];
extern struct MyDataInit __g_lxa_datatypes_DataTab;
extern struct InitTable  __g_lxa_datatypes_InitTab;
extern APTR              __g_lxa_datatypes_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                        // UWORD rt_MatchWord
    &ROMTag,                              // struct Resident *rt_MatchTag
    &__g_lxa_datatypes_EndResident,       // APTR  rt_EndSkip
    RTF_AUTOINIT,                         // UBYTE rt_Flags
    VERSION,                              // UBYTE rt_Version
    NT_LIBRARY,                           // UBYTE rt_Type
    0,                                    // BYTE  rt_Pri
    &_g_datatypes_ExLibName[0],           // char  *rt_Name
    &_g_datatypes_ExLibID[0],             // char  *rt_IdString
    &__g_lxa_datatypes_InitTab            // APTR  rt_Init
};

APTR __g_lxa_datatypes_EndResident;
struct Resident *__lxa_datatypes_ROMTag = &ROMTag;

struct InitTable __g_lxa_datatypes_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_datatypes_FuncTab[0],
    (APTR)                &__g_lxa_datatypes_DataTab,
    (APTR)                __g_lxa_datatypes_InitLib
};

/*
 * Function table
 *
 * Standard library functions at -6 through -24
 * Then API functions starting at -30
 * Last entry at -138: GetDTString
 * Total: 4 standard + 19 API = 23 vectors
 */
APTR __g_lxa_datatypes_FuncTab [] =
{
    __g_lxa_datatypes_OpenLib,           // -6   Open
    __g_lxa_datatypes_CloseLib,          // -12  Close
    __g_lxa_datatypes_ExpungeLib,        // -18  Expunge
    __g_lxa_datatypes_ExtFuncLib,        // -24  Reserved
    /* API functions starting at -30 */
    _datatypes_Reserved,                 // -30  (private)
    _datatypes_ObtainDataTypeA,          // -36  ObtainDataTypeA
    _datatypes_ReleaseDataType,          // -42  ReleaseDataType
    _datatypes_NewDTObjectA,             // -48  NewDTObjectA
    _datatypes_DisposeDTObject,          // -54  DisposeDTObject
    _datatypes_SetDTAttrsA,              // -60  SetDTAttrsA
    _datatypes_GetDTAttrsA,              // -66  GetDTAttrsA
    _datatypes_AddDTObject,              // -72  AddDTObject
    _datatypes_RefreshDTObjectA,         // -78  RefreshDTObjectA
    _datatypes_DoAsyncLayout,            // -84  DoAsyncLayout
    _datatypes_DoDTMethodA,              // -90  DoDTMethodA
    _datatypes_RemoveDTObject,           // -96  RemoveDTObject
    _datatypes_GetDTMethods,             // -102 GetDTMethods
    _datatypes_GetDTTriggerMethods,      // -108 GetDTTriggerMethods
    _datatypes_PrintDTObjectA,           // -114 PrintDTObjectA
    _datatypes_Reserved,                 // -120 (private)
    _datatypes_Reserved,                 // -126 (private)
    _datatypes_Reserved,                 // -132 (private)
    _datatypes_GetDTString,              // -138 GetDTString
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_datatypes_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_datatypes_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_datatypes_ExLibID[0],
    (ULONG) 0
};
