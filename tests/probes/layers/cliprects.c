/*
 * Probe (Phase 222c): layers.library arrangement - CreateUpfrontLayer/
 * CreateBehindLayer, MoveLayer, SizeLayer, UpfrontLayer, BehindLayer,
 * MoveLayerInFrontOf, MoveSizeLayer, DeleteLayer, WhichLayer - printing
 * every layer's ClipRect list (relative to the layer, with the obscuring
 * layer and whether it has backing store), damage region and flags.
 * Off-screen bitmap, compared with AmigaOS 3.1.
 */
#include "layerprobe.h"

#define W 160
#define H 100

static struct gp_bm g;

static void which(WORD x, WORD y)
{
    struct Layer *l = WhichLayer(lp_layers[0]->LayerInfo, x, y);
    probe_s("WhichLayer ");
    probe_dec(x);
    probe_ch(',');
    probe_dec(y);
    probe_s(" = L");
    probe_dec(l ? lp_index(l) : -1);
    probe_ch('\n');
}

int main(int argc, char **argv)
{
    struct Layer_Info *li;
    LONG rc;

    gp_args(argc, argv);
    LayersBase = OpenLibrary((STRPTR)"layers.library", 39);
    if (!LayersBase || !gp_alloc(&g, W, H, 2))
        return 20;
    li = NewLayerInfo();
    if (!li)
        return 20;

    lp_layers[0] = CreateUpfrontLayer(li, &g.bm, 0, 0, W - 1, H - 1, LAYERSIMPLE | LAYERBACKDROP, NULL);
    lp_layers[1] = CreateUpfrontLayer(li, &g.bm, 10, 10, 89, 59, LAYERSIMPLE, NULL);
    lp_layers[2] = CreateUpfrontLayer(li, &g.bm, 40, 30, 139, 79, LAYERSMART, NULL);
    lp_layers[3] = CreateBehindLayer(li, &g.bm, 20, 50, 69, 94, LAYERSMART, NULL);
    lp_layers[4] = CreateUpfrontLayer(li, &g.bm, 100, 5, 150, 40, LAYERSIMPLE, NULL);
    P_NULL("layers created", lp_layers[4]);
    lp_dump_all("created");

    which(5, 5);
    which(50, 40);
    which(25, 55);
    which(25, 85);
    which(120, 20);
    which(159, 99);
    which(-1, 0);

    rc = MoveLayer(0, lp_layers[1], 30, 5);
    P_LONG("MoveLayer L1 +30,+5", rc);
    lp_dump_all("after MoveLayer");

    rc = SizeLayer(0, lp_layers[2], -30, 15);
    P_LONG("SizeLayer L2 -30,+15", rc);
    lp_dump_all("after SizeLayer");

    rc = UpfrontLayer(0, lp_layers[3]);
    P_LONG("UpfrontLayer L3", rc);
    lp_dump_all("after UpfrontLayer");

    rc = BehindLayer(0, lp_layers[4]);
    P_LONG("BehindLayer L4", rc);
    lp_dump_all("after BehindLayer");

    rc = MoveLayerInFrontOf(lp_layers[2], lp_layers[1]);
    P_LONG("MoveLayerInFrontOf L2 L1", rc);
    lp_dump_all("after MoveLayerInFrontOf");

    rc = MoveSizeLayer(lp_layers[4], -60, 20, 10, -5);
    P_LONG("MoveSizeLayer L4", rc);
    lp_dump_all("after MoveSizeLayer");

    rc = UpfrontLayer(0, lp_layers[0]);
    P_LONG("UpfrontLayer backdrop", rc);
    rc = BehindLayer(0, lp_layers[1]);
    P_LONG("BehindLayer L1", rc);
    lp_dump_all("after backdrop moves");

    rc = DeleteLayer(0, lp_layers[2]);
    P_LONG("DeleteLayer L2", rc);
    lp_layers[2] = NULL;
    lp_dump_all("after DeleteLayer");


    {
        int i;
        for (i = LP_MAX - 1; i >= 0; i--)
            if (lp_layers[i]) {
                DeleteLayer(0, lp_layers[i]);
                lp_layers[i] = NULL;
            }
    }
    DisposeLayerInfo(li);
    gp_free(&g);
    CloseLibrary(LayersBase);
    return 0;
}
