/*
 * Probe (Phase 222b remainder): user copper lists (UCopperListInit/CMove/
 * CWait/CBump/FreeCopList), Bob removal (RemIBob), GEL buffers
 * (GetGBuffers/FreeGBuffers) and hardware sprite allocation (GetSprite/
 * GetExtSpriteA/FreeSprite).
 *
 * Setups follow the RKRM (GelsInfo with nextLine/lastColor, a ViewPort for
 * DrawGList/RemIBob, a RastPort with a BitMap for GetGBuffers): AmigaOS 3.1
 * hangs on DrawGList(rp, NULL) with Bobs and on GetGBuffers() with a
 * RastPort without BitMap.
 */
#include <exec/memory.h>
#include <graphics/gels.h>
#include <graphics/copper.h>
#include <graphics/sprite.h>
#include <graphics/view.h>
#include <hardware/custom.h>
#include "gfxprobe.h"

static struct gp_bm g;
static struct RastPort rp;

/* ---------------------------------------------------------------- copper */

static void p_ins(const char *label, struct CopIns *ci)
{
    probe_s(label);
    probe_s(" = op ");
    probe_dec(ci->OpCode);
    probe_s(" ");
    probe_hex((UWORD)ci->u3.u4.u1.DestAddr, 4);
    probe_s(" ");
    probe_hex((UWORD)ci->u3.u4.u2.DestData, 4);
    probe_ch('\n');
}

static void p_cl(const char *label, struct UCopList *ucl, struct CopList *cl)
{
    probe_s(label);
    probe_s(" = count ");
    probe_dec(cl->Count);
    probe_s(" max ");
    probe_dec(cl->MaxCount);
    probe_s(" ptr ");
    probe_dec(cl->CopPtr ? (LONG)(cl->CopPtr - cl->CopIns) : -1);
    probe_s(cl->Next ? " next" : " -");
    probe_s(ucl->CopList == cl ? " current" : "");
    probe_s(ucl->FirstCopList == cl ? " first" : "");
    probe_ch('\n');
}

static void copper(void)
{
    struct UCopList *ucl;
    struct CopList *first, *cl, *again;
    struct CopIns *saved_ins;
    struct CopIns *mine;

    gp_section("UCopperListInit");
    ucl = AllocMem(sizeof(struct UCopList), MEMF_PUBLIC | MEMF_CLEAR);
    mine = AllocMem(4 * sizeof(struct CopIns), MEMF_PUBLIC | MEMF_CLEAR);
    if (!ucl || !mine)
        return;
    first = UCopperListInit(ucl, 2);
    P_NULL("result", first);
    if (!first)
        return;
    P_BOOL("result==FirstCopList", first == ucl->FirstCopList);
    p_cl("first", ucl, first);
    P_LONG("ucl.Next", ucl->Next != NULL);

    gp_section("CMove/CWait/CBump");
    CMove(ucl, (APTR)0x0180, 0x55aa);
    p_ins("after CMove ins0", &first->CopIns[0]);
    p_cl("after CMove", ucl, first);
    CBump(ucl);
    p_cl("after CBump", ucl, first);
    CWait(ucl, 0x0033, 0x0077);
    p_ins("after CWait ins1", &first->CopIns[1]);
    CBump(ucl);
    p_cl("after 2nd CBump first", ucl, first);
    P_LONG("first ins1 op", first->CopIns[1].OpCode);
    cl = first->Next;
    if (cl) {
        p_cl("second", ucl, cl);
        P_BOOL("ins1.nxtlist==second", first->CopIns[1].u3.nxtlist == cl);
        p_ins("second ins0", &cl->CopIns[0]);
        CMove(ucl, (APTR)0x0182, 0x0123);
        p_cl("second after CMove", ucl, cl);
        CBump(ucl);
        p_cl("second after CBump", ucl, cl);
        p_ins("second ins1", &cl->CopIns[1]);
    }
    CEND(ucl);
    p_cl("after CEND current", ucl, ucl->CopList);

    /* full block: CopPtr behind the last slot, pointing into our own array */
    gp_section("full block");
    saved_ins = first->CopIns;
    first->CopIns = mine;
    first->CopPtr = mine + 1;
    first->Count = 1;
    first->MaxCount = 1;
    ucl->CopList = first;
    CMove(ucl, (APTR)0x0184, 0x0abc);
    p_ins("CMove slot1", &mine[1]);
    p_cl("after CMove", ucl, first);
    CWait(ucl, 0x0010, 0x0020);
    p_ins("CWait slot1", &mine[1]);
    p_cl("after CWait", ucl, first);
    first->CopIns = saved_ins;
    first->CopPtr = saved_ins + 2;
    first->Count = 2;
    first->MaxCount = 2;

    gp_section("UCopperListInit again");
    ucl->CopList = cl ? cl : first;
    saved_ins = first->CopIns;
    again = UCopperListInit(ucl, 3);
    P_NULL("result", again);
    P_BOOL("same first", again == first);
    if (again)
        P_BOOL("same CopIns", again->CopIns == saved_ins);
    if (again)
        P_BOOL("same Next", again->Next == cl);
    if (again)
        p_cl("again", ucl, again);
    FreeCopList(ucl->FirstCopList);
    FreeMem(ucl, sizeof(struct UCopList));
    FreeMem(mine, 4 * sizeof(struct CopIns));
}

