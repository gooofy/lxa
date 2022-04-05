/*

 * based (in part) on
 * CORDIC Approximation of Elementary Functions
 * The computer code and data files described and made available on this web page are distributed under the GNU LGPL license.
 * https://people.math.sc.edu/Burkardt/c_src/cordic/cordic.html
 */


#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <clib/mathffp_protos.h>
#include <inline/mathffp.h>

#include <clib/mathtrans_protos.h>
#include <inline/mathtrans.h>

#include "util.h"
#include "mem_map.h"

union FFP_ULONG { FLOAT f; ULONG ul; } ;

#define CCR_ZERO 0x00000004
#define CCR_NEG  0x00000008
#define CCR_OVF  0x00000002

static const union FFP_ULONG fuZero       = { .ul = 0x00000000 /* 0.000000 */ };
static const union FFP_ULONG fuE          = { .ul = 0xadf85442 /* 2.718282 */ };
static const union FFP_ULONG fuOne        = { .ul = 0x80000041 /* 1.000000 */ };
static const union FFP_ULONG fuNOne       = { .ul = 0x800000c1 /* -1.000000 */ };
static const union FFP_ULONG fuOneHalf    = { .ul = 0x80000040 /* 0.500000 */ };
static const union FFP_ULONG fuTwo        = { .ul = 0x80000042 /* 2.000000 */ };
static const union FFP_ULONG fuThree      = { .ul = 0xc0000042 /* 3.000000 */ };
static const union FFP_ULONG fuFour       = { .ul = 0x80000043 /* 4.000000 */ };
static const union FFP_ULONG fuNInfinity  = { .ul = 0xffffffff /* - Infty  */ };
static const union FFP_ULONG fuPi         = { .ul = 0xc90fdb42 /*  3.141593 */ };
static const union FFP_ULONG fuNPi        = { .ul = 0xc90fdbc2 /* -3.141593 */ };
static const union FFP_ULONG fu2Pi        = { .ul = 0xc90fdc43 /*  6.283186 */ };
static const union FFP_ULONG fuN2Pi       = { .ul = 0xc90fdcc3 /* -6.283186 */ };
static const union FFP_ULONG fuHalfPi     = { .ul = 0xc90fdb41 /*  1.570796 */ };
static const union FFP_ULONG fuNHalfPi    = { .ul = 0xc90fdbc1 /* -1.570796 */ };
static const union FFP_ULONG fuKProd      = { .ul = 0x9b74ee40 /*  0.607253 */ };

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
extern struct Library       *MathBase;

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

FLOAT __saveds _mathtrans_SPAtan ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPAtan() unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds cossin_cordic_ffp ( FLOAT beta, FLOAT *c, FLOAT *s )
{
# define ANGLES_LENGTH 15
# define KPROD_LENGTH 15

    static const ULONG aul[ANGLES_LENGTH] = {
      0xc90fdb40 /* 0.785398 */,
      0xed63383f /* 0.463648 */,
      0xfadbb03e /* 0.244979 */,
      0xfeadd53d /* 0.124355 */,
      0xffaade3c /* 0.062419 */,
      0xffeaae3b /* 0.031240 */,
      0xfffaab3a /* 0.015624 */,
      0xfffeab39 /* 0.007812 */,
      0xffffab38 /* 0.003906 */,
      0xffffeb37 /* 0.001953 */,
      0xfffffb36 /* 0.000977 */,
      0xffffff35 /* 0.000488 */,
      0x80000035 /* 0.000244 */,
      0x80000034 /* 0.000122 */,
      0x80000033 /* 0.000061 */
      };
    static const FLOAT *angles = (FLOAT*) aul;

    FLOAT theta;

    /*
      Shift angle to interval [-pi,pi]
    */

    theta = beta;
    while (SPCmp (theta, fuNPi.f)<0)
        theta = SPAdd (fu2Pi.f, theta);
    while (SPCmp (theta, fuPi.f)>0)
        theta = SPSub (fu2Pi.f, theta);

    /*
      Shift angle to interval [-pi/2,pi/2] and account for signs
    */

    FLOAT sign_factor;
    if (SPCmp(theta, fuNHalfPi.f)<0)
    {
        theta = SPAdd(fuPi.f, theta);
        sign_factor = fuNOne.f;
    }
    else if ( SPCmp(fuHalfPi.f, theta)<0 )
    {
        theta = SPSub (fuPi.f, theta);
        sign_factor = fuNOne.f;
    }
    else
    {
        sign_factor = fuOne.f;
    }
    //printf ("cossin_cordic_ffp: theta=%f, sign_factor=%f\n", decode_ffp(theta), decode_ffp(sign_factor));

    /*
      Initialize loop variables:
    */
    *c = fuOne.f;
    *s = fuZero.f;

    FLOAT poweroftwo = fuOne.f;
    FLOAT angle = angles[0];

    /*
      Iterations
    */

    for (int j=1; j<=15; j++ )
    {
        FLOAT sigma;
        if ( SPCmp(theta, fuZero.f)<0)
            sigma = fuNOne.f;
        else
            sigma = fuOne.f;

        FLOAT factor = SPMul(sigma, poweroftwo);

        FLOAT c2 = SPSub(SPMul(factor, *s), *c);
        FLOAT s2 = SPAdd(SPMul(factor, *c), *s);

        *c = c2;
        *s = s2;

        /*
          Update the remaining angle.
        */

        theta = SPSub(SPMul(sigma, angle), theta);

        poweroftwo = SPDiv (fuTwo.f, poweroftwo);

        /*
          Update the angle from table, or eventually by just dividing by two.
        */
        if ( ANGLES_LENGTH < j + 1 )
        {
          angle = SPDiv (fuTwo.f, angle);
        }
        else
        {
          angle = angles[j];
        }
    }

    /*
      Adjust length of output vector to be [cos(beta), sin(beta)]

      KPROD is essentially constant after a certain point, so if N is
      large, just take the last available value.
    */

    *c = SPMul(*c, fuKProd.f);
    *s = SPMul(*s, fuKProd.f);

    /*
      Adjust for possible sign change because angle was originally
      not in quadrant 1 or 4.
    */
    *c = SPMul(sign_factor, *c);
    *s = SPMul(sign_factor, *s);
# undef ANGLES_LENGTH
}

