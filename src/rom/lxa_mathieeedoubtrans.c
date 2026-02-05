/*
 * lxa_mathieeedoubtrans.c - IEEE Double Precision Transcendental Math Library
 *
 * This library implements the mathieeedoubtrans.library for lxa.
 * All transcendental math operations are delegated to the host via EMU_CALL
 * because ROM code cannot use soft-float library functions.
 *
 * Amiga 68k calling convention for doubles:
 * - Input:  high 32 bits in D0, low 32 bits in D1
 * - Output: high 32 bits in D0, low 32 bits in D1
 * - For two-operand: D0:D1 = arg, D2:D3 = exp (for pow)
 *
 * Phase 61: mathieeedoubtrans.library Implementation
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
#define EXLIBNAME  "mathieeedoubtrans"
#define EXLIBVER   " 40.1 (2026/02/05)"

char __aligned _g_mathieeedoubtrans_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_mathieeedoubtrans_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_mathieeedoubtrans_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_mathieeedoubtrans_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* Library initialization */
struct Library * __g_lxa_mathieeedoubtrans_InitLib (register struct Library *lib    __asm("d0"),
                                                     register BPTR           seglist __asm("a0"),
                                                     register struct ExecBase *sysb  __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_mathieeedoubtrans: InitLib() called\n");
    return lib;
}

struct Library * __g_lxa_mathieeedoubtrans_OpenLib (register struct Library *lib __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_mathieeedoubtrans: OpenLib() called\n");
    lib->lib_OpenCnt++;
    lib->lib_Flags &= ~LIBF_DELEXP;
    return lib;
}

BPTR __g_lxa_mathieeedoubtrans_CloseLib (register struct Library *lib __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_mathieeedoubtrans: CloseLib() called\n");
    lib->lib_OpenCnt--;
    return (BPTR)0;
}