/* ------------------------------------------------------------------ gels */

static WORD *chip_words(WORD n)
{
    return (WORD *)gp_chip(NULL, (ULONG)n * 2);
}

static void init_gels(struct VSprite *head, struct VSprite *tail, struct GelsInfo *gi)
{
    InitGels(head, tail, gi);
    gi->nextLine = (WORD *)AllocMem(8 * sizeof(WORD), MEMF_CLEAR);
    gi->lastColor = (WORD **)AllocMem(8 * sizeof(LONG), MEMF_CLEAR);
    gi->collHandler = (struct collTable *)AllocMem(sizeof(struct collTable), MEMF_CLEAR);
    gi->sprRsrvd = 0xFC;
    gi->leftmost = 0;
    gi->rightmost = 63;
    gi->topmost = 0;
    gi->bottommost = 31;
    rp.GelsInfo = gi;
}

static void free_gels(struct GelsInfo *gi)
{
    FreeMem(gi->nextLine, 8 * sizeof(WORD));
    FreeMem(gi->lastColor, 8 * sizeof(LONG));
    FreeMem(gi->collHandler, sizeof(struct collTable));
    rp.GelsInfo = NULL;
}

static void init_vs(struct VSprite *vs, WORD x, WORD y, WORD *image, WORD flags)
{
    UBYTE *p = (UBYTE *)vs;
    LONG i;
    for (i = 0; i < (LONG)sizeof(*vs); i++)
        p[i] = 0;
    vs->X = x;
    vs->Y = y;
    vs->Height = 1;
    vs->Width = 1;
    vs->Depth = 1;
    vs->Flags = flags;
    vs->ImageData = image;
    vs->PlanePick = 1;
    vs->CollMask = chip_words(1);
    vs->BorderLine = chip_words(1);
    InitMasks(vs);
}

static void init_bob(struct Bob *bob, struct VSprite *vs, WORD x, WORD y, WORD *image,
                     WORD flags, BOOL save)
{
    UBYTE *p = (UBYTE *)bob;
    LONG i;
    init_vs(vs, x, y, image, flags);
    for (i = 0; i < (LONG)sizeof(*bob); i++)
        p[i] = 0;
    bob->BobVSprite = vs;
    bob->ImageShadow = vs->CollMask;
    bob->SaveBuffer = save ? chip_words(8) : NULL;
    vs->VSBob = bob;
}

static void background(BOOL pattern)
{
    LONG n = (LONG)g.bm.BytesPerRow * g.bm.Rows, k;
    gp_clear(&g);
    if (pattern)
        for (k = 0; k < n; k++) {
            g.bm.Planes[0][k] = 0x5a;
            g.bm.Planes[1][k] = 0x33;
        }
}

