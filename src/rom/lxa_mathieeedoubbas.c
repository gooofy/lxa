/*
 * lxa_mathieeedoubbas.c - IEEE Double Precision Basic Math Library
 *
 * This library implements the mathieeedoubbas.library for lxa.
 * All double-precision math operations are delegated to the host via EMU_CALL
 * because ROM code cannot use soft-float library functions.
 *
 * Amiga 68k calling convention for doubles:
 * - Input:  high 32 bits in D0, low 32 bits in D1
 * - Output: high 32 bits in D0, low 32 bits in D1
 * - For two-operand: D0:D1 = left, D2:D3 = right
 *
 * Phase 61: mathieeedoubbas.library Implementation
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
#define EXLIBNAME  "mathieeedoubbas"
#define EXLIBVER   " 40.1 (2026/02/05)"

char __aligned _g_mathieeedoubbas_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_mathieeedoubbas_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_mathieeedoubbas_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_mathieeedoubbas_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* Library initialization */
struct Library * __g_lxa_mathieeedoubbas_InitLib (register struct Library *lib    __asm("d0"),
                                                   register BPTR           seglist __asm("a0"),
                                                   register struct ExecBase *sysb  __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_mathieeedoubbas: InitLib() called\n");
    return lib;
}

struct Library * __g_lxa_mathieeedoubbas_OpenLib (register struct Library *lib __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_mathieeedoubbas: OpenLib() called\n");
    lib->lib_OpenCnt++;
    lib->lib_Flags &= ~LIBF_DELEXP;
    return lib;
}

BPTR __g_lxa_mathieeedoubbas_CloseLib (register struct Library *lib __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_mathieeedoubbas: CloseLib() called\n");
    lib->lib_OpenCnt--;
    return (BPTR)0;
}

