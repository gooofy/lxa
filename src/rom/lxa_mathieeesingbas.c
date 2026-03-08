/*
 * lxa_mathieeesingbas.c - IEEE Single Precision Basic Math Library
 *
 * This library implements the mathieeesingbas.library for lxa.
 * All single-precision math operations are delegated to the host via EMU_CALL
 * because ROM code cannot use soft-float library functions.
 *
 * Amiga 68k calling convention for IEEE single-precision:
 * - One-operand:  d0 = input float, result in d0
 * - Two-operand:  d0 = left, d1 = right, result in d0
 * - IEEESPFix:    d0 = float, result d0 = LONG
 * - IEEESPFlt:    d0 = LONG, result d0 = float
 * - IEEESPCmp:    d0 = left, d1 = right, result d0 = -1/0/+1
 * - IEEESPTst:    d0 = float, result d0 = -1/0/+1
 *
 * Phase 77: mathieeesingbas.library Implementation
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
#define EXLIBNAME  "mathieeesingbas"
#define EXLIBVER   " 40.1 (2026/03/06)"

char __aligned _g_mathieeesingbas_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_mathieeesingbas_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_mathieeesingbas_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_mathieeesingbas_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* Library initialization */
struct Library * __g_lxa_mathieeesingbas_InitLib (register struct Library *lib    __asm("d0"),
                                                  register BPTR           seglist __asm("a0"),
                                                  register struct ExecBase *sysb  __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_mathieeesingbas: InitLib() called\n");
    return lib;
}

struct Library * __g_lxa_mathieeesingbas_OpenLib (register struct Library *lib __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_mathieeesingbas: OpenLib() called\n");
    lib->lib_OpenCnt++;
    lib->lib_Flags &= ~LIBF_DELEXP;
    return lib;
}

BPTR __g_lxa_mathieeesingbas_CloseLib (register struct Library *lib __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_mathieeesingbas: CloseLib() called\n");
    lib->lib_OpenCnt--;
    return (BPTR)0;
}