static void remibob(WORD flags, BOOL save, BOOL pattern)
{
    struct VSprite head, tail, base_vs, cover_vs;
    struct GelsInfo gi;
    struct Bob base, cover;
    struct ViewPort vp;
    WORD *image = chip_words(1);

    image[0] = (WORD)0x8000;
    background(pattern);
    InitVPort(&vp);
    init_gels(&head, &tail, &gi);
    init_bob(&base, &base_vs, 4, 4, image, flags, save);
    init_bob(&cover, &cover_vs, 5, 4, image, flags, save);
    AddBob(&base, &rp);
    AddBob(&cover, &rp);
    DrawGList(&rp, &vp);
    WaitBlit();
    gp_row("drawn", &rp, 4, 0, 24);
    P_HEX("flags base/cover", ((ULONG)(UWORD)base.Flags << 16) | (UWORD)cover.Flags);
    P_HEX("vsflags base/cover", ((ULONG)(UWORD)base_vs.Flags << 16) | (UWORD)cover_vs.Flags);
    if (save) {
        P_BYTES("base save", base.SaveBuffer, 16);
        P_BYTES("cover save", cover.SaveBuffer, 16);
    }
    RemIBob(&base, &rp, &vp);
    WaitBlit();
    gp_row("after RemIBob", &rp, 4, 0, 24);
    P_HEX("flags base/cover", ((ULONG)(UWORD)base.Flags << 16) | (UWORD)cover.Flags);
    P_HEX("vsflags base/cover", ((ULONG)(UWORD)base_vs.Flags << 16) | (UWORD)cover_vs.Flags);
    P_BOOL("head->cover", head.NextVSprite == &cover_vs);
    P_BOOL("cover->head", cover_vs.PrevVSprite == &head);
    P_BOOL("cover->tail", cover_vs.NextVSprite == &tail);
    DrawGList(&rp, &vp);
    WaitBlit();
    gp_row("redrawn", &rp, 4, 0, 24);
    P_HEX("flags base/cover", ((ULONG)(UWORD)base.Flags << 16) | (UWORD)cover.Flags);
    cover_vs.X = 9;
    cover_vs.Y = 5;
    DrawGList(&rp, &vp);
    WaitBlit();
    gp_row("moved y4", &rp, 4, 0, 24);
    gp_row("moved y5", &rp, 5, 0, 24);
    P_HEX("vsflags cover", (UWORD)cover_vs.Flags);
    P_LONG("cover OldX", cover_vs.OldX);
    P_LONG("cover OldY", cover_vs.OldY);
    RemBob(&cover);
    DrawGList(&rp, &vp);
    WaitBlit();
    gp_row("cover removed y4", &rp, 4, 0, 24);
    gp_row("cover removed y5", &rp, 5, 0, 24);
    P_HEX("flags cover", (UWORD)cover.Flags);
    P_HEX("vsflags cover", (UWORD)cover_vs.Flags);
    P_BOOL("list empty", head.NextVSprite == &tail);
    free_gels(&gi);
    gp_chip_free();
}

static void p_buf(const char *label, struct Bob *bob)
{
    struct VSprite *vs = bob->BobVSprite;
    probe_s(label);
    probe_s(" =");
    probe_s(bob->ImageShadow ? " shadow" : " -");
    probe_s(vs->CollMask ? (vs->CollMask == bob->ImageShadow ? " coll=shadow" : " coll") : " -");
    probe_s(bob->SaveBuffer ? " save" : " -");
    probe_s(vs->BorderLine ? " border" : " -");
    probe_s(bob->DBuffer ? " dbuf" : " -");
    if (bob->DBuffer)
        probe_s(bob->DBuffer->BufBuffer ? " bufbuffer" : " -");
    probe_ch('\n');
}

