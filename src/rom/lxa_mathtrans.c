/*
 * lxa_mathtrans.c - Motorola FFP transcendental math library
 *
 * lxa stores Motorola fast floating point values in registers, but hosted
 * ROM code cannot evaluate transcendentals directly. Bridge the public
 * mathtrans.library entry points through dedicated EMU_CALL handlers that
 * convert between FFP and host IEEE single precision.
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "mathtrans"
#define EXLIBVER   " 40.1 (2026/03/11)"

char __aligned _g_mathtrans_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_mathtrans_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_mathtrans_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_mathtrans_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

struct Library * __g_lxa_mathtrans_InitLib(register struct Library *lib     __asm("d0"),
                                           register BPTR            seglist __asm("a0"),
                                           register struct ExecBase *sysb   __asm("a6"))
{
    DPRINTF(LOG_DEBUG, "_mathtrans: InitLib() called\n");
    return lib;
}

struct Library * __g_lxa_mathtrans_OpenLib(register struct Library *lib __asm("a6"))
{
    DPRINTF(LOG_DEBUG, "_mathtrans: OpenLib() called\n");
    lib->lib_OpenCnt++;
    lib->lib_Flags &= ~LIBF_DELEXP;
    return lib;
}

BPTR __g_lxa_mathtrans_CloseLib(register struct Library *lib __asm("a6"))
{
    lib->lib_OpenCnt--;
    return (BPTR)0;
}

BPTR __g_lxa_mathtrans_ExpungeLib(register struct Library *lib __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_mathtrans_ExtFuncLib(void)
{
    return 0;
}

asm(
"_mathtrans_SPAtan:                                              \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5120, d0           | EMU_CALL_FFP_ATAN             \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPAtan(void);

asm(
"_mathtrans_SPSin:                                               \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5121, d0           | EMU_CALL_FFP_SIN              \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPSin(void);

asm(
"_mathtrans_SPCos:                                               \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5122, d0           | EMU_CALL_FFP_COS              \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPCos(void);

asm(
"_mathtrans_SPTan:                                               \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5123, d0           | EMU_CALL_FFP_TAN              \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPTan(void);

asm(
"_mathtrans_SPSincos:                                            \n"
"   move.l  d2, -(sp)           | save d2                       \n"
"   move.l  d1, d2              | d2 = cos result pointer       \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5124, d0           | EMU_CALL_FFP_SINCOS           \n"
"   illegal                                                    \n"
"   move.l  (sp)+, d2           | restore d2                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPSincos(void);

asm(
"_mathtrans_SPSinh:                                              \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5125, d0           | EMU_CALL_FFP_SINH             \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPSinh(void);

asm(
"_mathtrans_SPCosh:                                              \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5126, d0           | EMU_CALL_FFP_COSH             \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPCosh(void);

asm(
"_mathtrans_SPTanh:                                              \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5127, d0           | EMU_CALL_FFP_TANH             \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPTanh(void);

asm(
"_mathtrans_SPExp:                                               \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5128, d0           | EMU_CALL_FFP_EXP              \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPExp(void);

asm(
"_mathtrans_SPLog:                                               \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5129, d0           | EMU_CALL_FFP_LOG              \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPLog(void);

asm(
"_mathtrans_SPPow:                                               \n"
"   move.l  d2, -(sp)           | save d2                       \n"
"   move.l  d1, d2              | d2 = power                    \n"
"   move.l  d0, d1              | d1 = base                     \n"
"   move.l  #5130, d0           | EMU_CALL_FFP_POW              \n"
"   illegal                                                    \n"
"   move.l  (sp)+, d2           | restore d2                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPPow(void);

asm(
"_mathtrans_SPSqrt:                                              \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5131, d0           | EMU_CALL_FFP_SQRT             \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPSqrt(void);

asm(
"_mathtrans_SPTieee:                                             \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5132, d0           | EMU_CALL_FFP_TIEEE            \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPTieee(void);

asm(
"_mathtrans_SPFieee:                                             \n"
"   move.l  d0, d1              | d1 = ieee single input        \n"
"   move.l  #5133, d0           | EMU_CALL_FFP_FIEEE            \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPFieee(void);

asm(
"_mathtrans_SPAsin:                                              \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5134, d0           | EMU_CALL_FFP_ASIN             \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPAsin(void);

asm(
"_mathtrans_SPAcos:                                              \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5135, d0           | EMU_CALL_FFP_ACOS             \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPAcos(void);

asm(
"_mathtrans_SPLog10:                                             \n"
"   move.l  d0, d1              | d1 = ffp input                \n"
"   move.l  #5136, d0           | EMU_CALL_FFP_LOG10            \n"
"   illegal                                                    \n"
"   rts                                                        \n"
);
extern void mathtrans_SPLog10(void);

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
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_mathtrans_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_LIBRARY,
    0,
    &_g_mathtrans_ExLibName[0],
    &_g_mathtrans_ExLibID[0],
    &__g_lxa_mathtrans_InitTab
};

APTR __g_lxa_mathtrans_EndResident;
struct Resident *__lxa_mathtrans_ROMTag = &ROMTag;

struct InitTable __g_lxa_mathtrans_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_mathtrans_FuncTab[0],
    (APTR)                &__g_lxa_mathtrans_DataTab,
    (APTR)                __g_lxa_mathtrans_InitLib
};

APTR __g_lxa_mathtrans_FuncTab [] =
{
    __g_lxa_mathtrans_OpenLib,
    __g_lxa_mathtrans_CloseLib,
    __g_lxa_mathtrans_ExpungeLib,
    __g_lxa_mathtrans_ExtFuncLib,
    mathtrans_SPAtan,
    mathtrans_SPSin,
    mathtrans_SPCos,
    mathtrans_SPTan,
    mathtrans_SPSincos,
    mathtrans_SPSinh,
    mathtrans_SPCosh,
    mathtrans_SPTanh,
    mathtrans_SPExp,
    mathtrans_SPLog,
    mathtrans_SPPow,
    mathtrans_SPSqrt,
    mathtrans_SPTieee,
    mathtrans_SPFieee,
    mathtrans_SPAsin,
    mathtrans_SPAcos,
    mathtrans_SPLog10,
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
