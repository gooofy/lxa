/*
 * Probe (Phase 237): FPU instructions as compilers emit them.
 *
 * AmigaOS 3.1 reports the FPU in AttnFlags (AFF_68881/AFF_68882) and
 * programs built for it (SAS/C math=68881, Fish MoonTool .030/.040,
 * SManCP, Offender) use every addressing mode: fmove.d (xxx).L, fmovem.x
 * -(An)/(An)+, the (d8,An,Xn*s) forms, FDBcc, FScc and the transcendental
 * functions (68040: through 68040.library).  The instructions are encoded
 * as data because the probes are compiled for a plain 68000.
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include "probe.h"

extern struct ExecBase *SysBase;

double dsrc = 2.5, ddst;
static struct {
    double d25, d05, dbig, out1, out2;
    UBYTE mbuf[36];
} blk = { 2.5, 0.5, 10000000000.75, 0, 0 };
static LONG res[32];

void fputest(LONG *r, void *data);
__asm__(
    "\t.text\n"
    "\t.even\n"
    "\t.globl _fputest\n"
    "_fputest:\n\t"
    "move.l 4(sp),a0\n\t"
    "move.l 8(sp),a1\n\t"
    ".short 0x48e7,0x2020\n\t"   /* movem.l d2/a2,-(sp) */
    ".short 0xf239,0x5400\n\t.long _dsrc\n\t"   /* fmoved _dsrc,fp0 */
    ".short 0xf239,0x7400\n\t.long _ddst\n\t"   /* fmoved fp0,_ddst */
    ".short 0xf23c,0x4080,0x0000,0x0007\n\t"   /* fmove.l #7,fp1 */
    ".short 0xf211,0x54a3\n\t"   /* fmuld (a1),fp1 */
    ".short 0xf210,0x6080\n\t"   /* fmove.l fp1,(a0) */
    ".short 0xf23c,0x4080,0x0000,0x0002\n\t"   /* fmove.l #2,fp1 */
    ".short 0xf23c,0x4100,0x0000,0x0003\n\t"   /* fmove.l #3,fp2 */
    ".short 0xf23c,0x4280,0x0000,0x0006\n\t"   /* fmove.l #6,fp5 */
    ".short 0x45e9,0x004c\n\t"   /* lea 76(a1),a2 */
    ".short 0xf222,0xe026\n\t"   /* fmovemx fp1-fp2/fp5,-(a2) */
    ".short 0xf21a,0xd01a\n\t"   /* fmovemx (a2)+,fp3-fp4/fp6 */
    ".short 0xf228,0x6180,0x0004\n\t"   /* fmove.l fp3,4(a0) */
    ".short 0xf228,0x6200,0x0008\n\t"   /* fmove.l fp4,8(a0) */
    ".short 0xf228,0x6300,0x000c\n\t"   /* fmove.l fp6,12(a0) */
    ".short 0xf229,0x5400,0x0008\n\t"   /* fmoved 8(a1),fp0 */
    ".short 0xf200,0x000e\n\t"   /* fsinx fp0,fp0 */
    ".short 0xf23c,0x4023,0x000f,0x4240\n\t"   /* fmul.l #1000000,fp0 */
    ".short 0xf228,0x6000,0x0010\n\t"   /* fmove.l fp0,16(a0) */
    ".short 0xf23c,0x4000,0x0000,0x0001\n\t"   /* fmove.l #1,fp0 */
    ".short 0xf200,0x0010\n\t"   /* fetoxx fp0,fp0 */
    ".short 0xf23c,0x4023,0x000f,0x4240\n\t"   /* fmul.l #1000000,fp0 */
    ".short 0xf228,0x6000,0x0014\n\t"   /* fmove.l fp0,20(a0) */
    ".short 0xf23c,0x4000,0x0000,0x000a\n\t"   /* fmove.l #10,fp0 */
    ".short 0xf200,0x0014\n\t"   /* flognx fp0,fp0 */
    ".short 0xf23c,0x4023,0x000f,0x4240\n\t"   /* fmul.l #1000000,fp0 */
    ".short 0xf228,0x6000,0x0018\n\t"   /* fmove.l fp0,24(a0) */
    ".short 0xf23c,0x4000,0x0000,0x0001\n\t"   /* fmove.l #1,fp0 */
    ".short 0xf200,0x000a\n\t"   /* fatanx fp0,fp0 */
    ".short 0xf23c,0x4023,0x000f,0x4240\n\t"   /* fmul.l #1000000,fp0 */
    ".short 0xf228,0x6000,0x001c\n\t"   /* fmove.l fp0,28(a0) */
    ".short 0xf229,0x5400,0x0008\n\t"   /* fmoved 8(a1),fp0 */
    ".short 0xf200,0x001d\n\t"   /* fcosx fp0,fp0 */
    ".short 0xf23c,0x4023,0x000f,0x4240\n\t"   /* fmul.l #1000000,fp0 */
    ".short 0xf228,0x6000,0x0020\n\t"   /* fmove.l fp0,32(a0) */
    ".short 0xf229,0x5400,0x0008\n\t"   /* fmoved 8(a1),fp0 */
    ".short 0xf200,0x000f\n\t"   /* ftanx fp0,fp0 */
    ".short 0xf23c,0x4023,0x000f,0x4240\n\t"   /* fmul.l #1000000,fp0 */
    ".short 0xf228,0x6000,0x0024\n\t"   /* fmove.l fp0,36(a0) */
    ".short 0xf23c,0x4000,0x0000,0x0002\n\t"   /* fmove.l #2,fp0 */
    ".short 0xf200,0x0004\n\t"   /* fsqrtx fp0,fp0 */
    ".short 0xf23c,0x4023,0x000f,0x4240\n\t"   /* fmul.l #1000000,fp0 */
    ".short 0xf228,0x6000,0x0028\n\t"   /* fmove.l fp0,40(a0) */
    ".short 0xf229,0x5400,0x0010\n\t"   /* fmoved 16(a1),fp0 */
    ".short 0xf200,0x0003\n\t"   /* fintrzx fp0,fp0 */
    ".short 0xf229,0x7400,0x0018\n\t"   /* fmoved fp0,24(a1) */
    ".short 0xf229,0x5400,0x0010\n\t"   /* fmoved 16(a1),fp0 */
    ".short 0xf200,0x0001\n\t"   /* fintx fp0,fp0 */
    ".short 0xf229,0x7400,0x0020\n\t"   /* fmoved fp0,32(a1) */
    ".short 0xf23c,0x4000,0xffff,0xfff4\n\t"   /* fmove.l #-12,fp0 */
    ".short 0xf200,0x009e\n\t"   /* fgetexpx fp0,fp1 */
    ".short 0xf228,0x6080,0x002c\n\t"   /* fmove.l fp1,44(a0) */
    ".short 0xf200,0x011f\n\t"   /* fgetmanx fp0,fp2 */
    ".short 0xf23c,0x4123,0x0000,0x03e8\n\t"   /* fmul.l #1000,fp2 */
    ".short 0xf228,0x6100,0x0030\n\t"   /* fmove.l fp2,48(a0) */
    ".short 0xf200,0x5c35\n\t"   /* fmovecrx #53,fp0 */
    ".short 0xf228,0x6000,0x0034\n\t"   /* fmove.l fp0,52(a0) */
    ".short 0xf200,0x5c33\n\t"   /* fmovecrx #51,fp0 */
    ".short 0xf228,0x6000,0x0038\n\t"   /* fmove.l fp0,56(a0) */
    ".short 0xf23c,0x9000,0x0000,0x0010\n\t"   /* fmove.l #16,fpcr */
    ".short 0xf228,0xb400,0x003c\n\t"   /* fmovem.l fpiar/fpcr,60(a0) */
    ".short 0xf23c,0x9000,0x0000,0x0000\n\t"   /* fmove.l #0,fpcr */
    ".short 0x7203\n\t"   /* moveq #3,d1 */
    ".short 0x7000\n\t"   /* moveq #0,d0 */
    ".short 0x5280\n\t"   /* addq.l #1,d0 */
    ".short 0xf249,0x0000,0xfffa\n\t"   /* fdbf d1,17c 17c _fputest+0x17c */
    ".short 0x2140,0x0044\n\t"   /* move.l d0,68(a0) */
    ".short 0xf23c,0x4000,0x0000,0x0001\n\t"   /* fmove.l #1,fp0 */
    ".short 0xf23c,0x4038,0x0000,0x0002\n\t"   /* fcmp.l #2,fp0 */
    ".short 0xf268,0x0014,0x0048\n\t"   /* fslt 72(a0) */
    ".short 0xf268,0x0012,0x0049\n\t"   /* fsgt 73(a0) */
    ".short 0xf23c,0x4000,0x0000,0x0007\n\t"   /* fmove.l #7,fp0 */
    ".short 0xf23c,0x4021,0x0000,0x0003\n\t"   /* fmod.l #3,fp0 */
    ".short 0xf228,0x6000,0x004c\n\t"   /* fmove.l fp0,76(a0) */
    ".short 0xf229,0x5400,0x0008\n\t"   /* fmoved 8(a1),fp0 */
    ".short 0xf200,0x0131\n\t"   /* fsincosx fp0,fp1,fp2 */
    ".short 0xf23c,0x40a3,0x000f,0x4240\n\t"   /* fmul.l #1000000,fp1 */
    ".short 0xf23c,0x4123,0x000f,0x4240\n\t"   /* fmul.l #1000000,fp2 */
    ".short 0xf228,0x6080,0x0050\n\t"   /* fmove.l fp1,80(a0) */
    ".short 0xf228,0x6100,0x0054\n\t"   /* fmove.l fp2,84(a0) */
    ".short 0xf23c,0x4000,0x0000,0x0003\n\t"   /* fmove.l #3,fp0 */
    ".short 0xf23c,0x4026,0x0000,0x0002\n\t"   /* fscale.l #2,fp0 */
    ".short 0xf228,0x6000,0x0058\n\t"   /* fmove.l fp0,88(a0) */
    ".short 0x740c\n\t"   /* moveq #12,d2 */
    ".short 0xf23c,0x4000,0x0000,0x0005\n\t"   /* fmove.l #5,fp0 */
    ".short 0xf230,0x6000,0x2efc\n\t"   /* fmove.l fp0,(-4,a0,d2.l*8) */
    ".short 0x4cdf,0x0404\n\t"   /* movem.l (sp)+,d2/a2 */
    ".short 0x4e75\n\t"   /* rts */
);

