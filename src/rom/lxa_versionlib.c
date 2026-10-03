/*
 * lxa version.library
 *
 * The Workbench release marker of AmigaOS: LIBS:version.library has no
 * functions of its own, its version is the Workbench version C:Version
 * reports ("Kickstart 40.70, Workbench 40.42"; AmigaOS 3.1, verified on
 * the reference, Phase 221).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include "util.h"

#define VERSION    40
#define REVISION   42
#define EXLIBNAME  "version"
#define EXLIBVER   " 40.42 (02/18/94)"

char __aligned _g_version_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_version_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_version_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_version_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

struct Library * __g_lxa_version_InitLib ( register struct Library   *libbase __asm("d0"),
                                           register BPTR              seglist __asm("a0"),
                                           register struct ExecBase  *sysb __asm("a6"))
{
    return libbase;
}

BPTR __g_lxa_version_ExpungeLib ( register struct Library *libbase __asm("a6") )
{
    return 0;
}

struct Library * __g_lxa_version_OpenLib ( register struct Library *libbase __asm("a6") )
{
    libbase->lib_OpenCnt++;
    libbase->lib_Flags &= ~LIBF_DELEXP;
    return libbase;
}

BPTR __g_lxa_version_CloseLib ( register struct Library *libbase __asm("a6") )
{
    libbase->lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_version_ExtFuncLib ( void )
{
    return 0;
}

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

extern APTR __g_lxa_version_FuncTab [];
extern struct MyDataInit __g_lxa_version_DataTab;
extern struct InitTable __g_lxa_version_InitTab;
extern APTR __g_lxa_version_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_version_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_LIBRARY,
    0,
    &_g_version_ExLibName[0],
    &_g_version_ExLibID[0],
    &__g_lxa_version_InitTab
};

APTR __g_lxa_version_EndResident;
struct Resident *__lxa_version_ROMTag = &ROMTag;

struct InitTable __g_lxa_version_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_version_FuncTab[0],
    (APTR)                &__g_lxa_version_DataTab,
    (APTR)                __g_lxa_version_InitLib
};

APTR __g_lxa_version_FuncTab [] =
{
    __g_lxa_version_OpenLib,
    __g_lxa_version_CloseLib,
    __g_lxa_version_ExpungeLib,
    __g_lxa_version_ExtFuncLib,
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_version_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_version_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_version_ExLibID[0],
    (ULONG) 0
};
