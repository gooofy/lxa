#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <libraries/mathffp.h>

#include "util.h"
#include "mem_map.h"

#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DPRINTF(lvl, ...) LPRINTF (lvl, __VA_ARGS__)
#else
#define DPRINTF(lvl, ...)
#endif

asm(
"    .global ___restore_a4        \n"
"___restore_a4:                   \n"
"    _geta4:     lea ___a4_init,a4\n"
"                rts              \n"
);

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "mathffp"
#define EXLIBVER   " 40.1 (2022/03/03)"

char __aligned _g_mathffp_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_mathffp_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_mathffp_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_mathffp_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

struct ExecBase        *SysBase         = (struct ExecBase*)    ((UBYTE *)EXEC_BASE_START);

// libBase: MathBase
// baseType: struct Library *
// libname: mathffp.library
struct MathBase * __saveds __g_lxa_mathffp_InitLib    ( register struct MathBase *mathffpb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_mathffp: WARNING: InitLib() unimplemented STUB called.\n");
    return mathffpb;
}

struct MathBase * __saveds __g_lxa_mathffp_OpenLib ( register struct MathBase  *MathBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_mathffp: OpenLib() called\n");
    // FIXME MathBase->dl_lib.lib_OpenCnt++;
    // FIXME MathBase->dl_lib.lib_Flags &= ~LIBF_DELEXP;
    return MathBase;
}

BPTR __saveds __g_lxa_mathffp_CloseLib ( register struct MathBase  *mathffpb __asm("a6"))
{
    return NULL;
}

BPTR __saveds __g_lxa_mathffp_ExpungeLib ( register struct MathBase  *mathffpb      __asm("a6"))
{
    return NULL;
}

ULONG __g_lxa_mathffp_ExtFuncLib(void)
{
    return NULL;
}

static LONG __saveds _mathffp_SPFix ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPFix() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathffp_SPFlt ( register struct Library * MathBase __asm("a6"),
                                                        register LONG integer __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPFlt() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _mathffp_SPCmp ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT leftParm __asm("d1"),
                                                        register FLOAT rightParm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPCmp() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _mathffp_SPTst ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT parm __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPTst() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathffp_SPAbs ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPAbs() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathffp_SPNeg ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPNeg() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathffp_SPAdd ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT leftParm __asm("d1"),
                                                        register FLOAT rightParm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPAdd() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathffp_SPSub ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT leftParm __asm("d1"),
                                                        register FLOAT rightParm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPSub() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathffp_SPMul ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT leftParm __asm("d1"),
                                                        register FLOAT rightParm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPMul() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathffp_SPDiv ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT leftParm __asm("d1"),
                                                        register FLOAT rightParm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPDiv() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathffp_SPFloor ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPFloor() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathffp_SPCeil ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPCeil() unimplemented STUB called.\n");
    assert(FALSE);
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

extern APTR              __g_lxa_mathffp_FuncTab [];
extern struct MyDataInit __g_lxa_mathffp_DataTab;
extern struct InitTable  __g_lxa_mathffp_InitTab;
extern APTR              __g_lxa_mathffp_EndResident;

static struct Resident __aligned ROMTag =
{                                 //                               offset
    RTC_MATCHWORD,                // UWORD rt_MatchWord;                0
    &ROMTag,                      // struct Resident *rt_MatchTag;      2
    &__g_lxa_mathffp_EndResident, // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,                 // UBYTE rt_Flags;                    7
    VERSION,                      // UBYTE rt_Version;                  8
    NT_LIBRARY,                   // UBYTE rt_Type;                     9
    0,                            // BYTE  rt_Pri;                     10
    &_g_mathffp_ExLibName[0],     // char  *rt_Name;                   14
    &_g_mathffp_ExLibID[0],       // char  *rt_IdString;               18
    &__g_lxa_mathffp_InitTab      // APTR  rt_Init;                    22
};

APTR __g_lxa_mathffp_EndResident;
struct Resident *__lxa_mathffp_ROMTag = &ROMTag;

struct InitTable __g_lxa_mathffp_InitTab =
{
    (ULONG)               sizeof(struct Library),       // LibBaseSize
    (APTR              *) &__g_lxa_mathffp_FuncTab[0],  // FunctionTable
    (APTR)                &__g_lxa_mathffp_DataTab,     // DataTable
    (APTR)                __g_lxa_mathffp_InitLib       // InitLibFn
};

APTR __g_lxa_mathffp_FuncTab [] =
{
    __g_lxa_mathffp_OpenLib,
    __g_lxa_mathffp_CloseLib,
    __g_lxa_mathffp_ExpungeLib,
    __g_lxa_mathffp_ExtFuncLib,
    _mathffp_SPFix, // offset = -30
    _mathffp_SPFlt, // offset = -36
    _mathffp_SPCmp, // offset = -42
    _mathffp_SPTst, // offset = -48
    _mathffp_SPAbs, // offset = -54
    _mathffp_SPNeg, // offset = -60
    _mathffp_SPAdd, // offset = -66
    _mathffp_SPSub, // offset = -72
    _mathffp_SPMul, // offset = -78
    _mathffp_SPDiv, // offset = -84
    _mathffp_SPFloor, // offset = -90
    _mathffp_SPCeil, // offset = -96
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_mathffp_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_mathffp_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_mathffp_ExLibID[0],
    (ULONG) 0
};