static void show(const char *label, int i)
{
    P_LONG(label, res[i]);
}

int main(void)
{
    UWORD attn = SysBase->AttnFlags;
    const ULONG *w;

    P_SECTION("AttnFlags");
    P_BOOL("AFF_68881", attn & AFF_68881);
    P_BOOL("AFF_68882", attn & AFF_68882);
    if (!(attn & AFF_68881))
        return 0;

    fputest(res, &blk);

    P_SECTION("addressing modes");
    w = (const ULONG *)&ddst;
    P_HEX("fmove.d (xxx).L -> (xxx).L hi", w[0]);
    P_HEX("fmove.d (xxx).L -> (xxx).L lo", w[1]);
    show("fmove.l of 7 * 2.5", 0);
    P_BYTES("fmovem.x fp1/fp2/fp5,-(a2) slot 0", blk.mbuf + 0, 4);
    P_BYTES("fmovem.x fp1/fp2/fp5,-(a2) slot 1", blk.mbuf + 12, 4);
    P_BYTES("fmovem.x fp1/fp2/fp5,-(a2) slot 2", blk.mbuf + 24, 4);
    show("fmovem.x (a2)+ -> fp3", 1);
    show("fmovem.x (a2)+ -> fp4", 2);
    show("fmovem.x (a2)+ -> fp6", 3);
    show("fmove.l fp0,(-4,a0,d2.l*8)", 23);

    P_SECTION("functions (x 1000000)");
    show("fsin(0.5)", 4);
    show("fetox(1)", 5);
    show("flogn(10)", 6);
    show("fatan(1)", 7);
    show("fcos(0.5)", 8);
    show("ftan(0.5)", 9);
    show("fsqrt(2)", 10);
    show("fsincos(0.5) cos", 20);
    show("fsincos(0.5) sin", 21);

    P_SECTION("other operations");
    w = (const ULONG *)&blk.out1;
    P_HEX("fintrz(1e10+0.75) hi", w[0]);
    P_HEX("fintrz(1e10+0.75) lo", w[1]);
    /* blk.out2 (FINT of the same value) is not printed: the reference's
     * 68040.library returns 0x80000000 for |x| >= 2^31 there */
    show("fgetexp(-12)", 11);
    show("fgetman(-12) x 1000", 12);
    show("fmovecr #0x35", 13);
    show("fmovecr #0x33", 14);
    P_HEX("fmovem.l fpcr/fpiar -> fpcr", res[15]);
    show("fdbf loop count", 17);
    P_HEX("fslt/fsgt bytes", res[18]);
    show("fmod(7,3)", 19);
    show("fscale(3,2)", 22);
    return 0;
}