static void gbuffers(LONG dbuf)
{
    struct VSprite vs_a, vs_b;
    struct Bob bob_a, bob_b;
    struct AnimComp comp_a, comp_b;
    struct AnimOb anim;
    WORD *image = chip_words(4);
    UBYTE *p;
    LONG i;
    BOOL ok;

    image[0] = (WORD)0x8000;
    image[1] = 0x2000;
    image[2] = 0x4000;
    image[3] = 0x1000;
    p = (UBYTE *)&anim;
    for (i = 0; i < (LONG)sizeof(anim); i++)
        p[i] = 0;
    p = (UBYTE *)&comp_a;
    for (i = 0; i < (LONG)sizeof(comp_a); i++)
        p[i] = 0;
    init_bob(&bob_a, &vs_a, 0, 0, image, SAVEBACK | OVERLAY, FALSE);
    init_bob(&bob_b, &vs_b, 0, 0, image, SAVEBACK | OVERLAY, FALSE);
    vs_a.Height = 2;
    vs_a.Depth = 2;
    vs_b.Height = 2;
    vs_a.CollMask = vs_a.BorderLine = NULL;
    vs_b.CollMask = vs_b.BorderLine = NULL;
    bob_a.ImageShadow = bob_b.ImageShadow = NULL;
    bob_a.BobComp = &comp_a;
    bob_b.BobComp = &comp_b;
    comp_a.TimeSet = 1;
    comp_a.NextSeq = comp_a.PrevSeq = &comp_a;
    comp_a.NextComp = &comp_b;
    comp_a.HeadOb = &anim;
    comp_a.AnimBob = &bob_a;
    comp_b = comp_a;
    comp_b.NextComp = NULL;
    comp_b.PrevComp = &comp_a;
    comp_b.NextSeq = comp_b.PrevSeq = &comp_b;
    comp_b.AnimBob = &bob_b;
    anim.HeadComp = &comp_a;

    ok = GetGBuffers(&anim, &rp, dbuf);
    P_BOOL("GetGBuffers", ok);
    p_buf("bob a", &bob_a);
    p_buf("bob b", &bob_b);
    if (ok) {
        InitGMasks(&anim);
        P_HEX("a coll", ((ULONG)(UWORD)vs_a.CollMask[0] << 16) | (UWORD)vs_a.CollMask[1]);
        P_HEX("a border", (UWORD)vs_a.BorderLine[0]);
        P_HEX("b coll", ((ULONG)(UWORD)vs_b.CollMask[0] << 16) | (UWORD)vs_b.CollMask[1]);
        FreeGBuffers(&anim, &rp, dbuf);
        p_buf("freed a", &bob_a);
        p_buf("freed b", &bob_b);
    }
    gp_chip_free();
}

/* --------------------------------------------------------------- sprites */

static struct ExtSprite *make_ext(struct gp_bm *src)
{
    struct TagItem tags[2];
    tags[0].ti_Tag = SPRITEA_Width;
    tags[0].ti_Data = 16;
    tags[1].ti_Tag = TAG_DONE;
    return AllocSpriteDataA(&src->bm, tags);
}

