/*
 * Test: graphics/sprites_gels
 * Tests sprite allocation and GEL list handling.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
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
    struct Bob bob;
    struct BitMap *bm;
    struct RastPort rp;
    UWORD sprite_data[1] = { 0x8000 };
    UWORD sprite_data_alt[1] = { 0x4000 };
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
