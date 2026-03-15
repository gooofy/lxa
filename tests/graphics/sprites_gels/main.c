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
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct GfxBase *GfxBase;

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
