#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <libraries/mathffp.h>
#include <inline/mathffp.h>

#include "util.h"
#include "mem_map.h"

#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DPRINTF(lvl, ...) LPRINTF (lvl, __VA_ARGS__)
#else
#define DPRINTF(lvl, ...)
#endif

#if 0
asm(
"    .global ___restore_a4        \n"
"___restore_a4:                   \n"
"    _geta4:     lea ___a4_init,a4\n"
"                rts              \n"
);
#endif

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "mathffp"
#define EXLIBVER   " 40.1 (2022/03/03)"

union FFP_ULONG { ULONG ul; FLOAT f; };

char __aligned _g_mathffp_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_mathffp_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_mathffp_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_mathffp_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

//struct ExecBase        *SysBase         = (struct ExecBase*)    ((UBYTE *)EXEC_BASE_START);
extern struct ExecBase      *SysBase;

// libBase: MathBase
// baseType: struct Library *
// libname: mathffp.library
struct MathBase * __saveds __g_lxa_mathffp_InitLib  ( register struct MathBase *mathffpb    __asm("a6"),
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

FLOAT __saveds _mathffp_SPFlt ( register struct Library * MathBase __asm("a6"),
                                register LONG integer __asm("d0"));

asm(
"__mathffp_SPFlt:                                                                 \n"

"         tst.l   d0                | zero?                                       \n"
"         beq     1f                | by convention FFP 0 == 0                    \n"

"         bmi.b   5f                | negative?                                   \n"
"         moveq   #95, d1           | set positive exponent  95 = 64+31 -> 2^31    \n"
"         bra     2f                | begin conversion                            \n"

"5:       neg.l   d0                | absolute value                              \n"
"         bvc     6f                | check for overflow                          \n"
"         moveq   #-32, d1          | overflow, exponent -32 = -128+64+32 -> 2^32 \n"
"         bra	  3f                | done                                        \n"
"6:       moveq   #-33, d1          | no overflow, exp   -33 = -128+64+31 -> 2^31 \n"

"         /* optimization: skip 16 normalization loop iters for small ints */     \n"
"2:       cmp.l   #0x7fff, d0       | >0x7fff ?                                   \n"
"         bhi     4f                | yes -> no optimization possible             \n"
"         swap    d0                | use swap to shift mantissa 16 bits          \n"
"         sub.b   #0x10, d1         | adjust exponent by 16                       \n"

"         /* normalization loop */                                                \n"
"4:       add.l   d0, d0            | shift mantissa left 1 bit                   \n"
"         dbmi    d1, 4b            | loop until MSB is set                       \n"

"		  /* rounding */                                                          \n"
"         tst.b   d0                | test low byte of mantissa                   \n"
"         bpl.b   3f                | MSB not set -> done                         \n"
"         add.l   #0x100, d0        | round up : mantissa + 1                     \n"
"         bcc.b   3f                | no overflow ? -> done                       \n"
"         roxr.l  #1, d0            | rotate down and insert carry bit            \n"
"         addq.b  #1, d1            | adjust exponent                             \n"

"3:       move.b  d1, d0            | put sign+exponent into d0                   \n"

"1:       rts                       |                                             \n"
);


LONG __saveds mathffp_SPCmp ( register struct Library *MathBase __asm("a6"),
                              register FLOAT           y        __asm("d1"),
                              register FLOAT           x        __asm("d0"));

asm(
"_mathffp_SPCmp:                    |                             \n"
"         tst.b     d0              | x negative?                 \n"
"         bpl.s     1f              | no -> 1:                    \n"
"         tst.b     d1              | y negative?                 \n"
"         bpl.s     1f              | no -> 1:                    \n"
"                                                                 \n"
"         /* both negative -> compare x vs y reversed */          \n"
"         cmp.b     d0, d1          | compare sign+exp            \n"
"         bne.s     2f              | ne ? -> done, compute res   \n"
"         cmp.l     d0, d1          | compare significands        \n"
"         bra.s     2f              | done, compute res           \n"
"                                                                 \n"
"         /* at least one positive -> compare x vs y */           \n"
"1:       cmp.b     d1, d0          | compare sign+exp            \n"
"         bne.s     2f              | ne -> cone, compute res     \n"
"         cmp.l     d1, d0          | compare significands        \n"
"                                                                 \n"
"2:                                                               \n"
"         /* compute result, preserve ccr */                      \n"

"         movem.l   a6, -(sp)       | save a6                     \n"
"         move.l    4, a6           | SysBase -> a6               \n"
"         jsr       -528(a6)        | GetCC()                     \n"
"         move.l    (sp)+, a6       | restore a6                  \n"
"         move.w    d0, d1          | ccr -> d1                   \n"
"         and.w     #0xff, d1       |                             \n"
"                                                                 \n"
"         moveq.l   #0, d0          | clear result                \n"
"         move.w    d1, ccr         | restore CCR                 \n"
"         blt.s     3f              | y > x  -> +1                \n"
"         bgt.s     4f              | y < x  -> -1                \n"
"         bra.s     5f              | y == x -> 0                 \n"
"3:                                                               \n"
"         addq.l    #1, d0			| res := 1                    \n"
"         bra.s     5f                                            \n"
"4:                                                               \n"
"         subq.l    #1, d0          | res := -1                   \n"
"5:                                                               \n"
"         move.w    d1, ccr         | restore ccr                 \n"
"         rts                       |                             \n"
);

LONG __saveds mathffp_SPTst ( register struct Library * MathBase __asm("a6"),
                              register FLOAT parm __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPTst() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds mathffp_SPAbs ( register struct Library * MathBase __asm("a6"),
                               register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPAbs() unimplemented STUB called.\n");
    assert(FALSE);
}


FLOAT __saveds mathffp_SPNeg ( register struct Library * MathBase __asm("a6"),
                               register FLOAT parm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPNeg() unimplemented STUB called.\n");
    assert(FALSE);
}

/* x+y */
FLOAT __saveds mathffp_SPAdd ( register struct Library *MathBase __asm("a6"),
                               register FLOAT           x __asm("d1"),
                               register FLOAT           y __asm("d0"));
asm(
"_mathffp_SPAdd:                                                         \n"
"         movem.l   d3-d5, -(sp)    | save registers                     \n"
"         move.b    d1, d3          | x.Sexp -> d3                       \n"
"         bmi.s     xneg            | x negative ? -> xneg               \n"
"         bgt.s     xpos            | x positive ? -> xpos               \n"
"         /* x == 0 -> return y */                                       \n"
"         tst.b   d0                | set ccr for result                 \n"
"         movem.l (sp)+,d3-d5       | restore registers                  \n"
"         rts                       | done                               \n"
"         /* x positive */                                               \n"
"xpos:                                                                   \n"
"         move.b    d0, d4          | y.Sexp -> d4                       \n"
"         beq.s     1f              | y zero ?                           \n"
"         bgt.s     samesign        | y positive -> samesign             \n"
"         jmp       mixsign         | y negative -> mixed signs          \n"
"1:                                                                      \n"
"         /* y == 0 -> return x */                                       \n"
"         move.l  d1, d0            | x -> result                        \n"
"         tst.b   d0                | set ccr for result                 \n"
"         movem.l (sp)+, d3-d5      | restore registers                  \n"
"         rts                       | done                               \n"
"                                                                        \n"
"         /* -x, check y sign */                                         \n"
"xneg:    move.b  d0, d4            | y.Sexp -> d4                       \n"
"         bmi.s   samesign          | y negative ? -> same sign          \n"
"         bgt.s   mixsign           | y positive ? -> mix sign           \n"
"         /* y == 0 -> return x */                                       \n"
"         move.l  d1, d0            | x -> result                        \n"
"         tst.b   d0                | set ccr for result                 \n"
"         movem.l (sp)+, d3-d5      | restore registers                  \n"
"         rts                       | done                               \n"
"                                                                        \n"
"         /* x y same sign */                                            \n"
"samesign:sub.b     d3, d4          | y.Sexp - x.Sexp                    \n"
"         bmi.s     sxgty           | x>y ?                              \n"
"         move.b    d0, d3          | y.Sexp -> d3                       \n"
"                                   |                                    \n"
"         /* x.exp <= y.exp  */                                          \n"
"         cmp.b     #24, d4         | y too big for x to matter?         \n"
"         bcs.s     2f              | no -> continue                     \n"
"         /* return y */                                                 \n"
"         tst.b   d0                | set ccr for result                 \n"
"         movem.l (sp)+,d3-d5       | restore registers                  \n"
"         rts                       | done                               \n"
"2:                                                                      \n"
"         move.l    d1, d5          | x -> d5                            \n"
"         clr.b     d5              | 0 -> d5.Sexp                       \n"
"         lsr.l     d4, d5          | shift x to match y's Sexp          \n"
"         move.b    #0x80, d0       | trigger carry in y                 \n"
"         add.l     d5, d0          | add mantissas                      \n"
"         bcs.s     snorm           | carry? -> normalize                \n"
"fini:    move.b    d3, d0          | restore Sexp                       \n"
"         movem.l   (sp)+, d3-d5    | restore registers                  \n"
"         rts                       | done                               \n"
"                                                                        \n"
"         /* same sign x.exp > y.exp */                                  \n"
"sxgty:   cmp.b   #-24, d4          | y too small to matter?             \n"
"         bgt.s   3f                | no -> continue                     \n"
"         /* return x */                                                 \n"
"         move.l  d1, d0            | x -> result                        \n"
"         tst.b   d0                | set ccr for result                 \n"
"         movem.l (sp)+, d3-d5      | restore registers                  \n"
"         rts                       | done                               \n"
"3:                                                                      \n"
"         neg.b   d4                | positive exp difference            \n"
"         move.l  d1, d5            | x -> d5                            \n"
"         clr.b   d0                | y.Sexp = 0                         \n"
"         lsr.l   d4, d0            | shift y to match x's Sexp          \n"
"         move.b  #0x80, d5         | trigger carry in x                 \n"
"         add.l   d5, d0            | add mantissas                      \n"
"         bcs.s   snorm             | carry ? -> normalize               \n"
"         move.b  d3, d0            | restore Sexp                       \n"
"         movem.l (sp)+, d3-d5      | restore registers                  \n"
"         rts                       | done                               \n"
"                                                                        \n"
"         /* same sign mantissa overflow normalization */                \n"
"snorm:   roxr.l  #1, d0            | shift right, insert carry bit      \n"
"         addq.b  #1, d3            | Sexp++                             \n"
"         bvs.s   sexpovl           | Sexp overflow?                     \n"
"         bra.s   fini              | finish otherwise                   \n"
"sexpovl: moveq   #-1, d0           | 0xffffffff                         \n"
"         subq.b  #1, d3            | create max pos Sexp in d3          \n"
"         move.b  d3, d0            | inject into result                 \n"
"         ori     #0x02, ccr        | set overflow bit in CCR            \n"
"         movem.l (sp)+, d3-d5      | restore registers                  \n"
"         rts                       | done                               \n"
"                                                                        \n"
"         /* x y mixed signs */                                          \n"
"mixsign: moveq   #-128, d5         | FFP sign mask -> d5                \n"
"         eor.b   d5, d4            | invert y.Sexp sign                 \n"
"         sub.b   d3, d4            | y.Sexp - x.Sexp                    \n"
"         beq.s   mxeqy             | x.Sexp == y.Sexp ?                 \n"
"         bmi.s   mxgty             | x.Sexp > y.Sexp ?                  \n"
"                                                                        \n"
"         /* x.exp <= y.exp */                                           \n"
"         cmp.b   #24, d4           | y too big for x to matter?         \n"
"         bcs.s   4f                | no -> 4f                           \n"
"         /* return y */                                                 \n"
"         tst.b   d0                | set ccr for result                 \n"
"         movem.l (sp)+,d3-d5       | restore registers                  \n"
"         rts                       | done                               \n"
"4:                                                                      \n"
"         move.b  d0, d3            | y.Sexp -> d3                       \n"
"         move.b  d5, d0            | trigger carry                      \n"
"         move.l  d1, d5            | x -> d5                            \n"
"mxadd:   clr.b   d5                | x.Sexp = 0                         \n"
"         lsr.l   d4, d5            | shift x to match y's Sexp          \n"
"         sub.l   d5, d0            | subtract mantissas                 \n"
"         bmi.s   fini              | no overflow ? -> finish            \n"
"                                                                        \n"
"         /* mixed signs mantissa overflow normalization */              \n"
"mnorm1:  move.b  d3, d4            | y.Sexp -> d3                       \n"
"mnorm2:  clr.b   d0                | res.Sexp -> 0                      \n"
"         subq.b  #1, d3            | adjust Sexp                        \n"
"nshift:  add.l   d0, d0            | res <<= 1                          \n"
"         dbmi    d3, nshift        | loop                               \n"
"         eor.b   d3, d4            | test sign                          \n"
"         bge.s   6f                | no underflow ? -> continue         \n"
"         /* return 0 */                                                 \n"
"         moveq   #0, d0            | 0 -> d0                            \n"
"         movem.l (sp)+,d3-d5       | restore registers                  \n"
"         rts                       | done                               \n"
"6:                                                                      \n"
"         move.b  d3, d0            | restore Sexp                       \n"
"         bne.s   7f                | no exp underflow ? -> continue     \n"
"         /* return 0 */                                                 \n"
"         moveq   #0, d0            | 0 -> d0                            \n"
"         movem.l (sp)+,d3-d5       | restore registers                  \n"
"         rts                       | done                               \n"
"7:                                                                      \n"
"         movem.l (sp)+,d3-d5       | restore registers                  \n"
"         rts                       | done                               \n"
"                                                                        \n"
"         /* mixed sign x.exp > y.exp */                                 \n"
"mxgty:   cmp.b   #-24, d4          | y too small to matter?             \n"
"         bgt     5f                | no -> 5f                           \n"
"         /* return x */                                                 \n"
"         move.l  d1, d0            | x -> result                        \n"
"         tst.b   d0                | set ccr for result                 \n"
"         movem.l (sp)+, d3-d5      | restore registers                  \n"
"         rts                       | done                               \n"
"5:                                                                      \n"
"         neg.b   d4                | positive exp difference            \n"
"         move.l  d0, d5            | y -> d5                            \n"
"         move.l  d1, d0            | x -> d0                            \n"
"         move.b  #0x80, d0         | trigger carry (rounding)           \n"
"         bra.s   mxadd             | -> mixed sign mantissa addition    \n"
"                                                                        \n"
"         /* mixed sign x.exp == y.exp */                                \n"
"mxeqy:   move.b  d0, d4            | save y.Sexp -> d4                  \n"
"         exg     d4, d3            | exchange y.Sexp with x.Sexp        \n"
"         move.b  d1, d0            | x.Sexp -> d0                       \n"
"         sub.l   d1, d0            | subtract mantissas                 \n"
"         bne.s   8f                | not equal ? -> continue            \n"
"         /* return 0 */                                                 \n"
"         moveq   #0, d0            | 0 -> d0                            \n"
"         movem.l (sp)+,d3-d5       | restore registers                  \n"
"         rts                       | done                               \n"
"8:                                                                      \n"
"         bpl.s   mnorm1            | positive? -> normalize             \n"
"         neg.l   d0                | invert sign                        \n"
"         move.b  d4, d3            | restore y.Sexp->d3                 \n"
"         bra.s   mnorm2            | -> normalize                       \n"
);

static FLOAT __saveds _mathffp_SPSub ( register struct Library *MathBase __asm("a6"),
                                       register FLOAT           y        __asm("d1"),
                                       register FLOAT           x        __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_mathffp: SPSub() called.\n");

    // compute z = x - y

    union FFP_ULONG ufx = {.f = x};
    union FFP_ULONG ufy = {.f = y};

    ufy.ul ^= 0x00000080; // invert sign

    return SPAdd(ufy.f, ufx.f);
}

static FLOAT __saveds _mathffp_SPMul ( register struct Library * MathBase __asm("a6"),
                                                        register FLOAT leftParm __asm("d1"),
                                                        register FLOAT rightParm __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_mathffp: SPMul() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds mathffp_SPDiv ( register struct Library * MathBase  __asm("a6"),
                               register FLOAT            fdivisor  __asm("d1"),
                               register FLOAT            fdividend __asm("d0"));


/* register usage:
 *      d0 : dividend, x / result
 *      d1 : divisor,  y
 *      d2 : bitmask
 *      d3 : sigY
 *      d4 : sigX
 *      d5 : expZ
 *      d6 : scratch
 *      d7 : sigZ
 */

asm(
"_mathffp_SPDiv:                                                  \n"
"       movem.l     d2-d7,-(sp)          | save registers         \n"
"                                                                 \n"
"       /* handle division by zero */                             \n"
"       tst.l       d1                   | divisor zero ?         \n"
"       bne         11f                  | not zero -> continue   \n"
"       divu.w      #0, d0               | trigger exception      \n"
"       tst.l       d1                   | is d1 fixed now?       \n"
"       bne         11f                  | yes -> continue        \n"
"       or.l        #0xffffff7f, d0      | min/max ffp            \n"
"       tst.b       d0                   | set cond codes         \n"
"       ori         #0x02, ccr           | set overflow cc        \n"
"       movem.l     (sp)+, d2-d7         | restore registers      \n"
"       rts                              |                        \n"
"                                                                 \n"
"       /* compute exponent expZ = expX - expY + 65 */            \n"
"11:                                                              \n"
"       move.b      d0, d5               | dividend -> d5         \n"
"       beq         10f                  | zero ? -> done         \n"
"     	andi.l      #0x7f, d5            | mask exponent          \n"
"       move.b      d1, d2               | divisor -> d2          \n"
"       andi.l      #0x7f, d2            | mask exponent          \n"
"       sub.b       d2, d5               | subtract exponents     \n"
"     	addi.b      #65, d5              | add bias               \n"
"                                                                 \n"
"       /* extract significands */                                \n"
"       move.l      #0xffffff00, d4      | significand mask -> d4 \n"
"      	and.l       d0, d4               | sigX -> d4             \n"
"       move.l      #0xffffff00, d2      | significand mask -> d2 \n"
"       move.l      d1, d3               | divisor -> d3          \n"
"       and.l       d2, d3               | sigY -> d3             \n"
"                                                                 \n"
"       moveq       #0, d7               | sigZ = 0               \n"
"                                                                 \n"
"       /* division loop */                                       \n"
"                                                                 \n"
"       move.l      #0x80000000, d2      | bitmask -> d2          \n"
"1:                                                               \n"
"       tst.l       d4                   | sigX == 0?             \n"
"       beq.s       5f                   | -> div loop done       \n"
"                                                                 \n"
"       moveq       #0x40, d6            | bitmask < 0x40 ?       \n"
"       cmp.l       d2, d6               |                        \n"
"       bcc.s       5f                   | -> div loop done       \n"
"                                                                 \n"
"       move.l      d4, d6               |                        \n"
"       sub.l       d3, d6               | sigX - sigY -> d6      \n"
"       bmi         4f                   | < 0 ? -> 4f            \n"
"                                                                 \n"
"       move.l      d6, d4               | sigX -= sigY           \n"
"       or.l        d2, d7               | sigZ |= bitmask        \n"
"                                                                 \n"
"2:                                                               \n"
"       tst.l       d4                   | sigX <= 0 ?            \n"
"       ble         3f                   | yes -> 3f              \n"
"       add.l       d4, d4               | sigX <<= 1             \n"
"       lsr.l       #1, d2               | bitmask >>= 1          \n"
"       bra.s       2b                   | -> inner loop          \n"
"                                                                 \n"
"3:                                                               \n"
"       tst.l       d3                   | sigY <= 0 ?            \n"
"       ble         1b                   | yes -> div loop        \n"
"       add.l       d3, d3               | sigY <<= 1             \n"
"       add.l       d2, d2               | bitmask <<= 1          \n"
"       bra.s       3b                   | -> inner loop          \n"
"                                                                 \n"
"4:                                                               \n"
"       lsr.l       #1, d3               | sigY >>= 1             \n"
"       lsr.l       #1, d2               | bm >>= 1               \n"
"       bra.s       1b                   | -> div loop            \n"
"                                                                 \n"
"       /* normalization loop */                                  \n"
"5:                                                               \n"
"       move.l      #0x40000000, d6                               \n"
"6:                                                               \n"
"       tst.l       d7                   | sigZ > 0 ?             \n"
"       ble         8f                   | no -> done             \n"
"       cmp.l       d7, d6               | sigZ >= 0x40000000 ?   \n"
"       blt.s       7f                   | no -> 6:               \n"
"       subi.l      #0x80000000, d7      | sigZ -= 0x80000000     \n"
"7:                                                               \n"
"       add.l       d7, d7               | sigZ <<= 1             \n"
"       subq.b      #1, d5               | dZ--                   \n"
"       bra.s       6b                   | norm loop              \n"
"                                                                 \n"
"       /* construct result */                                    \n"
"8:                                                               \n"
"       tst.b       d7                   | (byte) sigZ < 0 ?      \n"
"       bge         9f                   | no                     \n"
"       addi.l      #0x100, d7           | sigZ += 256            \n"
"9:                                                               \n"
"       move.l      #0xffffff00, d6      | significand mask       \n"
"       and.l       d6, d7               | sigZ &= sig mask       \n"
"                                                                 \n"
"       /* compute sign */                                        \n"
"       eor.l       d1, d0               | signZ = signX ^ signY  \n"
"       move.l      #0x80, d6            | mask out sign bit      \n"
"       and.l       d6, d0               | -> res                 \n"

"       or.l        d5, d0               | res |= expZ            \n"
"       or.l        d7, d0               | res |= sigZ            \n"
"                                                                 \n"
"10:                                                              \n"
"       movem.l     (sp)+, d2-d7         | restore registers      \n"
"       rts                              |                        \n"
);

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
    mathffp_SPCmp,  // offset = -42
    mathffp_SPTst, // offset = -48
    mathffp_SPAbs, // offset = -54
    mathffp_SPNeg, // offset = -60
    mathffp_SPAdd, // offset = -66
    _mathffp_SPSub, // offset = -72
    _mathffp_SPMul, // offset = -78
    mathffp_SPDiv,  // offset = -84
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