BPTR __g_lxa_mathieeedoubbas_ExpungeLib (register struct Library *lib __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_mathieeedoubbas_ExtFuncLib(void)
{
    return 0;
}

/*
 * IEEEDPFix - Convert IEEE double to integer (truncate toward zero)
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0 = LONG integer
 *
 * Uses EMU_CALL_IEEEDP_FIX: d1:d2=double -> d0=LONG
 */
asm(
"_mathieeedoubbas_IEEEDPFix:                                  \n"
"   move.l  d1, d2              | d2 = lo (from d1)           \n"
"   move.l  d0, d1              | d1 = hi (from d0)           \n"
"   move.l  #5000, d0           | EMU_CALL_IEEEDP_FIX         \n"
"   illegal                                                   \n"
"   rts                         | d0 = LONG result            \n"
);
extern void mathieeedoubbas_IEEEDPFix(void);

/*
 * IEEEDPFlt - Convert integer to IEEE double
 * 
 * Input:  D0 = LONG integer
 * Output: D0:D1 = IEEE double
 *
 * Uses EMU_CALL_IEEEDP_FLT: d1=LONG -> d0:d1=double
 */
asm(
"_mathieeedoubbas_IEEEDPFlt:                                  \n"
"   move.l  d0, d1              | d1 = LONG input             \n"
"   move.l  #5001, d0           | EMU_CALL_IEEEDP_FLT         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubbas_IEEEDPFlt(void);

/*
 * IEEEDPCmp - Compare two IEEE doubles
 * 
 * Input:  D0:D1 = left operand, D2:D3 = right operand
 * Output: D0 = -1 (left < right), 0 (equal), +1 (left > right)
 *         Also sets condition codes: N (negative), Z (zero)
 *
 * Uses EMU_CALL_IEEEDP_CMP: d1:d2=left, d3:d4=right -> d0=-1,0,1
 */
asm(
"_mathieeedoubbas_IEEEDPCmp:                                  \n"
"   move.l  d3, d4              | d4 = right lo               \n"
"   move.l  d2, d3              | d3 = right hi               \n"
"   move.l  d1, d2              | d2 = left lo                \n"
"   move.l  d0, d1              | d1 = left hi                \n"
"   move.l  #5002, d0           | EMU_CALL_IEEEDP_CMP         \n"
"   illegal                                                   \n"
"   tst.l   d0                  | set condition codes         \n"
"   rts                                                       \n"
);
extern void mathieeedoubbas_IEEEDPCmp(void);

/*
 * IEEEDPTst - Test IEEE double against zero
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0 = -1 (negative), 0 (zero), +1 (positive)
 *
 * Uses EMU_CALL_IEEEDP_TST: d1:d2=double -> d0=-1,0,1
 */
asm(
"_mathieeedoubbas_IEEEDPTst:                                  \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5003, d0           | EMU_CALL_IEEEDP_TST         \n"
"   illegal                                                   \n"
"   tst.l   d0                  | set condition codes         \n"
"   rts                                                       \n"
);
extern void mathieeedoubbas_IEEEDPTst(void);

/*
 * IEEEDPAbs - Absolute value
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = |input|
 *
 * Uses EMU_CALL_IEEEDP_ABS: d1:d2=double -> d0:d1=double
 */
asm(
"_mathieeedoubbas_IEEEDPAbs:                                  \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5004, d0           | EMU_CALL_IEEEDP_ABS         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubbas_IEEEDPAbs(void);

/*
 * IEEEDPNeg - Negate
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = -input
 *
 * Uses EMU_CALL_IEEEDP_NEG: d1:d2=double -> d0:d1=double
 */
asm(
"_mathieeedoubbas_IEEEDPNeg:                                  \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5005, d0           | EMU_CALL_IEEEDP_NEG         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubbas_IEEEDPNeg(void);

/*
 * IEEEDPAdd - Addition
 * 
 * Input:  D0:D1 = left, D2:D3 = right
 * Output: D0:D1 = left + right
 *
 * Uses EMU_CALL_IEEEDP_ADD: d1:d2=left, d3:d4=right -> d0:d1=double
 */
asm(
"_mathieeedoubbas_IEEEDPAdd:                                  \n"
"   move.l  d3, d4              | d4 = right lo               \n"
"   move.l  d2, d3              | d3 = right hi               \n"
"   move.l  d1, d2              | d2 = left lo                \n"
"   move.l  d0, d1              | d1 = left hi                \n"
"   move.l  #5006, d0           | EMU_CALL_IEEEDP_ADD         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubbas_IEEEDPAdd(void);

/*
 * IEEEDPSub - Subtraction
 * 
 * Input:  D0:D1 = left, D2:D3 = right
 * Output: D0:D1 = left - right
 *
 * Uses EMU_CALL_IEEEDP_SUB: d1:d2=left, d3:d4=right -> d0:d1=double
 */
asm(
"_mathieeedoubbas_IEEEDPSub:                                  \n"
"   move.l  d3, d4              | d4 = right lo               \n"
"   move.l  d2, d3              | d3 = right hi               \n"
"   move.l  d1, d2              | d2 = left lo                \n"
"   move.l  d0, d1              | d1 = left hi                \n"
"   move.l  #5007, d0           | EMU_CALL_IEEEDP_SUB         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubbas_IEEEDPSub(void);

/*
 * IEEEDPMul - Multiplication
 * 
 * Input:  D0:D1 = factor1, D2:D3 = factor2
 * Output: D0:D1 = factor1 * factor2
 *
 * Uses EMU_CALL_IEEEDP_MUL: d1:d2=left, d3:d4=right -> d0:d1=double
 */
asm(
"_mathieeedoubbas_IEEEDPMul:                                  \n"
"   move.l  d3, d4              | d4 = f2 lo                  \n"
"   move.l  d2, d3              | d3 = f2 hi                  \n"
"   move.l  d1, d2              | d2 = f1 lo                  \n"
"   move.l  d0, d1              | d1 = f1 hi                  \n"
"   move.l  #5008, d0           | EMU_CALL_IEEEDP_MUL         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubbas_IEEEDPMul(void);

/*
 * IEEEDPDiv - Division
 * 
 * Input:  D0:D1 = dividend, D2:D3 = divisor
 * Output: D0:D1 = dividend / divisor
 *
 * Uses EMU_CALL_IEEEDP_DIV: d1:d2=dividend, d3:d4=divisor -> d0:d1=double
 */
asm(
"_mathieeedoubbas_IEEEDPDiv:                                  \n"
"   move.l  d3, d4              | d4 = divisor lo             \n"
"   move.l  d2, d3              | d3 = divisor hi             \n"
"   move.l  d1, d2              | d2 = dividend lo            \n"
"   move.l  d0, d1              | d1 = dividend hi            \n"
"   move.l  #5009, d0           | EMU_CALL_IEEEDP_DIV         \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubbas_IEEEDPDiv(void);

/*
 * IEEEDPFloor - Largest integer not greater than input
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = floor(input)
 *
 * Uses EMU_CALL_IEEEDP_FLOOR: d1:d2=double -> d0:d1=double
 */
asm(
"_mathieeedoubbas_IEEEDPFloor:                                \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5010, d0           | EMU_CALL_IEEEDP_FLOOR       \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubbas_IEEEDPFloor(void);

/*
 * IEEEDPCeil - Smallest integer not less than input
 * 
 * Input:  D0:D1 = IEEE double
 * Output: D0:D1 = ceil(input)
 *
 * Uses EMU_CALL_IEEEDP_CEIL: d1:d2=double -> d0:d1=double
 */
asm(
"_mathieeedoubbas_IEEEDPCeil:                                 \n"
"   move.l  d1, d2              | d2 = lo                     \n"
"   move.l  d0, d1              | d1 = hi                     \n"
"   move.l  #5011, d0           | EMU_CALL_IEEEDP_CEIL        \n"
"   illegal                                                   \n"
"   rts                         | d0:d1 = double result       \n"
);
extern void mathieeedoubbas_IEEEDPCeil(void);

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

extern APTR              __g_lxa_mathieeedoubbas_FuncTab [];
extern struct MyDataInit __g_lxa_mathieeedoubbas_DataTab;
extern struct InitTable  __g_lxa_mathieeedoubbas_InitTab;
extern APTR              __g_lxa_mathieeedoubbas_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                        // UWORD rt_MatchWord
    &ROMTag,                              // struct Resident *rt_MatchTag
    &__g_lxa_mathieeedoubbas_EndResident, // APTR  rt_EndSkip
    RTF_AUTOINIT,                         // UBYTE rt_Flags
    VERSION,                              // UBYTE rt_Version
    NT_LIBRARY,                           // UBYTE rt_Type
    0,                                    // BYTE  rt_Pri
    &_g_mathieeedoubbas_ExLibName[0],     // char  *rt_Name
    &_g_mathieeedoubbas_ExLibID[0],       // char  *rt_IdString
    &__g_lxa_mathieeedoubbas_InitTab      // APTR  rt_Init
};

APTR __g_lxa_mathieeedoubbas_EndResident;
struct Resident *__lxa_mathieeedoubbas_ROMTag = &ROMTag;

struct InitTable __g_lxa_mathieeedoubbas_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_mathieeedoubbas_FuncTab[0],
    (APTR)                &__g_lxa_mathieeedoubbas_DataTab,
    (APTR)                __g_lxa_mathieeedoubbas_InitLib
};

APTR __g_lxa_mathieeedoubbas_FuncTab [] =
{
    __g_lxa_mathieeedoubbas_OpenLib,
    __g_lxa_mathieeedoubbas_CloseLib,
    __g_lxa_mathieeedoubbas_ExpungeLib,
    __g_lxa_mathieeedoubbas_ExtFuncLib,
    mathieeedoubbas_IEEEDPFix,   // offset = -30
    mathieeedoubbas_IEEEDPFlt,   // offset = -36
    mathieeedoubbas_IEEEDPCmp,   // offset = -42
    mathieeedoubbas_IEEEDPTst,   // offset = -48
    mathieeedoubbas_IEEEDPAbs,   // offset = -54
    mathieeedoubbas_IEEEDPNeg,   // offset = -60
    mathieeedoubbas_IEEEDPAdd,   // offset = -66
    mathieeedoubbas_IEEEDPSub,   // offset = -72
    mathieeedoubbas_IEEEDPMul,   // offset = -78
    mathieeedoubbas_IEEEDPDiv,   // offset = -84
    mathieeedoubbas_IEEEDPFloor, // offset = -90
    mathieeedoubbas_IEEEDPCeil,  // offset = -96
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_mathieeedoubbas_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_mathieeedoubbas_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_mathieeedoubbas_ExLibID[0],
    (ULONG) 0
};
