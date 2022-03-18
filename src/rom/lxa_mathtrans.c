#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include "util.h"
#include "mem_map.h"

#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DPRINTF(lvl, ...) LPRINTF (lvl, __VA_ARGS__)
#else
#define DPRINTF(lvl, ...)
#endif

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "mathtrans"
#define EXLIBVER   " 40.1 (2022/03/04)"

char __aligned _g_mathtrans_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_mathtrans_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_mathtrans_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_mathtrans_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

//struct ExecBase        *SysBase         = (struct ExecBase*)    ((UBYTE *)EXEC_BASE_START);
extern struct ExecBase      *SysBase;

// libBase: MathTransBase
// baseType: struct Library *
// libname: mathtrans.library
struct Library * __saveds __g_lxa_mathtrans_InitLib    ( register struct Library *mathtransb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_mathtrans: WARNING: InitLib() unimplemented STUB called.\n");
    return mathtransb;
}

struct Library * __saveds __g_lxa_mathtrans_OpenLib ( register struct Library  *Library __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_mathtrans: OpenLib() called\n");
    // FIXME Library->dl_lib.lib_OpenCnt++;
    // FIXME Library->dl_lib.lib_Flags &= ~LIBF_DELEXP;
    return Library;
}

BPTR __saveds __g_lxa_mathtrans_CloseLib ( register struct Library  *mathtransb __asm("a6"))
{
    return NULL;
}

BPTR __saveds __g_lxa_mathtrans_ExpungeLib ( register struct Library  *mathtransb      __asm("a6"))
{
    return NULL;
}

ULONG __g_lxa_mathtrans_ExtFuncLib(void)
{
    return NULL;
}

static FLOAT __saveds _mathtrans_SPAtan ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPAtan() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPSin ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPSin() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPCos ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPCos() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPTan ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPTan() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPSincos ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT * cosResult __asm("d1"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPSincos() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPSinh ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPSinh() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPCosh ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPCosh() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPTanh ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPTanh() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPExp ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPExp() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPLog ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPLog() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPPow ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT power __asm("d1"),
                                                        register FLOAT arg __asm("d0"))
{
    DPRINTF (LOG_WARNING, "_mathtrans: SPPow() unimplemented STUB called.\n");
    //assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPSqrt ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPSqrt() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPTieee ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPTieee() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPFieee ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPFieee() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPAsin ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPAsin() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPAcos ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPAcos() unimplemented STUB called.\n");
    assert(FALSE);
}

static FLOAT __saveds _mathtrans_SPLog10 ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathtrans: SPLog10() unimplemented STUB called.\n");
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

extern APTR              __g_lxa_mathtrans_FuncTab [];
extern struct MyDataInit __g_lxa_mathtrans_DataTab;
extern struct InitTable  __g_lxa_mathtrans_InitTab;
extern APTR              __g_lxa_mathtrans_EndResident;

static struct Resident __aligned ROMTag =
{                                 //                               offset
    RTC_MATCHWORD,                // UWORD rt_MatchWord;                0
    &ROMTag,                      // struct Resident *rt_MatchTag;      2
    &__g_lxa_mathtrans_EndResident, // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,                 // UBYTE rt_Flags;                    7
    VERSION,                      // UBYTE rt_Version;                  8
    NT_LIBRARY,                   // UBYTE rt_Type;                     9
    0,                            // BYTE  rt_Pri;                     10
    &_g_mathtrans_ExLibName[0],     // char  *rt_Name;                   14
    &_g_mathtrans_ExLibID[0],       // char  *rt_IdString;               18
    &__g_lxa_mathtrans_InitTab      // APTR  rt_Init;                    22
};

APTR __g_lxa_mathtrans_EndResident;
struct Resident *__lxa_mathtrans_ROMTag = &ROMTag;

struct InitTable __g_lxa_mathtrans_InitTab =
{
    (ULONG)               sizeof(struct Library),         // LibBaseSize
    (APTR              *) &__g_lxa_mathtrans_FuncTab[0],  // FunctionTable
    (APTR)                &__g_lxa_mathtrans_DataTab,     // DataTable
    (APTR)                __g_lxa_mathtrans_InitLib       // InitLibFn
};

APTR __g_lxa_mathtrans_FuncTab [] =
{
    __g_lxa_mathtrans_OpenLib,
    __g_lxa_mathtrans_CloseLib,
    __g_lxa_mathtrans_ExpungeLib,
    __g_lxa_mathtrans_ExtFuncLib,
    _mathtrans_SPAtan, // offset = -30
    _mathtrans_SPSin, // offset = -36
    _mathtrans_SPCos, // offset = -42
    _mathtrans_SPTan, // offset = -48
    _mathtrans_SPSincos, // offset = -54
    _mathtrans_SPSinh, // offset = -60
    _mathtrans_SPCosh, // offset = -66
    _mathtrans_SPTanh, // offset = -72
    _mathtrans_SPExp, // offset = -78
    _mathtrans_SPLog, // offset = -84
    _mathtrans_SPPow, // offset = -90
    _mathtrans_SPSqrt, // offset = -96
    _mathtrans_SPTieee, // offset = -102
    _mathtrans_SPFieee, // offset = -108
    _mathtrans_SPAsin, // offset = -114
    _mathtrans_SPAcos, // offset = -120
    _mathtrans_SPLog10, // offset = -126
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_mathtrans_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_mathtrans_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_mathtrans_ExLibID[0],
    (ULONG) 0
};