BPTR __g_lxa_mathieeesingbas_ExpungeLib (register struct Library *lib __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_mathieeesingbas_ExtFuncLib(void)
{
    return 0;
}

/*
 * IEEESPFix - Convert IEEE single to integer (truncate toward zero)
 *
 * Input:  D0 = IEEE single float
 * Output: D0 = LONG integer
 *
 * Uses EMU_CALL_IEEESP_FIX: d1=float -> d0=LONG
 */
asm(
"_mathieeesingbas_IEEESPFix:                                  \n"
"   move.l  d0, d1              | d1 = float input             \n"
"   move.l  #5100, d0           | EMU_CALL_IEEESP_FIX          \n"
"   illegal                                                    \n"
"   rts                         | d0 = LONG result             \n"
);
extern void mathieeesingbas_IEEESPFix(void);

/*
 * IEEESPFlt - Convert integer to IEEE single float
 *
 * Input:  D0 = LONG integer
 * Output: D0 = IEEE single float
 *
 * Uses EMU_CALL_IEEESP_FLT: d1=LONG -> d0=float
 */
asm(
"_mathieeesingbas_IEEESPFlt:                                  \n"
"   move.l  d0, d1              | d1 = LONG input              \n"
"   move.l  #5101, d0           | EMU_CALL_IEEESP_FLT          \n"
"   illegal                                                    \n"
"   rts                         | d0 = float result            \n"
);
extern void mathieeesingbas_IEEESPFlt(void);

/*
 * IEEESPCmp - Compare two IEEE single floats
 *
 * Input:  D0 = left operand, D1 = right operand
 * Output: D0 = -1 (left < right), 0 (equal), +1 (left > right)
 *         Also sets condition codes: N (negative), Z (zero)
 *
 * Uses EMU_CALL_IEEESP_CMP: d1=left, d2=right -> d0=-1,0,1
 * Note: Must save/restore d2 since it is callee-saved in the m68k ABI.
 */
asm(
"_mathieeesingbas_IEEESPCmp:                                  \n"
"   move.l  d2, -(sp)           | save d2 (callee-saved)       \n"
"   move.l  d1, d2              | d2 = right                   \n"
"   move.l  d0, d1              | d1 = left                    \n"
"   move.l  #5102, d0           | EMU_CALL_IEEESP_CMP          \n"
"   illegal                                                    \n"
"   move.l  (sp)+, d2           | restore d2                   \n"
"   tst.l   d0                  | set condition codes          \n"
"   rts                                                        \n"
);
extern void mathieeesingbas_IEEESPCmp(void);

/*
 * IEEESPTst - Test IEEE single float against zero
 *
 * Input:  D0 = IEEE single float
 * Output: D0 = -1 (negative), 0 (zero), +1 (positive)
 *
 * Uses EMU_CALL_IEEESP_TST: d1=float -> d0=-1,0,1
 */
asm(
"_mathieeesingbas_IEEESPTst:                                  \n"
"   move.l  d0, d1              | d1 = float input             \n"
"   move.l  #5103, d0           | EMU_CALL_IEEESP_TST          \n"
"   illegal                                                    \n"
"   tst.l   d0                  | set condition codes          \n"
"   rts                                                        \n"
);
extern void mathieeesingbas_IEEESPTst(void);

/*
 * IEEESPAbs - Absolute value
 *
 * Input:  D0 = IEEE single float
 * Output: D0 = |input|
 *
 * Uses EMU_CALL_IEEESP_ABS: d1=float -> d0=float
 */
asm(
"_mathieeesingbas_IEEESPAbs:                                  \n"
"   move.l  d0, d1              | d1 = float input             \n"
"   move.l  #5104, d0           | EMU_CALL_IEEESP_ABS          \n"
"   illegal                                                    \n"
"   rts                         | d0 = float result            \n"
);
extern void mathieeesingbas_IEEESPAbs(void);

/*
 * IEEESPNeg - Negate
 *
 * Input:  D0 = IEEE single float
 * Output: D0 = -input
 *
 * Uses EMU_CALL_IEEESP_NEG: d1=float -> d0=float
 */
asm(
"_mathieeesingbas_IEEESPNeg:                                  \n"
"   move.l  d0, d1              | d1 = float input             \n"
"   move.l  #5105, d0           | EMU_CALL_IEEESP_NEG          \n"
"   illegal                                                    \n"
"   rts                         | d0 = float result            \n"
);
extern void mathieeesingbas_IEEESPNeg(void);

/*
 * IEEESPAdd - Addition
 *
 * Input:  D0 = left, D1 = right
 * Output: D0 = left + right
 *
 * Uses EMU_CALL_IEEESP_ADD: d1=left, d2=right -> d0=float
 * Note: Must save/restore d2 since it is callee-saved in the m68k ABI.
 */
asm(
"_mathieeesingbas_IEEESPAdd:                                  \n"
"   move.l  d2, -(sp)           | save d2 (callee-saved)       \n"
"   move.l  d1, d2              | d2 = right                   \n"
"   move.l  d0, d1              | d1 = left                    \n"
"   move.l  #5106, d0           | EMU_CALL_IEEESP_ADD          \n"
"   illegal                                                    \n"
"   move.l  (sp)+, d2           | restore d2                   \n"
"   rts                         | d0 = float result            \n"
);
extern void mathieeesingbas_IEEESPAdd(void);

/*
 * IEEESPSub - Subtraction
 *
 * Input:  D0 = left, D1 = right
 * Output: D0 = left - right
 *
 * Uses EMU_CALL_IEEESP_SUB: d1=left, d2=right -> d0=float
 * Note: Must save/restore d2 since it is callee-saved in the m68k ABI.
 */
asm(
"_mathieeesingbas_IEEESPSub:                                  \n"
"   move.l  d2, -(sp)           | save d2 (callee-saved)       \n"
"   move.l  d1, d2              | d2 = right                   \n"
"   move.l  d0, d1              | d1 = left                    \n"
"   move.l  #5107, d0           | EMU_CALL_IEEESP_SUB          \n"
"   illegal                                                    \n"
"   move.l  (sp)+, d2           | restore d2                   \n"
"   rts                         | d0 = float result            \n"
);
extern void mathieeesingbas_IEEESPSub(void);

/*
 * IEEESPMul - Multiplication
 *
 * Input:  D0 = factor1, D1 = factor2
 * Output: D0 = factor1 * factor2
 *
 * Uses EMU_CALL_IEEESP_MUL: d1=left, d2=right -> d0=float
 * Note: Must save/restore d2 since it is callee-saved in the m68k ABI.
 */
asm(
"_mathieeesingbas_IEEESPMul:                                  \n"
"   move.l  d2, -(sp)           | save d2 (callee-saved)       \n"
"   move.l  d1, d2              | d2 = factor2                 \n"
"   move.l  d0, d1              | d1 = factor1                 \n"
"   move.l  #5108, d0           | EMU_CALL_IEEESP_MUL          \n"
"   illegal                                                    \n"
"   move.l  (sp)+, d2           | restore d2                   \n"
"   rts                         | d0 = float result            \n"
);
extern void mathieeesingbas_IEEESPMul(void);

/*
 * IEEESPDiv - Division
 *
 * Input:  D0 = dividend, D1 = divisor
 * Output: D0 = dividend / divisor
 *
 * Uses EMU_CALL_IEEESP_DIV: d1=dividend, d2=divisor -> d0=float
 * Note: Must save/restore d2 since it is callee-saved in the m68k ABI.
 */
asm(
"_mathieeesingbas_IEEESPDiv:                                  \n"
"   move.l  d2, -(sp)           | save d2 (callee-saved)       \n"
"   move.l  d1, d2              | d2 = divisor                 \n"
"   move.l  d0, d1              | d1 = dividend                \n"
"   move.l  #5109, d0           | EMU_CALL_IEEESP_DIV          \n"
"   illegal                                                    \n"
"   move.l  (sp)+, d2           | restore d2                   \n"
"   rts                         | d0 = float result            \n"
);
extern void mathieeesingbas_IEEESPDiv(void);

/*
 * IEEESPFloor - Largest integer not greater than input
 *
 * Input:  D0 = IEEE single float
 * Output: D0 = floor(input) as IEEE single float
 *
 * Uses EMU_CALL_IEEESP_FLOOR: d1=float -> d0=float
 */
asm(
"_mathieeesingbas_IEEESPFloor:                                \n"
"   move.l  d0, d1              | d1 = float input             \n"
"   move.l  #5110, d0           | EMU_CALL_IEEESP_FLOOR        \n"
"   illegal                                                    \n"
"   rts                         | d0 = float result            \n"
);
extern void mathieeesingbas_IEEESPFloor(void);

/*
 * IEEESPCeil - Smallest integer not less than input
 *
 * Input:  D0 = IEEE single float
 * Output: D0 = ceil(input) as IEEE single float
 *
 * Uses EMU_CALL_IEEESP_CEIL: d1=float -> d0=float
 */
asm(
"_mathieeesingbas_IEEESPCeil:                                 \n"
"   move.l  d0, d1              | d1 = float input             \n"
"   move.l  #5111, d0           | EMU_CALL_IEEESP_CEIL         \n"
"   illegal                                                    \n"
"   rts                         | d0 = float result            \n"
);
extern void mathieeesingbas_IEEESPCeil(void);

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

extern APTR              __g_lxa_mathieeesingbas_FuncTab [];
extern struct MyDataInit __g_lxa_mathieeesingbas_DataTab;
extern struct InitTable  __g_lxa_mathieeesingbas_InitTab;
extern APTR              __g_lxa_mathieeesingbas_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                         // UWORD rt_MatchWord
    &ROMTag,                               // struct Resident *rt_MatchTag
    &__g_lxa_mathieeesingbas_EndResident,  // APTR  rt_EndSkip
    RTF_AUTOINIT,                          // UBYTE rt_Flags
    VERSION,                               // UBYTE rt_Version
    NT_LIBRARY,                            // UBYTE rt_Type
    0,                                     // BYTE  rt_Pri
    &_g_mathieeesingbas_ExLibName[0],      // char  *rt_Name
    &_g_mathieeesingbas_ExLibID[0],        // char  *rt_IdString
    &__g_lxa_mathieeesingbas_InitTab       // APTR  rt_Init
};