FLOAT __saveds _mathtrans_SPSin ( register struct Library *MathTransBase __asm("a6"),
                                  register FLOAT           beta          __asm("d0"))
{
    LPRINTF (LOG_DEBUG, "_mathtrans: SPSin() called.\n");
    FLOAT c, s;
    cossin_cordic_ffp ( beta, &c, &s );
    return s;
}

FLOAT __saveds _mathtrans_SPCos ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPCos() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds _mathtrans_SPTan ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPTan() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds _mathtrans_SPSincos ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT * cosResult __asm("d1"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPSincos() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds _mathtrans_SPSinh ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPSinh() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds _mathtrans_SPCosh ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPCosh() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds _mathtrans_SPTanh ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPTanh() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds mathtrans_SPExp ( register struct Library *MathTransBase __asm("a6"),
                                 register FLOAT           x             __asm("d0"))
{
    static const ULONG aul[5] = {
        0xd3094c41 /* 1.648721 */,
        0xa45af241 /* 1.284025 */,
        0x910b0241 /* 1.133148 */,
        0x88415b41 /* 1.064494 */,
        0x84102b41 /* 1.031743 */,
    };
    static const FLOAT *a = (FLOAT *) aul;

    int x_int = (int) SPFix(SPFloor (x));

    /*
     * Determine the weights.
     */
    FLOAT poweroftwo = fuOneHalf.f;
    FLOAT z = SPSub ( SPFlt(x_int), x);

    // printf ("ffp  cordic x_int=%d, poweroftwo=%f, z=%f\n", x_int, decode_ffp(poweroftwo), decode_ffp(z));

    FLOAT w[5];

    for (int i=0; i<5; i++ )
    {
        w[i] = fuZero.f;
        if (SPCmp(poweroftwo,z) < 0)
        {
            w[i] = fuOne.f;
            z    = SPSub(poweroftwo, z);
        }
        poweroftwo = SPDiv (fuTwo.f, poweroftwo);
    }

    /*
     * Calculate products.
     */
    FLOAT fx = fuOne.f;
    // printf ("ffp  cordic fx [0]: %f\n", decode_ffp(fx));

    for (int i=0; i<5; i++)
    {
        FLOAT ai;
        if (i<5)
        {
            ai = a[i];
        }
        else
        {
            ai = SPAdd (fuOne.f, SPDiv(fuTwo.f, SPSub(fuOne.f, ai)));
        }

        // printf ("ffp  cordic fx [i=%d]: %f ai=%f w[i]=%f\n", i, decode_ffp(fx), decode_ffp(ai), decode_ffp(w[i]));

        if (SPCmp(fuZero.f, w[i]) < 0 )
        {
            fx = SPMul(fx, ai);
        }
    }

    /*
     * Perform residual multiplication.
     */
    // printf ("ffp  cordic fx [1]: %f\n", decode_ffp(fx));
    fx = SPMul(fx, SPAdd(fuOne.f, SPMul(z, SPAdd( fuOne.f, SPDiv (SPMul(fuTwo.f, SPAdd(fuOne.f, SPDiv(SPMul(fuThree.f, SPAdd(fuOne.f, SPDiv(fuFour.f, z))), z))), z)))));
    // printf ("ffp  cordic fx [2]: %f\n", decode_ffp(fx));

    /*
      Account for factor EXP(X_INT).
    */
    if (x_int<0)
    {
        for (int i = 1; i <= -x_int; i++ )
        {
            fx = SPDiv (fuE.f, fx);
        }
    }
    else
    {
        for (int i = 1; i <= x_int; i++ )
        {
            fx = SPMul (fuE.f, fx);
        }
    }

    return fx;
}

FLOAT __saveds _mathtrans_SPLog ( register struct Library *MathTransBase __asm("a6"),
                                         register FLOAT           x             __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_mathtrans: SPLog() called.\n");

    static const ULONG aul[5] = {
        0xd3094c41 /* 1.648721 */,
        0xa45af241 /* 1.284025 */,
        0x910b0241 /* 1.133148 */,
        0x88415b41 /* 1.064494 */,
        0x84102b41 /* 1.031743 */,
    };
    static const FLOAT *a = (FLOAT *) aul;

    int c = SPCmp (x, fuZero.f);

    if ( c < 0 )
    {
        // FIXME: does this really work?
        SetSR (CCR_OVF, CCR_ZERO | CCR_NEG | CCR_OVF);
        return fuZero.f;
    }

    if (c == 0)
        return fuNInfinity.f;

    int k = 0;

    while ( SPCmp (fuE.f, x) <= 0 )
    {
        k = k + 1;
        x = SPDiv(fuE.f, x);
    }

    while ( SPCmp(x, fuOne.f) < 0 )
    {
        k = k - 1;
        x = SPMul(x, fuE.f);
    }

    /*
     * Determine the weights.
     */
    FLOAT w[5];

    for (int i = 0; i < 5; i++ )
    {
        w[i] = fuZero.f;

        FLOAT ai = a[i];

        if ( SPCmp(ai, x) < 0 )
        {
            w[i] = fuOne.f;
            x = SPDiv (ai, x);
        }
    }

    x = SPSub(fuOne.f, x);

    x = SPMul(x, SPSub(SPMul(SPDiv(fuTwo.f,x), SPAdd(fuOne.f, SPMul(SPDiv(fuThree.f,x), SPSub(SPDiv(fuFour.f,x),fuOne.f)))),fuOne.f));

    /*
     * Assemble
     */
    FLOAT poweroftwo = fuOneHalf.f;

    for ( int i = 0; i < 5; i++ )
    {
        x = SPAdd(x, SPMul(w[i], poweroftwo));
        poweroftwo = SPDiv (fuTwo.f, poweroftwo);
    }

    return SPAdd(x, SPFlt(k));
}

FLOAT __saveds _mathtrans_SPPow ( register struct Library *MathTransBase __asm("a6"),
                                         register FLOAT           power         __asm("d1"),
                                         register FLOAT           base          __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_mathtrans: SPPow() called.\n");

	union FFP_ULONG ufbase;
	ufbase.f = base;

	ufbase.ul &= 0xffffff7f; // mask out sign bit

    // base^power = e^(power * ln base)

    FLOAT res;
    res = SPLog (ufbase.f );
    res = SPMul (res, power);
    res = SPExp (res);

    return res;
}

FLOAT __saveds _mathtrans_SPSqrt ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPSqrt() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds _mathtrans_SPTieee ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPTieee() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds _mathtrans_SPFieee ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPFieee() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds _mathtrans_SPAsin ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPAsin() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds _mathtrans_SPAcos ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPAcos() unimplemented STUB called.\n");
    assert(FALSE);
}

FLOAT __saveds _mathtrans_SPLog10 ( register struct Library * MathTransBase __asm("a6"),
                                                        register FLOAT parm __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_mathtrans: SPLog10() unimplemented STUB called.\n");
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
    mathtrans_SPExp,  // offset = -78
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
