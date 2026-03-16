/*
 * Test: graphics/sprites_gels
 * Tests sprite allocation and GEL list handling.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/collide.h>
#include <graphics/gels.h>
#include <graphics/sprite.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct GfxBase *GfxBase;
extern struct ExecBase *SysBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static int g_boundary_hits;
static WORD g_boundary_flags;
static int g_pair_hits;
static struct VSprite *g_pair_first;
static struct VSprite *g_pair_second;
static int g_animob_routine_calls;
static int g_animcomp_timeout_calls;
static int g_animcomp_static_calls;
static int g_animcomp_nextseq_calls;
static struct AnimOb *g_last_animob;
static struct AnimComp *g_last_timeout_comp;
static struct AnimComp *g_last_static_comp;
static struct AnimComp *g_last_nextseq_comp;

static LONG boundary_collision(struct VSprite *vs, WORD hits)
{
    (void)vs;
    g_boundary_hits++;
    g_boundary_flags = hits;
    return 0;
}

static LONG pair_collision(struct VSprite *first, struct VSprite *second)
{
    g_pair_hits++;
    g_pair_first = first;
    g_pair_second = second;
    return 0;
}

static WORD animob_routine(struct AnimOb *anOb)
{
    g_animob_routine_calls++;
    g_last_animob = anOb;
    return 0;
}

static WORD animcomp_timeout_routine(struct AnimComp *anComp)
{
    g_animcomp_timeout_calls++;
    g_last_timeout_comp = anComp;
    return 0;
}

static WORD animcomp_static_routine(struct AnimComp *anComp)
{
    g_animcomp_static_calls++;
    g_last_static_comp = anComp;
    return 0;
}

static WORD animcomp_nextseq_routine(struct AnimComp *anComp)
{
    g_animcomp_nextseq_calls++;
    g_last_nextseq_comp = anComp;
    return 0;
}

static void init_test_vsprite(struct VSprite *vs,
                              WORD x,
                              WORD y,
                              WORD width_words,
                              WORD height,
                              WORD depth,
                              UWORD *image_data,
                              WORD flags)
{
    vs->NextVSprite = NULL;
    vs->PrevVSprite = NULL;
    vs->DrawPath = NULL;
    vs->ClearPath = NULL;
    vs->OldY = 0;
    vs->OldX = 0;
    vs->Flags = flags;
    vs->Y = y;
    vs->X = x;
    vs->Height = height;
    vs->Width = width_words;
    vs->Depth = depth;
    vs->MeMask = 0;
    vs->HitMask = 0;
    vs->ImageData = (WORD *)image_data;
    vs->BorderLine = NULL;
    vs->CollMask = NULL;
    vs->SprColors = NULL;
    vs->VSBob = NULL;
    vs->PlanePick = 1;
    vs->PlaneOnOff = 0;
    vs->VUserExt = 0;
}

int main(void)
{
    struct SimpleSprite simple_sprite;
    struct SimpleSprite sprite_pool[8];
    struct ViewPort vp;
    struct GelsInfo gels_info;
    struct VSprite head;
    struct VSprite tail;
    struct VSprite vs_a;
    struct VSprite vs_b;
    struct VSprite bob_vs;
    struct VSprite bob_top_vs;
    struct VSprite coll_a;
    struct VSprite coll_b;
    struct collTable coll_table;
    struct Bob bob;
    struct Bob bob_top;
    struct BitMap *bm;
    struct RastPort rp;
    UWORD sprite_data[1] = { 0x8000 };
    UWORD sprite_data_alt[1] = { 0x4000 };
    UWORD mask_image_data[4] = { 0x8000, 0x1000, 0x0200, 0x0010 };
    UWORD mask_collmask[2] = { 0, 0 };
    UWORD mask_borderline[1] = { 0 };
    UWORD collision_mask_a[2] = { 0x8000, 0x0000 };
    UWORD collision_mask_b[2] = { 0x8000, 0x0000 };
    UWORD border_line_a[1] = { 0 };
    UWORD border_line_b[1] = { 0 };
    int errors = 0;
    int i;

    print("Testing Sprite and GEL APIs...\n");

    simple_sprite.num = 0;
    if (GetSprite(&simple_sprite, 3) != 3 || simple_sprite.num != 3)
    {
        print("FAIL: GetSprite() specific allocation failed\n");
        errors++;
    }
    else
    {
        print("OK: GetSprite() specific allocation succeeded\n");
    }

    if (GetSprite(&sprite_pool[0], -1) != 0 || sprite_pool[0].num != 0)
    {
        print("FAIL: GetSprite() automatic allocation failed\n");
        errors++;
    }
    else
    {
        print("OK: GetSprite() automatic allocation returned first free sprite\n");
    }

    for (i = 1; i < 7; i++)
        GetSprite(&sprite_pool[i], -1);

    if (GetSprite(&sprite_pool[7], -1) != -1 || sprite_pool[7].num != (UWORD)-1)
    {
        print("FAIL: GetSprite() exhaustion did not return -1\n");
        errors++;
    }
    else
    {
        print("OK: GetSprite() reports exhaustion\n");
    }

    FreeSprite(3);
    if (GetSprite(&simple_sprite, 3) != 3)
    {
        print("FAIL: FreeSprite() did not release the slot\n");
        errors++;
    }
    else
    {
        print("OK: FreeSprite() released the slot\n");
    }

    for (i = 0; i < 8; i++)
        FreeSprite(i);

    {
        struct ExtSprite ext_sprite;
        struct ExtSprite attached_primary;
        struct ExtSprite attached_sprite;
        struct ExtSprite specific_sprite;
        struct ExtSprite specific_attached;
        struct ExtSprite fail_attached;
        struct TagItem specific_tags[] = {
            { GSTAG_SPRITE_NUM, 5 },
            { TAG_DONE, 0 }
        };
        struct TagItem attached_tags[] = {
            { GSTAG_ATTACHED, (ULONG)&attached_sprite },
            { TAG_DONE, 0 }
        };
        struct TagItem specific_attached_tags[] = {
            { GSTAG_SPRITE_NUM, 6 },
            { GSTAG_ATTACHED, (ULONG)&specific_attached },
            { TAG_DONE, 0 }
        };
        struct TagItem invalid_attached_tags[] = {
            { GSTAG_SPRITE_NUM, 3 },
            { GSTAG_ATTACHED, (ULONG)&fail_attached },
            { TAG_DONE, 0 }
        };

        ext_sprite.es_SimpleSprite.num = 99;
        attached_primary.es_SimpleSprite.num = 99;
        attached_sprite.es_SimpleSprite.num = 99;
        if (GetExtSpriteA(&ext_sprite, NULL) != 0 || ext_sprite.es_SimpleSprite.num != 0)
        {
            print("FAIL: GetExtSpriteA() automatic single allocation failed\n");
            errors++;
        }
        else
        {
            print("OK: GetExtSpriteA() allocates the first free single sprite\n");
        }

        if (GetExtSpriteA(&specific_sprite, specific_tags) != 5 || specific_sprite.es_SimpleSprite.num != 5)
        {
            print("FAIL: GetExtSpriteA() specific single allocation failed\n");
            errors++;
        }
        else
        {
            print("OK: GetExtSpriteA() honors GSTAG_SPRITE_NUM for single sprites\n");
        }

        if (GetExtSpriteA(&attached_primary, attached_tags) != 2 ||
            attached_primary.es_SimpleSprite.num != 2 ||
            attached_sprite.es_SimpleSprite.num != 3)
        {
            print("FAIL: GetExtSpriteA() automatic attached allocation failed\n");
            errors++;
        }
        else
        {
            print("OK: GetExtSpriteA() allocates the first free attached pair\n");
        }

        if (GetExtSpriteA(&specific_sprite, specific_attached_tags) != 6 ||
            specific_sprite.es_SimpleSprite.num != 6 ||
            specific_attached.es_SimpleSprite.num != 7)
        {
            print("FAIL: GetExtSpriteA() specific attached allocation failed\n");
            errors++;
        }
        else
        {
            print("OK: GetExtSpriteA() honors GSTAG_SPRITE_NUM for attached pairs\n");
        }

        fail_attached.es_SimpleSprite.num = 77;
        if (GetExtSpriteA(&specific_sprite, invalid_attached_tags) != -1 ||
            specific_sprite.es_SimpleSprite.num != (UWORD)-1 ||
            fail_attached.es_SimpleSprite.num != (UWORD)-1)
        {
            print("FAIL: GetExtSpriteA() accepted an odd attached sprite request\n");
            errors++;
        }
        else
        {
            print("OK: GetExtSpriteA() rejects odd-numbered attached pair requests\n");
        }

        FreeSprite(0);
        FreeSprite(5);
        FreeSprite(2);
        FreeSprite(3);
        FreeSprite(6);
        FreeSprite(7);
    }

    InitVPort(&vp);
    vp.DxOffset = 5;
    vp.DyOffset = 7;
    simple_sprite.posctldata = sprite_data;
    ChangeSprite(&vp, &simple_sprite, sprite_data_alt);
    MoveSprite(&vp, &simple_sprite, 12, 20);
    if (simple_sprite.posctldata != sprite_data_alt || simple_sprite.x != 17 || simple_sprite.y != 27)
    {
        print("FAIL: ChangeSprite()/MoveSprite() did not update sprite state\n");
        errors++;
    }
    else
    {
        print("OK: ChangeSprite()/MoveSprite() updated sprite state\n");
    }

    InitGels(&head, &tail, &gels_info);
    if (gels_info.gelHead != &head || gels_info.gelTail != &tail ||
        head.NextVSprite != &tail || tail.PrevVSprite != &head ||
        head.X != (WORD)-32768 || tail.X != (WORD)32767)
    {
        print("FAIL: InitGels() did not initialize sentinels\n");
        errors++;
    }
    else
    {
        print("OK: InitGels() initialized sentinels\n");
    }

    bm = AllocBitMap(64, 32, 1, BMF_CLEAR, NULL);
    if (!bm)
    {
        print("FAIL: AllocBitMap() returned NULL\n");
        return 20;
    }

    InitRastPort(&rp);
    rp.BitMap = bm;
    rp.GelsInfo = &gels_info;
    SetAPen(&rp, 1);

    init_test_vsprite(&vs_a, 20, 10, 1, 1, 1, sprite_data, VSPRITE);
    init_test_vsprite(&vs_b, 10, 5, 1, 1, 1, sprite_data, VSPRITE);

    AddVSprite(&vs_a, &rp);
    AddVSprite(&vs_b, &rp);
    if (head.NextVSprite != &vs_b || vs_b.NextVSprite != &vs_a || vs_a.NextVSprite != &tail)
    {
        print("FAIL: AddVSprite() did not sort by Y/X\n");
        errors++;
    }
    else
    {
        print("OK: AddVSprite() sorts by Y/X\n");
    }

    vs_b.X = 30;
    vs_b.Y = 15;
    SortGList(&rp);
    if (head.NextVSprite != &vs_a || vs_a.NextVSprite != &vs_b || vs_b.NextVSprite != &tail)
    {
        print("FAIL: SortGList() did not reorder list\n");
        errors++;
    }
    else
    {
        print("OK: SortGList() reordered the list\n");
    }

    RemVSprite(&vs_a);
    if (head.NextVSprite != &vs_b || vs_b.PrevVSprite != &head || vs_a.NextVSprite != NULL)
    {
        print("FAIL: RemVSprite() did not unlink the VSprite\n");
        errors++;
    }
    else
    {
        print("OK: RemVSprite() unlinked the VSprite\n");
    }

    init_test_vsprite(&coll_a, 0, 0, 1, 2, 2, mask_image_data, VSPRITE);
    coll_a.CollMask = mask_collmask;
    coll_a.BorderLine = mask_borderline;
    InitMasks(&coll_a);
    if ((UWORD)coll_a.CollMask[0] != 0x8200U ||
        (UWORD)coll_a.CollMask[1] != 0x1010U ||
        (UWORD)coll_a.BorderLine[0] != 0x9210U)
    {
        print("FAIL: InitMasks() did not build masks from image planes\n");
        errors++;
    }
    else
    {
        print("OK: InitMasks() builds CollMask and BorderLine\n");
    }

    InitGels(&head, &tail, &gels_info);
    rp.GelsInfo = &gels_info;
    gels_info.collHandler = &coll_table;
    for (i = 0; i < 16; i++)
        coll_table.collPtrs[i] = NULL;
    SetCollision(BORDERHIT, (VOID (*)())boundary_collision, &gels_info);
    SetCollision(2, (VOID (*)())pair_collision, &gels_info);
    SetCollision(15, (VOID (*)())pair_collision, &gels_info);
    if ((APTR)coll_table.collPtrs[BORDERHIT] != (APTR)boundary_collision ||
        (APTR)coll_table.collPtrs[2] != (APTR)pair_collision ||
        (APTR)coll_table.collPtrs[15] != (APTR)pair_collision ||
        coll_table.collPtrs[1] != NULL)
    {
        print("FAIL: SetCollision() did not populate collision vectors correctly\n");
        errors++;
    }
    else
    {
        print("OK: SetCollision() stores the requested collision vectors\n");
    }
    gels_info.leftmost = 8;
    gels_info.rightmost = 20;
    gels_info.topmost = 8;
    gels_info.bottommost = 20;

    init_test_vsprite(&coll_a, 7, 7, 1, 2, 1, sprite_data, VSPRITE);
    init_test_vsprite(&coll_b, 7, 7, 1, 2, 1, sprite_data, VSPRITE);
    coll_a.CollMask = collision_mask_a;
    coll_a.BorderLine = border_line_a;
    coll_a.MeMask = 1 << 2;
    coll_a.HitMask = (1 << BORDERHIT) | (1 << 2);
    coll_b.CollMask = collision_mask_b;
    coll_b.BorderLine = border_line_b;
    coll_b.MeMask = 1 << 2;
    coll_b.HitMask = 1 << 2;
    AddVSprite(&coll_a, &rp);
    AddVSprite(&coll_b, &rp);

    g_boundary_hits = 0;
    g_boundary_flags = 0;
    g_pair_hits = 0;
    g_pair_first = NULL;
    g_pair_second = NULL;
    DoCollision(&rp);

    if (g_boundary_hits != 1 || g_boundary_flags != (TOPHIT | LEFTHIT))
    {
        print("FAIL: DoCollision() boundary detection mismatch\n");
        errors++;
    }
    else
    {
        print("OK: DoCollision() reports boundary hits\n");
    }

    if (g_pair_hits != 1 ||
        !((g_pair_first == &coll_a && g_pair_second == &coll_b) ||
          (g_pair_first == &coll_b && g_pair_second == &coll_a)))
    {
        print("FAIL: DoCollision() gel collision dispatch mismatch\n");
        errors++;
    }
    else
    {
        print("OK: DoCollision() dispatches gel collision callback\n");
    }

    SetRast(&rp, 0);
    InitGels(&head, &tail, &gels_info);
    rp.GelsInfo = &gels_info;
    {
        struct VSprite anim_vs_a;
        struct VSprite anim_vs_b;
        struct Bob anim_bob_a;
        struct Bob anim_bob_b;
        struct AnimComp anim_comp_a;
        struct AnimComp anim_comp_b;
        struct AnimOb anim_first;
        struct AnimOb anim_second;
        struct AnimOb *anim_key = NULL;

        init_test_vsprite(&anim_vs_a, 11, 12, 1, 1, 1, sprite_data, 0);
        init_test_vsprite(&anim_vs_b, 13, 14, 1, 1, 1, sprite_data, 0);

        anim_bob_a.Flags = 0;
        anim_bob_a.SaveBuffer = NULL;
        anim_bob_a.ImageShadow = NULL;
        anim_bob_a.Before = NULL;
        anim_bob_a.After = NULL;
        anim_bob_a.BobVSprite = &anim_vs_a;
        anim_bob_a.BobComp = &anim_comp_a;
        anim_bob_a.DBuffer = NULL;
        anim_bob_a.BUserExt = 0;

        anim_bob_b.Flags = 0;
        anim_bob_b.SaveBuffer = NULL;
        anim_bob_b.ImageShadow = NULL;
        anim_bob_b.Before = NULL;
        anim_bob_b.After = NULL;
        anim_bob_b.BobVSprite = &anim_vs_b;
        anim_bob_b.BobComp = &anim_comp_b;
        anim_bob_b.DBuffer = NULL;
        anim_bob_b.BUserExt = 0;

        anim_comp_a.Flags = 0;
        anim_comp_a.Timer = 0;
        anim_comp_a.TimeSet = 7;
        anim_comp_a.NextComp = &anim_comp_b;
        anim_comp_a.PrevComp = NULL;
        anim_comp_a.NextSeq = &anim_comp_a;
        anim_comp_a.PrevSeq = &anim_comp_a;
        anim_comp_a.AnimCRoutine = NULL;
        anim_comp_a.YTrans = 0;
        anim_comp_a.XTrans = 0;
        anim_comp_a.HeadOb = &anim_first;
        anim_comp_a.AnimBob = &anim_bob_a;

        anim_comp_b.Flags = 0;
        anim_comp_b.Timer = 0;
        anim_comp_b.TimeSet = 3;
        anim_comp_b.NextComp = NULL;
        anim_comp_b.PrevComp = &anim_comp_a;
        anim_comp_b.NextSeq = &anim_comp_b;
        anim_comp_b.PrevSeq = &anim_comp_b;
        anim_comp_b.AnimCRoutine = NULL;
        anim_comp_b.YTrans = 0;
        anim_comp_b.XTrans = 0;
        anim_comp_b.HeadOb = &anim_first;
        anim_comp_b.AnimBob = &anim_bob_b;

        anim_first.NextOb = NULL;
        anim_first.PrevOb = (struct AnimOb *)0x1;
        anim_first.Clock = 0;
        anim_first.AnOldY = 0;
        anim_first.AnOldX = 0;
        anim_first.AnY = 0;
        anim_first.AnX = 0;
        anim_first.YVel = 0;
        anim_first.XVel = 0;
        anim_first.YAccel = 0;
        anim_first.XAccel = 0;
        anim_first.RingYTrans = 0;
        anim_first.RingXTrans = 0;
        anim_first.AnimORoutine = NULL;
        anim_first.HeadComp = &anim_comp_a;
        anim_first.AUserExt = 0;

        anim_second = anim_first;
        anim_second.HeadComp = NULL;
        anim_second.NextOb = (struct AnimOb *)0x1;
        anim_second.PrevOb = (struct AnimOb *)0x1;

        AddAnimOb(&anim_first, &anim_key, &rp);
        AddAnimOb(&anim_second, &anim_key, &rp);

        if (anim_key != &anim_second || anim_second.NextOb != &anim_first ||
            anim_second.PrevOb != NULL || anim_first.PrevOb != &anim_second ||
            anim_first.NextOb != NULL || anim_comp_a.Timer != 7 ||
            anim_comp_b.Timer != 3 || (anim_bob_a.Flags & BWAITING) == 0 ||
            (anim_bob_b.Flags & BWAITING) == 0 || anim_vs_a.VSBob != &anim_bob_a ||
            anim_vs_b.VSBob != &anim_bob_b)
        {
            print("FAIL: AddAnimOb() did not link objects or initialize components\n");
            errors++;
        }
        else
        {
            print("OK: AddAnimOb() prepends AnimObs and queues component Bobs\n");
        }
    }

    {
        struct VSprite seq_vs;
        struct VSprite next_vs;
        struct VSprite static_vs;
        struct Bob seq_bob;
        struct Bob next_bob;
        struct Bob static_bob;
        struct AnimComp seq_comp;
        struct AnimComp next_seq_comp;
        struct AnimComp static_comp;
        struct AnimOb anim_ob;
        struct AnimOb *anim_key = NULL;

        SetRast(&rp, 0);
        InitGels(&head, &tail, &gels_info);
        rp.GelsInfo = &gels_info;

        g_animob_routine_calls = 0;
        g_animcomp_timeout_calls = 0;
        g_animcomp_static_calls = 0;
        g_animcomp_nextseq_calls = 0;
        g_last_animob = NULL;
        g_last_timeout_comp = NULL;
        g_last_static_comp = NULL;
        g_last_nextseq_comp = NULL;

        init_test_vsprite(&seq_vs, 20, 18, 1, 1, 1, sprite_data, 0);
        init_test_vsprite(&next_vs, 2, 2, 1, 1, 1, sprite_data_alt, 0);
        init_test_vsprite(&static_vs, 6, 6, 1, 1, 1, sprite_data, 0);

        seq_bob.Flags = 0;
        seq_bob.SaveBuffer = NULL;
        seq_bob.ImageShadow = NULL;
        seq_bob.Before = NULL;
        seq_bob.After = NULL;
        seq_bob.BobVSprite = &seq_vs;
        seq_bob.BobComp = &seq_comp;
        seq_bob.DBuffer = NULL;
        seq_bob.BUserExt = 0;

        next_bob.Flags = 0;
        next_bob.SaveBuffer = NULL;
        next_bob.ImageShadow = NULL;
        next_bob.Before = NULL;
        next_bob.After = NULL;
        next_bob.BobVSprite = &next_vs;
        next_bob.BobComp = &next_seq_comp;
        next_bob.DBuffer = NULL;
        next_bob.BUserExt = 0;

        static_bob.Flags = 0;
        static_bob.SaveBuffer = NULL;
        static_bob.ImageShadow = NULL;
        static_bob.Before = NULL;
        static_bob.After = NULL;
        static_bob.BobVSprite = &static_vs;
        static_bob.BobComp = &static_comp;
        static_bob.DBuffer = NULL;
        static_bob.BUserExt = 0;

        seq_comp.Flags = RINGTRIGGER;
        seq_comp.Timer = 1;
        seq_comp.TimeSet = 5;
        seq_comp.NextComp = &static_comp;
        seq_comp.PrevComp = NULL;
        seq_comp.NextSeq = &next_seq_comp;
        seq_comp.PrevSeq = &next_seq_comp;
        seq_comp.AnimCRoutine = animcomp_timeout_routine;
        seq_comp.YTrans = 0;
        seq_comp.XTrans = 0;
        seq_comp.HeadOb = &anim_ob;
        seq_comp.AnimBob = &seq_bob;

        next_seq_comp.Flags = 0;
        next_seq_comp.Timer = 0;
        next_seq_comp.TimeSet = 4;
        next_seq_comp.NextComp = (struct AnimComp *)0x1;
        next_seq_comp.PrevComp = (struct AnimComp *)0x1;
        next_seq_comp.NextSeq = &seq_comp;
        next_seq_comp.PrevSeq = &seq_comp;
        next_seq_comp.AnimCRoutine = animcomp_nextseq_routine;
        next_seq_comp.YTrans = 128;
        next_seq_comp.XTrans = 64;
        next_seq_comp.HeadOb = &anim_ob;
        next_seq_comp.AnimBob = &next_bob;

        static_comp.Flags = 0;
        static_comp.Timer = 5;
        static_comp.TimeSet = 5;
        static_comp.NextComp = NULL;
        static_comp.PrevComp = &seq_comp;
        static_comp.NextSeq = &static_comp;
        static_comp.PrevSeq = &static_comp;
        static_comp.AnimCRoutine = animcomp_static_routine;
        static_comp.YTrans = 64;
        static_comp.XTrans = 0;
        static_comp.HeadOb = &anim_ob;
        static_comp.AnimBob = &static_bob;

        anim_ob.NextOb = NULL;
        anim_ob.PrevOb = NULL;
        anim_ob.Clock = 41;
        anim_ob.AnOldY = -1;
        anim_ob.AnOldX = -1;
        anim_ob.AnY = 0;
        anim_ob.AnX = 0;
        anim_ob.YVel = 64;
        anim_ob.XVel = 64;
        anim_ob.YAccel = 0;
        anim_ob.XAccel = 0;
        anim_ob.RingYTrans = 64;
        anim_ob.RingXTrans = 64;
        anim_ob.AnimORoutine = animob_routine;
        anim_ob.HeadComp = &seq_comp;
        anim_ob.AUserExt = 0;
        anim_key = &anim_ob;

        AddBob(&seq_bob, &rp);
        AddBob(&static_bob, &rp);
        Animate(&anim_key, &rp);

        if (anim_ob.Clock != 42 || anim_ob.AnOldX != 0 || anim_ob.AnOldY != 0 ||
            anim_ob.AnX != 128 || anim_ob.AnY != 128 ||
            anim_ob.HeadComp != &next_seq_comp || next_seq_comp.NextComp != &static_comp ||
            next_seq_comp.PrevComp != NULL || static_comp.PrevComp != &next_seq_comp ||
            next_seq_comp.Timer != 4 || (seq_bob.Flags & BOBSAWAY) == 0 ||
            (next_bob.Flags & BWAITING) == 0 || (UWORD)seq_vs.X != 0x8001 ||
            (UWORD)seq_vs.Y != 0x8001 ||
            next_vs.OldX != 20 || next_vs.OldY != 18 || next_vs.X != 3 || next_vs.Y != 4 ||
            static_vs.X != 2 || static_vs.Y != 3 || g_animob_routine_calls != 1 ||
            g_last_animob != &anim_ob || g_animcomp_timeout_calls != 1 ||
            g_last_timeout_comp != &seq_comp || g_animcomp_static_calls != 1 ||
            g_last_static_comp != &static_comp || g_animcomp_nextseq_calls != 0 ||
            g_last_nextseq_comp != NULL)
        {
            print("FAIL: Animate() did not update motion, sequence, and callbacks correctly\n");
            errors++;
        }
        else
        {
            print("OK: Animate() updates motion, switches sequences, and calls routines\n");
        }
    }

    {
        struct VSprite buf_vs_a;
        struct VSprite buf_vs_b;
        struct VSprite buf_vs_c;
        struct Bob buf_bob_a;
        struct Bob buf_bob_b;
        struct Bob buf_bob_c;
        struct AnimComp buf_comp_a;
        struct AnimComp buf_comp_b;
        struct AnimComp buf_comp_c;
        struct AnimComp buf_seq_a;
        struct AnimOb buf_anim;
        UWORD buf_image_data[4] = { 0x8000, 0x2000, 0x4000, 0x1000 };
        UWORD buf_image_data_alt[2] = { 0x1111, 0x2222 };
        UWORD buf_image_data_third[4] = { 0x0f00, 0x00f0, 0x3000, 0x0003 };
        UWORD buf_collmask_a[2] = { 0, 0 };
        UWORD buf_collmask_b[2] = { 0, 0 };
        UWORD buf_collmask_c[2] = { 0, 0 };
        UWORD buf_border_a[1] = { 0 };
        UWORD buf_border_b[1] = { 0 };
        UWORD buf_border_c[1] = { 0 };

        init_test_vsprite(&buf_vs_a, 0, 0, 1, 2, 2, buf_image_data, 0);
        init_test_vsprite(&buf_vs_b, 0, 0, 1, 2, 2, buf_image_data, 0);
        init_test_vsprite(&buf_vs_c, 0, 0, 1, 2, 2, buf_image_data_third, 0);

        buf_bob_a.Flags = 0;
        buf_bob_a.SaveBuffer = NULL;
        buf_bob_a.ImageShadow = NULL;
        buf_bob_a.Before = NULL;
        buf_bob_a.After = NULL;
        buf_bob_a.BobVSprite = &buf_vs_a;
        buf_bob_a.BobComp = &buf_comp_a;
        buf_bob_a.DBuffer = NULL;
        buf_bob_a.BUserExt = 0;

        buf_bob_b = buf_bob_a;
        buf_bob_b.BobVSprite = &buf_vs_b;
        buf_bob_b.BobComp = &buf_seq_a;

        buf_bob_c = buf_bob_a;
        buf_bob_c.BobVSprite = &buf_vs_c;
        buf_bob_c.BobComp = &buf_comp_b;

        buf_comp_a.Flags = 0;
        buf_comp_a.Timer = 0;
        buf_comp_a.TimeSet = 1;
        buf_comp_a.NextComp = &buf_comp_b;
        buf_comp_a.PrevComp = NULL;
        buf_comp_a.NextSeq = &buf_seq_a;
        buf_comp_a.PrevSeq = &buf_seq_a;
        buf_comp_a.AnimCRoutine = NULL;
        buf_comp_a.YTrans = 0;
        buf_comp_a.XTrans = 0;
        buf_comp_a.HeadOb = &buf_anim;
        buf_comp_a.AnimBob = &buf_bob_a;

        buf_seq_a = buf_comp_a;
        buf_seq_a.NextComp = NULL;
        buf_seq_a.PrevComp = NULL;
        buf_seq_a.NextSeq = &buf_comp_a;
        buf_seq_a.PrevSeq = &buf_comp_a;
        buf_seq_a.AnimBob = &buf_bob_b;

        buf_comp_b = buf_comp_a;
        buf_comp_b.NextComp = NULL;
        buf_comp_b.PrevComp = &buf_comp_a;
        buf_comp_b.NextSeq = &buf_comp_b;
        buf_comp_b.PrevSeq = &buf_comp_b;
        buf_comp_b.AnimBob = &buf_bob_c;

        buf_anim.NextOb = NULL;
        buf_anim.PrevOb = NULL;
        buf_anim.Clock = 0;
        buf_anim.AnOldY = 0;
        buf_anim.AnOldX = 0;
        buf_anim.AnY = 0;
        buf_anim.AnX = 0;
        buf_anim.YVel = 0;
        buf_anim.XVel = 0;
        buf_anim.YAccel = 0;
        buf_anim.XAccel = 0;
        buf_anim.RingYTrans = 0;
        buf_anim.RingXTrans = 0;
        buf_anim.AnimORoutine = NULL;
        buf_anim.HeadComp = &buf_comp_a;
        buf_anim.AUserExt = 0;

        buf_vs_a.CollMask = buf_collmask_a;
        buf_vs_a.BorderLine = buf_border_a;
        buf_vs_b.Depth = 1;
        buf_vs_b.ImageData = buf_image_data_alt;
        buf_vs_b.CollMask = buf_collmask_b;
        buf_vs_b.BorderLine = buf_border_b;
        buf_vs_c.CollMask = buf_collmask_c;
        buf_vs_c.BorderLine = buf_border_c;

        InitGMasks(&buf_anim);
        if ((UWORD)buf_vs_a.CollMask[0] != 0xc000U || (UWORD)buf_vs_a.CollMask[1] != 0x3000U ||
            (UWORD)buf_vs_a.BorderLine[0] != 0xf000U || (UWORD)buf_vs_b.CollMask[0] != 0x1111U ||
            (UWORD)buf_vs_b.CollMask[1] != 0x2222U || (UWORD)buf_vs_b.BorderLine[0] != 0x3333U ||
            (UWORD)buf_vs_c.CollMask[0] != 0x3f00U || (UWORD)buf_vs_c.CollMask[1] != 0x00f3U ||
            (UWORD)buf_vs_c.BorderLine[0] != 0x3ff3U)
        {
            print("FAIL: InitGMasks() did not initialize every sequence mask\n");
            errors++;
        }
        else
        {
            print("OK: InitGMasks() initializes every component sequence mask\n");
        }

        if (!GetGBuffers(&buf_anim, &rp, TRUE))
        {
            print("FAIL: GetGBuffers() did not allocate buffers for the AnimOb\n");
            errors++;
        }
        else if (!buf_bob_a.ImageShadow || buf_vs_a.CollMask != buf_bob_a.ImageShadow ||
                  !buf_bob_a.SaveBuffer || !buf_vs_a.BorderLine || !buf_bob_a.DBuffer ||
                 !buf_bob_a.DBuffer->BufBuffer || !buf_bob_b.ImageShadow ||
                 buf_vs_b.CollMask != buf_bob_b.ImageShadow || !buf_bob_b.SaveBuffer ||
                 !buf_vs_b.BorderLine || !buf_bob_b.DBuffer || !buf_bob_b.DBuffer->BufBuffer ||
                 !buf_bob_c.ImageShadow || buf_vs_c.CollMask != buf_bob_c.ImageShadow ||
                 !buf_bob_c.SaveBuffer || !buf_vs_c.BorderLine || !buf_bob_c.DBuffer ||
                 !buf_bob_c.DBuffer->BufBuffer)
        {
            print("FAIL: GetGBuffers() returned success without filling all buffer pointers\n");
            errors++;
        }
        else
        {
            print("OK: GetGBuffers() allocates save, mask, border, and DBuf buffers\n");
        }

        if (buf_bob_a.ImageShadow)
        {
            UWORD *separate_mask = (UWORD *)AllocMem(4, MEMF_CHIP | MEMF_CLEAR);

            if (!separate_mask)
            {
                print("FAIL: Could not allocate separate CollMask for FreeGBuffers() test\n");
                errors++;
                FreeGBuffers(&buf_anim, &rp, TRUE);
            }
            else
            {
                buf_vs_b.CollMask = separate_mask;
                FreeGBuffers(&buf_anim, &rp, TRUE);

                if (buf_bob_a.ImageShadow || buf_bob_a.SaveBuffer || buf_vs_a.CollMask || buf_vs_a.BorderLine ||
                    buf_bob_a.DBuffer || buf_bob_b.ImageShadow || buf_bob_b.SaveBuffer || buf_vs_b.CollMask ||
                    buf_vs_b.BorderLine || buf_bob_b.DBuffer || buf_bob_c.ImageShadow || buf_bob_c.SaveBuffer ||
                    buf_vs_c.CollMask || buf_vs_c.BorderLine || buf_bob_c.DBuffer)
                {
                    print("FAIL: FreeGBuffers() did not clear all allocated AnimOb buffers\n");
                    errors++;
                }
                else
                {
                    print("OK: FreeGBuffers() releases allocated buffers and clears pointers\n");
                }
            }
        }
    }

    SetRast(&rp, 0);
    InitGels(&head, &tail, &gels_info);
    rp.GelsInfo = &gels_info;

    init_test_vsprite(&bob_vs, 7, 9, 1, 1, 1, sprite_data, 0);
    bob.Flags = 0;
    bob.SaveBuffer = NULL;
    bob.ImageShadow = NULL;
    bob.Before = NULL;
    bob.After = NULL;
    bob.BobVSprite = &bob_vs;
    bob.BobComp = NULL;
    bob.DBuffer = NULL;
    bob.BUserExt = 0;

    AddBob(&bob, &rp);
    DrawGList(&rp, NULL);
    if ((bob.Flags & BDRAWN) == 0 || bob_vs.VSBob != &bob || ReadPixel(&rp, 7, 9) != 1)
    {
        print("FAIL: AddBob()/DrawGList() did not draw the Bob\n");
        errors++;
    }
    else
    {
        print("OK: AddBob()/DrawGList() drew the Bob\n");
    }

    SetRast(&rp, 0);
    InitGels(&head, &tail, &gels_info);
    rp.GelsInfo = &gels_info;
    init_test_vsprite(&bob_vs, 7, 9, 1, 1, 1, sprite_data, 0);
    init_test_vsprite(&bob_top_vs, 8, 9, 1, 1, 1, sprite_data, 0);
    bob.Flags = 0;
    bob.SaveBuffer = NULL;
    bob.ImageShadow = NULL;
    bob.Before = NULL;
    bob.After = NULL;
    bob.BobVSprite = &bob_vs;
    bob.BobComp = NULL;
    bob.DBuffer = NULL;
    bob.BUserExt = 0;
    bob_top.Flags = 0;
    bob_top.SaveBuffer = NULL;
    bob_top.ImageShadow = NULL;
    bob_top.Before = NULL;
    bob_top.After = NULL;
    bob_top.BobVSprite = &bob_top_vs;
    bob_top.BobComp = NULL;
    bob_top.DBuffer = NULL;
    bob_top.BUserExt = 0;

    AddBob(&bob, &rp);
    AddBob(&bob_top, &rp);
    DrawGList(&rp, NULL);
    RemIBob(&bob, &rp, NULL);
    if (head.NextVSprite != &bob_top_vs || bob_top_vs.PrevVSprite != &head ||
        bob_vs.NextVSprite != NULL || (bob.Flags & BOBNIX) == 0 ||
        (bob_top.Flags & BOBNIX) == 0 || ReadPixel(&rp, 7, 9) != 0 ||
        ReadPixel(&rp, 8, 9) != 0)
    {
        print("FAIL: RemIBob() did not immediately clear and unlink the Bob\n");
        errors++;
    }
    else
    {
        print("OK: RemIBob() immediately clears and unlinks the Bob\n");
    }

    DrawGList(&rp, NULL);
    if (ReadPixel(&rp, 7, 9) != 0 || ReadPixel(&rp, 8, 9) != 1 ||
        (bob_top.Flags & BOBNIX) != 0)
    {
        print("FAIL: DrawGList() did not redraw overlapping Bob after RemIBob()\n");
        errors++;
    }
    else
    {
        print("OK: DrawGList() redraws overlapping Bob after RemIBob()\n");
    }

    SetRast(&rp, 0);
    InitGels(&head, &tail, &gels_info);
    rp.GelsInfo = &gels_info;
    init_test_vsprite(&bob_vs, 7, 9, 1, 1, 1, sprite_data, 0);
    bob.Flags = 0;
    bob.SaveBuffer = NULL;
    bob.ImageShadow = NULL;
    bob.Before = NULL;
    bob.After = NULL;
    bob.BobVSprite = &bob_vs;
    bob.BobComp = NULL;
    bob.DBuffer = NULL;
    bob.BUserExt = 0;

    AddBob(&bob, &rp);
    DrawGList(&rp, NULL);
    RemBob(&bob);
    DrawGList(&rp, NULL);
    if ((bob.Flags & BOBNIX) == 0 || head.NextVSprite != &tail)
    {
        print("FAIL: RemBob macro + DrawGList() did not retire the Bob\n");
        errors++;
    }
    else
    {
        print("OK: RemBob macro is honored by DrawGList()\n");
    }

    FreeBitMap(bm);

    if (errors == 0)
    {
        print("\nPASS: graphics/sprites_gels all tests passed\n");
        return 0;
    }

    print("\nFAIL: graphics/sprites_gels had errors\n");
    return 20;
}