APTR __g_lxa_mathieeesingbas_EndResident;
struct Resident *__lxa_mathieeesingbas_ROMTag = &ROMTag;

struct InitTable __g_lxa_mathieeesingbas_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_mathieeesingbas_FuncTab[0],
    (APTR)                &__g_lxa_mathieeesingbas_DataTab,
    (APTR)                __g_lxa_mathieeesingbas_InitLib
};

APTR __g_lxa_mathieeesingbas_FuncTab [] =
{
    __g_lxa_mathieeesingbas_OpenLib,
    __g_lxa_mathieeesingbas_CloseLib,
    __g_lxa_mathieeesingbas_ExpungeLib,
    __g_lxa_mathieeesingbas_ExtFuncLib,
    mathieeesingbas_IEEESPFix,   // offset = -30
    mathieeesingbas_IEEESPFlt,   // offset = -36
    mathieeesingbas_IEEESPCmp,   // offset = -42
    mathieeesingbas_IEEESPTst,   // offset = -48
    mathieeesingbas_IEEESPAbs,   // offset = -54
    mathieeesingbas_IEEESPNeg,   // offset = -60
    mathieeesingbas_IEEESPAdd,   // offset = -66
    mathieeesingbas_IEEESPSub,   // offset = -72
    mathieeesingbas_IEEESPMul,   // offset = -78
    mathieeesingbas_IEEESPDiv,   // offset = -84
    mathieeesingbas_IEEESPFloor, // offset = -90
    mathieeesingbas_IEEESPCeil,  // offset = -96
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_mathieeesingbas_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_mathieeesingbas_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_mathieeesingbas_ExLibID[0],
    (ULONG) 0
};