static void ext_sprites(void)
{
    struct gp_bm src;
    struct ExtSprite *es, *es2;
    struct TagItem tags[3];
    WORD i, r;

    if (!gp_alloc(&src, 16, 2, 2))
        return;
    src.bm.Planes[0][0] = 0x80;
    src.bm.Planes[1][0] = 0x40;
    es = make_ext(&src);
    es2 = make_ext(&src);
    P_NULL("AllocSpriteDataA", es);
    if (!es || !es2)
        return;
    P_LONG("height", es->es_SimpleSprite.height);
    P_LONG("wordwidth", es->es_wordwidth);
    P_LONG("flags", es->es_flags);
    P_HEX("data0", ((ULONG)es->es_SimpleSprite.posctldata[2] << 16) | es->es_SimpleSprite.posctldata[3]);
    P_HEX("data1", ((ULONG)es->es_SimpleSprite.posctldata[4] << 16) | es->es_SimpleSprite.posctldata[5]);

    r = GetExtSpriteA(es, NULL);
    P_LONG("no tags", r);
    P_LONG("num", es->es_SimpleSprite.num);
    if (r >= 0)
        FreeSprite(r);

    for (i = 0; i < 8; i++) {
        tags[0].ti_Tag = GSTAG_SPRITE_NUM;
        tags[0].ti_Data = i;
        tags[1].ti_Tag = TAG_DONE;
        es->es_SimpleSprite.num = 77;
        r = GetExtSpriteA(es, tags);
        probe_s("GSTAG_SPRITE_NUM ");
        probe_dec(i);
        probe_s(" = ");
        probe_dec(r);
        probe_s(" num ");
        probe_dec(es->es_SimpleSprite.num);
        probe_ch('\n');
        if (r >= 0)
            FreeSprite(r);
    }

    for (i = 0; i < 2; i++) {
        tags[0].ti_Tag = GSTAG_ATTACHED;
        tags[0].ti_Data = (ULONG)es2;
        tags[1].ti_Tag = i ? GSTAG_SPRITE_NUM : TAG_DONE;
        tags[1].ti_Data = 2;
        tags[2].ti_Tag = TAG_DONE;
        es->es_SimpleSprite.num = 77;
        es2->es_SimpleSprite.num = 77;
        r = GetExtSpriteA(es, tags);
        P_LONG(i ? "GSTAG_ATTACHED num 2" : "GSTAG_ATTACHED", r);
        P_LONG("num", es->es_SimpleSprite.num);
        P_LONG("attached num", es2->es_SimpleSprite.num);
        P_LONG("attached flags", es2->es_flags);
        if (r >= 0) {
            FreeSprite(r);
            if (es2->es_SimpleSprite.num >= 0 && es2->es_SimpleSprite.num < 8 &&
                es2->es_SimpleSprite.num != r)
                FreeSprite(es2->es_SimpleSprite.num);
        }
    }
    FreeSpriteData(es);
    FreeSpriteData(es2);
    gp_free(&src);
}

static void sprites(void)
{
    struct SimpleSprite ss[8];
    WORD got[8];
    WORD i, r;

    gp_section("GetSprite");
    for (i = 0; i < 8; i++) {
        ss[i].num = 77;
        got[i] = GetSprite(&ss[i], i);
        probe_s("GetSprite(");
        probe_dec(i);
        probe_s(") = ");
        probe_dec(got[i]);
        probe_s(" num ");
        probe_dec(ss[i].num);
        probe_ch('\n');
    }
    for (i = 0; i < 8; i++)
        if (got[i] >= 0)
            FreeSprite(got[i]);

    for (i = 0; i < 8; i++) {
        ss[i].num = 77;
        got[i] = GetSprite(&ss[i], -1);
        probe_s("GetSprite(-1) #");
        probe_dec(i);
        probe_s(" = ");
        probe_dec(got[i]);
        probe_s(" num ");
        probe_dec(ss[i].num);
        probe_ch('\n');
    }
    for (i = 0; i < 8; i++)
        if (got[i] >= 0)
            FreeSprite(got[i]);

    ss[0].num = 77;
    r = GetSprite(&ss[0], 3);
    ss[1].num = 77;
    P_LONG("GetSprite(3)", r);
    P_LONG("GetSprite(3) again", GetSprite(&ss[1], 3));
    P_LONG("again num", ss[1].num);
    if (r >= 0)
        FreeSprite(r);

    gp_section("GetExtSpriteA");
    ext_sprites();
}
int main(int argc, char **argv)
{
    gp_args(argc, argv);
    if (!gp_alloc(&g, 32, 16, 2))
        return 20;
    gp_rp(&rp, &g);
    SetAPen(&rp, 1);

    copper();

    gp_section("RemIBob plain");
    remibob(0, FALSE, FALSE);
    gp_section("RemIBob plain pattern");
    remibob(OVERLAY, FALSE, TRUE);
    gp_section("RemIBob SAVEBACK");
    remibob(SAVEBACK | OVERLAY, TRUE, FALSE);
    gp_section("RemIBob SAVEBACK pattern");
    remibob(SAVEBACK | OVERLAY, TRUE, TRUE);
    gp_section("RemIBob SAVEBACK no OVERLAY pattern");
    remibob(SAVEBACK, TRUE, TRUE);

    gp_section("GetGBuffers dbuf");
    gbuffers(TRUE);
    gp_section("GetGBuffers single");
    gbuffers(FALSE);

    sprites();

    gp_free(&g);
    return 0;
}