BPTR __g_lxa_mathieeedoubtrans_ExpungeLib (register struct Library *lib __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_mathieeedoubtrans_ExtFuncLib(void)
{
    return 0;
}

/*
 * IEEEDPAtan - Arc tangent
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = atan(input) in radians
 *
 * Uses EMU_CALL_IEEEDP_ATAN
 */
asm(
"_mathieeedoubtrans_IEEEDPAtan:                               \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5020, d0           | EMU_CALL_IEEEDP_ATAN        \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPAtan(void);

/*
 * IEEEDPSin - Sine
 * 
 * Input:  D0:D1 = IEEE double (radians)
 * Output: D0:D1 = sin(input)
 */
asm(
"_mathieeedoubtrans_IEEEDPSin:                                \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5021, d0           | EMU_CALL_IEEEDP_SIN         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPSin(void);

/*
 * IEEEDPCos - Cosine
 * 
 * Input:  D0:D1 = IEEE double (radians)
 * Output: D0:D1 = cos(input)
 */
asm(
"_mathieeedoubtrans_IEEEDPCos:                                \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5022, d0           | EMU_CALL_IEEEDP_COS         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPCos(void);

/*
 * IEEEDPTan - Tangent
 * 
 * Input:  D0:D1 = IEEE double (radians)
 * Output: D0:D1 = tan(input)
 */
asm(
"_mathieeedoubtrans_IEEEDPTan:                                \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5023, d0           | EMU_CALL_IEEEDP_TAN         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPTan(void);

/*
 * IEEEDPSincos - Sine and Cosine
 * 
 * Input:  D0:D1 = IEEE double (radians), A0 = pointer to store cos
 * Output: D0:D1 = sin(input), *A0 = cos(input)
 *
 * Note: a0 contains pointer where cos result should be stored
 */
asm(
"_mathieeedoubtrans_IEEEDPSincos:                             \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5024, d0           | EMU_CALL_IEEEDP_SINCOS      \n"
"   illegal                     | host stores cos to (a0)     \n"
"   rts                         | d0:d1 = sin result          \n"
);
extern void mathieeedoubtrans_IEEEDPSincos(void);

/*
 * IEEEDPSinh - Hyperbolic sine
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = sinh(input)
 */
asm(
"_mathieeedoubtrans_IEEEDPSinh:                               \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5025, d0           | EMU_CALL_IEEEDP_SINH        \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPSinh(void);

/*
 * IEEEDPCosh - Hyperbolic cosine
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = cosh(input)
 */
asm(
"_mathieeedoubtrans_IEEEDPCosh:                               \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5026, d0           | EMU_CALL_IEEEDP_COSH        \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPCosh(void);

/*
 * IEEEDPTanh - Hyperbolic tangent
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = tanh(input)
 */
asm(
"_mathieeedoubtrans_IEEEDPTanh:                               \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5027, d0           | EMU_CALL_IEEEDP_TANH        \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPTanh(void);

/*
 * IEEEDPExp - Natural exponential (e^x)
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = exp(input)
 */
asm(
"_mathieeedoubtrans_IEEEDPExp:                                \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5028, d0           | EMU_CALL_IEEEDP_EXP         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPExp(void);

/*
 * IEEEDPLog - Natural logarithm
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = log(input)
 */
asm(
"_mathieeedoubtrans_IEEEDPLog:                                \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5029, d0           | EMU_CALL_IEEEDP_LOG         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPLog(void);

/*
 * IEEEDPPow - Power function (arg^exp)
 * 
 * Input:  D0:D1 = arg (base), D2:D3 = exp (exponent)
 * Output: D0:D1 = arg^exp
 *
 * Note: FD file says (exp,arg)(d2/d3,d0/d1) so exp is in d2:d3, arg in d0:d1
 */
asm(
"_mathieeedoubtrans_IEEEDPPow:                                \n"
"   move.l  d3, d4              | d4 = exp lo                 \n"
"   move.l  d2, d3              | d3 = exp hi                 \n"
"   move.l  d1, d2              | d2 = arg lo                 \n"
"   move.l  d0, d1              | d1 = arg hi                 \n"
"   move.l  #5030, d0           | EMU_CALL_IEEEDP_POW         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPPow(void);

/*
 * IEEEDPSqrt - Square root
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = sqrt(input)
 */
asm(
"_mathieeedoubtrans_IEEEDPSqrt:                               \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5031, d0           | EMU_CALL_IEEEDP_SQRT        \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPSqrt(void);

/*
 * IEEEDPTieee - Convert double to IEEE single
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0 = IEEE single
 */
asm(
"_mathieeedoubtrans_IEEEDPTieee:                              \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5032, d0           | EMU_CALL_IEEEDP_TIEEE       \n"
"   illegal                                                   \n"
"   rts                         | d0 = single result          \n"
);
extern void mathieeedoubtrans_IEEEDPTieee(void);

/*
 * IEEEDPFieee - Convert IEEE single to double
 * 
 * Input:  D0 = IEEE single
 * Output: D0:D1 = IEEE double
 */
asm(
"_mathieeedoubtrans_IEEEDPFieee:                              \n"
"   move.l  d0, d1              | d1 = single input           \n"
"   move.l  #5033, d0           | EMU_CALL_IEEEDP_FIEEE       \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPFieee(void);

/*
 * IEEEDPAsin - Arc sine
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = asin(input) in radians
 */
asm(
"_mathieeedoubtrans_IEEEDPAsin:                               \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5034, d0           | EMU_CALL_IEEEDP_ASIN        \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPAsin(void);

/*
 * IEEEDPAcos - Arc cosine
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = acos(input) in radians
 */
asm(
"_mathieeedoubtrans_IEEEDPAcos:                               \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5035, d0           | EMU_CALL_IEEEDP_ACOS        \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPAcos(void);

/*
 * IEEEDPLog10 - Base-10 logarithm
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = log10(input)
 */
asm(
"_mathieeedoubtrans_IEEEDPLog10:                              \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5036, d0           | EMU_CALL_IEEEDP_LOG10       \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubtrans_IEEEDPLog10(void);

/*
 * Library initialization structures
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

extern APTR              __g_lxa_mathieeedoubtrans_FuncTab [];
extern struct MyDataInit __g_lxa_mathieeedoubtrans_DataTab;
extern struct InitTable  __g_lxa_mathieeedoubtrans_InitTab;
extern APTR              __g_lxa_mathieeedoubtrans_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                         // UWORD rt_MatchWord
    &ROMTag,                               // struct Resident *rt_MatchTag
    &__g_lxa_mathieeedoubtrans_EndResident,// APTR  rt_EndSkip
    RTF_AUTOINIT,                          // UBYTE rt_Flags
    VERSION,                               // UBYTE rt_Version
    NT_LIBRARY,                            // UBYTE rt_Type
    0,                                     // BYTE  rt_Pri
    &_g_mathieeedoubtrans_ExLibName[0],    // char  *rt_Name
    &_g_mathieeedoubtrans_ExLibID[0],      // char  *rt_IdString
    &__g_lxa_mathieeedoubtrans_InitTab     // APTR  rt_Init
};

APTR __g_lxa_mathieeedoubtrans_EndResident;
struct Resident *__lxa_mathieeedoubtrans_ROMTag = &ROMTag;

struct InitTable __g_lxa_mathieeedoubtrans_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_mathieeedoubtrans_FuncTab[0],
    (APTR)                &__g_lxa_mathieeedoubtrans_DataTab,
    (APTR)                __g_lxa_mathieeedoubtrans_InitLib
};

APTR __g_lxa_mathieeedoubtrans_FuncTab [] =
{
    __g_lxa_mathieeedoubtrans_OpenLib,
    __g_lxa_mathieeedoubtrans_CloseLib,
    __g_lxa_mathieeedoubtrans_ExpungeLib,
    __g_lxa_mathieeedoubtrans_ExtFuncLib,
    mathieeedoubtrans_IEEEDPAtan,    // offset = -30
    mathieeedoubtrans_IEEEDPSin,     // offset = -36
    mathieeedoubtrans_IEEEDPCos,     // offset = -42
    mathieeedoubtrans_IEEEDPTan,     // offset = -48
    mathieeedoubtrans_IEEEDPSincos,  // offset = -54
    mathieeedoubtrans_IEEEDPSinh,    // offset = -60
    mathieeedoubtrans_IEEEDPCosh,    // offset = -66
    mathieeedoubtrans_IEEEDPTanh,    // offset = -72
    mathieeedoubtrans_IEEEDPExp,     // offset = -78
    mathieeedoubtrans_IEEEDPLog,     // offset = -84
    mathieeedoubtrans_IEEEDPPow,     // offset = -90
    mathieeedoubtrans_IEEEDPSqrt,    // offset = -96
    mathieeedoubtrans_IEEEDPTieee,   // offset = -102
    mathieeedoubtrans_IEEEDPFieee,   // offset = -108
    mathieeedoubtrans_IEEEDPAsin,    // offset = -114
    mathieeedoubtrans_IEEEDPAcos,    // offset = -120
    mathieeedoubtrans_IEEEDPLog10,   // offset = -126
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_mathieeedoubtrans_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_mathieeedoubtrans_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_mathieeedoubtrans_ExLibID[0],
    (ULONG) 0
};
