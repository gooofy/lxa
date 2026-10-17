/*
 * lxa 68040.library
 *
 * On an A4000/040 AmigaOS 3.1 loads LIBS:68040.library through SetPatch; it
 * supplies the 68040 support code (emulation of the FPU instructions the
 * 68040 lacks, cache handling).  The library stays open, so programs find
 * it in SysBase->LibList and may check its version (probe exec/machine:
 * 37.30 with six vectors, lib_NegSize 36, on the reference).
 *
 * lxa's emulated 68040 executes every 68881/68882 instruction itself and
 * exec already reports AFF_FPU40, enables both caches and moves the VBR to
 * fast RAM at boot, so this library has no work to do at init time.  Its
 * two vectors after ExtFunc are private: nothing documents them, calls are
 * reported as private slots.
 *
 * This is lxa's own implementation; it shares nothing with the Commodore
 * binary except the observable version number.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include "util.h"

#define VERSION    37
#define REVISION   30
#define EXLIBNAME  "68040"
#define EXLIBVER   " 37.30 (18.1.93)"

char __aligned _g_68040_ExLibName [] = EXLIBNAME ".library";
/* the IdString ends in a line feed (reference) */
char __aligned _g_68040_ExLibID   [] = EXLIBNAME EXLIBVER "\n";
char __aligned _g_68040_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_68040_VERSTRING [] = "\0$VER: " EXLIBNAME ".library" EXLIBVER;

extern struct ExecBase *SysBase;

struct Library * __g_lxa_68040_InitLib ( register struct Library   *libbase __asm("d0"),
                                         register BPTR              seglist __asm("a0"),
                                         register struct ExecBase  *sysb __asm("a6"))
{
    return libbase;
}

BPTR __g_lxa_68040_ExpungeLib ( register struct Library *libbase __asm("a6") )
{
    /* the CPU support library stays resident */
    return 0;
}

struct Library * __g_lxa_68040_OpenLib ( register struct Library *libbase __asm("a6") )
{
    libbase->lib_OpenCnt++;
    libbase->lib_Flags &= ~LIBF_DELEXP;
    return libbase;
}

BPTR __g_lxa_68040_CloseLib ( register struct Library *libbase __asm("a6") )
{
    libbase->lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_68040_ExtFuncLib ( void )
{
    return 0;
}

ULONG __g_lxa_68040_Private1 ( void )
{
    PRIVATE_FUNCTION_ERROR("_68040", "Private1");
    return 0;
}

ULONG __g_lxa_68040_Private2 ( void )
{
    PRIVATE_FUNCTION_ERROR("_68040", "Private2");
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

extern APTR __g_lxa_68040_FuncTab [];
extern struct MyDataInit __g_lxa_68040_DataTab;
extern struct InitTable __g_lxa_68040_InitTab;
extern APTR __g_lxa_68040_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_68040_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_LIBRARY,
    0,
    &_g_68040_ExLibName[0],
    &_g_68040_ExLibID[0],
    &__g_lxa_68040_InitTab
};

APTR __g_lxa_68040_EndResident;
struct Resident *__lxa_68040_ROMTag = &ROMTag;

struct InitTable __g_lxa_68040_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_68040_FuncTab[0],
    (APTR)                &__g_lxa_68040_DataTab,
    (APTR)                __g_lxa_68040_InitLib
};

APTR __g_lxa_68040_FuncTab [] =
{
    __g_lxa_68040_OpenLib,
    __g_lxa_68040_CloseLib,
    __g_lxa_68040_ExpungeLib,
    __g_lxa_68040_ExtFuncLib,
    __g_lxa_68040_Private1,
    __g_lxa_68040_Private2,
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_68040_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_68040_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_68040_ExLibID[0],
    (ULONG) 0
};
