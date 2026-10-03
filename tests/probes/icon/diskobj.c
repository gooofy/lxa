/*
 * Probe (Phase 222f): icon.library V40 DiskObject I/O - GetDefDiskObject
 * for every type, PutDiskObject of default and hand-built icons (file
 * bytes dumped with pointer fields masked), GetDiskObject/GetDiskObjectNew
 * read-back, error cases, DeleteDiskObject, PutDefDiskObject round trip.
 * Output compared with AmigaOS 3.1.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <clib/exec_protos.h>
#include <clib/icon_protos.h>
#include <inline/exec.h>
#include <inline/icon.h>
#include "iconprobe.h"

struct Library *IconBase;

/* hide existing default icons (ENV:Sys/def_*) for the duration of the probe */
static char hidden[24][48];
static int nhidden;

static void hide_defaults(void)
{
    static struct FileInfoBlock fib;
    char found[24][40];
    int n = 0, i;
    BPTR lock = Lock((STRPTR)"ENV:Sys", ACCESS_READ);
    if (!lock)
        return;
    if (Examine(lock, &fib)) {
        while (ExNext(lock, &fib) && n < 24) {
            const char *s = fib.fib_FileName;
            int len = 0;
            while (s[len])
                len++;
            if (len < 4 || len > 36 || s[0] != 'd' || s[1] != 'e' || s[2] != 'f' || s[3] != '_')
                continue;
            for (i = 0; i <= len; i++)
                found[n][i] = s[i];
            n++;
        }
    }
    UnLock(lock);
    for (i = 0; i < n; i++) {
        char from[64] = "ENV:Sys/", to[64];
        int k = 8, m = 0;
        while (found[i][m])
            from[k++] = found[i][m++];
        from[k] = 0;
        for (m = 0; m <= k; m++)
            to[m] = from[m];
        to[k] = '_';
        to[k + 1] = 'p';
        to[k + 2] = 0;
        if (Rename((STRPTR)from, (STRPTR)to)) {
            for (m = 0; m <= k; m++)
                hidden[nhidden][m] = from[m];
            nhidden++;
        }
    }
}

static void restore_defaults(void)
{
    int i;
    for (i = 0; i < nhidden; i++) {
        char to[64];
        int k = 0;
        while (hidden[i][k]) {
            to[k] = hidden[i][k];
            k++;
        }
        to[k] = '_';
        to[k + 1] = 'p';
        to[k + 2] = 0;
        DeleteFile((STRPTR)hidden[i]);
        Rename((STRPTR)to, (STRPTR)hidden[i]);
    }
}

/* 16x6, depth 2 */
static UWORD __chip img1data[] = {
    0xffff, 0x8001, 0x8181, 0x8181, 0x8001, 0xffff,
    0x0000, 0x7ffe, 0x7e7e, 0x7e7e, 0x7ffe, 0x0000,
};
/* 20x3, depth 1 */
static UWORD __chip img2data[] = {
    0xaaaa, 0xa000, 0x5555, 0x5000, 0xffff, 0xf000,
};

static struct Image img1 = {0, 0, 16, 6, 2, img1data, 3, 0, NULL};
static struct Image img2 = {1, 2, 20, 3, 1, img2data, 1, 2, NULL};
static struct Image img3 = {3, 4, 16, 6, 2, img1data, 2, 1, NULL};
static struct Image img4 = {-1, -2, 20, 3, 1, img2data, 0, 1, NULL};

static STRPTR tt1[] = {(STRPTR)"FILETYPE=text|ascii", (STRPTR)"(COMMENT)", (STRPTR)"DONOTWAIT",
                       (STRPTR)"", (STRPTR)"X=", NULL};
static STRPTR tt_empty[] = {NULL};

static void init_dobj(struct DiskObject *d, UBYTE type)
{
    UBYTE *p = (UBYTE *)d;
    ULONG i;
    for (i = 0; i < sizeof(*d); i++)
        p[i] = 0;
    d->do_Magic = WB_DISKMAGIC;
    d->do_Version = WB_DISKVERSION;
    d->do_Type = type;
    d->do_Gadget.Width = 16;
    d->do_Gadget.Height = 7;
    d->do_Gadget.Flags = GFLG_GADGIMAGE | GFLG_GADGHCOMP;
    d->do_Gadget.Activation = GACT_RELVERIFY | GACT_IMMEDIATE;
    d->do_Gadget.GadgetType = GTYP_BOOLGADGET;
    d->do_Gadget.GadgetRender = &img1;
    d->do_Gadget.UserData = (APTR)WB_DISKREVISION;
    d->do_CurrentX = NO_ICON_POSITION;
    d->do_CurrentY = NO_ICON_POSITION;
}

static void roundtrip(const char *label, const char *name, struct DiskObject *d)
{
    struct DiskObject *r;
    BOOL ok;
    SetIoErr(1234);
    ok = PutDiskObject((STRPTR)name, d);
    probe_s(label);
    probe_s(": PutDiskObject = ");
    probe_s(ok ? "TRUE" : "FALSE");
    p_kv("IoErr", IoErr());
    probe_ch('\n');
    if (!ok)
        return;
    dump_info(name);
    SetIoErr(1234);
    r = GetDiskObject((STRPTR)name);
    p_diskobj("  read back", r);
    if (r)
        FreeDiskObject(r);
    SetIoErr(1234);
    ok = DeleteDiskObject((STRPTR)name);
    probe_s("  DeleteDiskObject = ");
    probe_s(ok ? "TRUE" : "FALSE");
    p_kv("IoErr", IoErr());
    probe_ch('\n');
}

/* print (and optionally delete) the def_* files PutDefDiskObject created */
static void list_defs(BOOL del)
{
    static struct FileInfoBlock fib;
    static char names[16][40];
    int n = 0, i;
    BPTR lock = Lock((STRPTR)"ENV:Sys", ACCESS_READ);
    if (!lock)
        return;
    if (Examine(lock, &fib)) {
        while (ExNext(lock, &fib) && n < 16) {
            const char *s = fib.fib_FileName;
            int len = 0;
            while (s[len])
                len++;
            if (len < 4 || s[0] != 'd' || s[1] != 'e' || s[2] != 'f' || s[3] != '_')
                continue;
            if (len > 2 && s[len - 2] == '_' && s[len - 1] == 'p')
                continue;
            for (i = 0; i <= len && i < 39; i++)
                names[n][i] = s[i];
            names[n][39] = 0;
            n++;
        }
    }
    UnLock(lock);
    /* sort for a stable order */
    for (i = 0; i < n; i++) {
        int j;
        for (j = i + 1; j < n; j++) {
            int k = 0;
            while (names[i][k] && names[i][k] == names[j][k])
                k++;
            if ((UBYTE)names[i][k] > (UBYTE)names[j][k]) {
                char tmp[40];
                CopyMem(names[i], tmp, 40);
                CopyMem(names[j], names[i], 40);
                CopyMem(tmp, names[j], 40);
            }
        }
    }
    for (i = 0; i < n; i++) {
        char path[64] = "ENV:Sys/";
        int k = 8, m = 0;
        P_STR("ENV:Sys entry", names[i]);
        while (names[i][m])
            path[k++] = names[i][m++];
        path[k] = 0;
        if (del)
            DeleteFile((STRPTR)path);
    }
}

static void write_raw(const char *name, const UBYTE *data, LONG len)
{
    BPTR fh = Open((STRPTR)name, MODE_NEWFILE);
    if (fh) {
        Write(fh, (APTR)data, len);
        Close(fh);
    }
}

int main(void)
{
    struct DiskObject *d, hand;
    struct DrawerData dd;
    LONG t;
    BPTR lock;
    BOOL made_sys = FALSE;

    IconBase = OpenLibrary((STRPTR)"icon.library", 37);
    if (!IconBase)
        return 20;

    lock = Lock((STRPTR)"ENV:Sys", ACCESS_READ);
    if (lock)
        UnLock(lock);
    else {
        lock = CreateDir((STRPTR)"ENV:Sys");
        if (lock) {
            UnLock(lock);
            made_sys = TRUE;
        }
    }
    hide_defaults();

    P_SECTION("GetDefDiskObject");
    for (t = -1; t <= 10; t++) {
        char lab[32];
        int k = 0;
        const char *s = "GetDefDiskObject(";
        while (*s)
            lab[k++] = *s++;
        if (t < 0)
            lab[k++] = '-', lab[k++] = '1';
        else if (t >= 10)
            lab[k++] = '1', lab[k++] = '0';
        else
            lab[k++] = (char)('0' + t);
        lab[k++] = ')';
        lab[k] = 0;
        SetIoErr(1234);
        d = GetDefDiskObject(t);
        p_diskobj(lab, d);
        if (d) {
            if (d->do_Gadget.GadgetRender) {
                struct Image *im = (struct Image *)d->do_Gadget.GadgetRender;
                if (im->ImageData && im->Width > 0 && im->Height > 0 && im->Depth > 0)
                    P_BYTES("  render data", im->ImageData,
                            ((im->Width + 15) / 16) * 2 * im->Height * im->Depth);
            }
            if (d->do_Gadget.SelectRender) {
                struct Image *im = (struct Image *)d->do_Gadget.SelectRender;
                if (im->ImageData && im->Width > 0 && im->Height > 0 && im->Depth > 0)
                    P_BYTES("  select data", im->ImageData,
                            ((im->Width + 15) / 16) * 2 * im->Height * im->Depth);
            }
            roundtrip("  put default", "T:icprobe_def", d);
            FreeDiskObject(d);
        }
    }

    P_SECTION("hand-built icons");
    init_dobj(&hand, WBTOOL);
    hand.do_ToolTypes = tt1;
    hand.do_StackSize = 8192;
    hand.do_CurrentX = 10;
    hand.do_CurrentY = 20;
    roundtrip("tool with tooltypes", "T:icprobe_a", &hand);

    init_dobj(&hand, WBPROJECT);
    hand.do_DefaultTool = (STRPTR)"SYS:Utilities/More";
    hand.do_Gadget.SelectRender = &img2;
    hand.do_Gadget.Flags = GFLG_GADGIMAGE | GFLG_GADGHIMAGE;
    roundtrip("project with two images", "T:icprobe_b", &hand);

    init_dobj(&hand, WBPROJECT);
    hand.do_Gadget.GadgetRender = &img3;
    hand.do_Gadget.SelectRender = &img4;
    hand.do_Gadget.Flags = GFLG_GADGIMAGE | GFLG_GADGHIMAGE;
    roundtrip("images with offsets", "T:icprobe_b2", &hand);
    p_image("in-memory render after put", &img3);
    p_image("in-memory select after put", &img4);

    init_dobj(&hand, WBPROJECT);
    hand.do_DefaultTool = (STRPTR)"";
    hand.do_ToolTypes = tt_empty;
    hand.do_StackSize = -5;
    roundtrip("project empty tool, empty tooltypes", "T:icprobe_c", &hand);

    {
        UBYTE *p = (UBYTE *)&dd;
        ULONG i;
        for (i = 0; i < sizeof(dd); i++)
            p[i] = 0;
    }
    dd.dd_NewWindow.LeftEdge = 10;
    dd.dd_NewWindow.TopEdge = 20;
    dd.dd_NewWindow.Width = 300;
    dd.dd_NewWindow.Height = 100;
    dd.dd_NewWindow.DetailPen = 255;
    dd.dd_NewWindow.BlockPen = 255;
    dd.dd_NewWindow.IDCMPFlags = 0x12345678;
    dd.dd_NewWindow.Flags = WFLG_SIZEGADGET | WFLG_DRAGBAR;
    dd.dd_NewWindow.Title = (UBYTE *)"title";
    dd.dd_NewWindow.MinWidth = 90;
    dd.dd_NewWindow.MinHeight = 40;
    dd.dd_NewWindow.MaxWidth = -1;
    dd.dd_NewWindow.MaxHeight = -1;
    dd.dd_NewWindow.Type = WBENCHSCREEN;
    dd.dd_CurrentX = 7;
    dd.dd_CurrentY = -3;
    dd.dd_Flags = DDFLAGS_SHOWALL;
    dd.dd_ViewModes = DDVM_BYNAME;

    init_dobj(&hand, WBDRAWER);
    hand.do_DrawerData = &dd;
    roundtrip("drawer revision 1", "T:icprobe_e", &hand);

    init_dobj(&hand, WBDRAWER);
    hand.do_DrawerData = &dd;
    hand.do_Gadget.UserData = 0;
    roundtrip("drawer revision 0", "T:icprobe_f", &hand);

    init_dobj(&hand, WBDISK);
    hand.do_DrawerData = &dd;
    hand.do_DefaultTool = (STRPTR)"SYS:System/DiskCopy";
    roundtrip("disk", "T:icprobe_g", &hand);

    init_dobj(&hand, WBGARBAGE);
    hand.do_DrawerData = &dd;
    roundtrip("garbage", "T:icprobe_h", &hand);

    init_dobj(&hand, WBTOOL);
    hand.do_DrawerData = &dd;
    roundtrip("tool with drawerdata", "T:icprobe_i", &hand);

    init_dobj(&hand, WBPROJECT);
    hand.do_Gadget.GadgetRender = NULL;
    hand.do_Gadget.Width = 0;
    hand.do_Gadget.Height = 0;
    roundtrip("project without image", "T:icprobe_j", &hand);

    init_dobj(&hand, WBKICK);
    hand.do_Gadget.LeftEdge = 5;
    hand.do_Gadget.TopEdge = 6;
    hand.do_Gadget.GadgetID = 99;
    hand.do_Gadget.MutualExclude = 0x11223344;
    hand.do_Gadget.Flags = 0xffff;
    hand.do_Gadget.Activation = 0;
    hand.do_Gadget.GadgetType = 0x1234;
    roundtrip("kick odd fields", "T:icprobe_k", &hand);

    init_dobj(&hand, WBTOOL);
    hand.do_Version = 7;
    roundtrip("version 7", "T:icprobe_l", &hand);

    init_dobj(&hand, WBTOOL);
    hand.do_Version = 0;
    roundtrip("version 0", "T:icprobe_m", &hand);

    init_dobj(&hand, WBTOOL);
    hand.do_Gadget.UserData = (APTR)0x00000105;
    roundtrip("userdata 0x105", "T:icprobe_n", &hand);

    init_dobj(&hand, WBTOOL);
    hand.do_Gadget.UserData = (APTR)0x00000100;
    roundtrip("userdata 0x100", "T:icprobe_o", &hand);

    init_dobj(&hand, WBTOOL);
    hand.do_Gadget.UserData = (APTR)0x00000002;
    roundtrip("userdata 2", "T:icprobe_p", &hand);

    init_dobj(&hand, 0);
    roundtrip("type 0", "T:icprobe_q", &hand);

    init_dobj(&hand, 200);
    roundtrip("type 200", "T:icprobe_r", &hand);

    init_dobj(&hand, WBTOOL);
    roundtrip("put to missing dir", "T:icprobe_nodir/x", &hand);

    P_SECTION("GetDiskObject errors");
    SetIoErr(1234);
    d = GetDiskObject(NULL);
    p_diskobj("GetDiskObject(NULL)", d);
    if (d)
        FreeDiskObject(d);
    SetIoErr(1234);
    d = GetDiskObject((STRPTR)"T:icprobe_missing");
    p_diskobj("missing", d);
    if (d)
        FreeDiskObject(d);
    {
        static const UBYTE bad[80] = {0x12, 0x34, 0, 1};
        write_raw("T:icprobe_bad.info", bad, sizeof(bad));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_bad");
        p_diskobj("bad magic", d);
        if (d)
            FreeDiskObject(d);
        DeleteFile((STRPTR)"T:icprobe_bad.info");
    }
    {
        static const UBYTE trunc[40] = {0xe3, 0x10, 0, 1};
        write_raw("T:icprobe_tr.info", trunc, sizeof(trunc));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_tr");
        p_diskobj("truncated header", d);
        if (d)
            FreeDiskObject(d);
        DeleteFile((STRPTR)"T:icprobe_tr.info");
    }
    {
        /* valid header, no images, no strings, version 0 */
        static UBYTE minimal[78];
        minimal[0] = 0xe3;
        minimal[1] = 0x10;
        minimal[48] = WBPROJECT;
        minimal[77] = 0x10;
        write_raw("T:icprobe_min.info", minimal, sizeof(minimal));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_min");
        p_diskobj("minimal header", d);
        if (d)
            FreeDiskObject(d);
        DeleteFile((STRPTR)"T:icprobe_min.info");
    }
    {
        /* header announces an image and a default tool, file ends early */
        static UBYTE part[90];
        part[0] = 0xe3;
        part[1] = 0x10;
        part[3] = 1;
        part[25] = 1;
        part[48] = WBTOOL;
        part[53] = 1;
        write_raw("T:icprobe_part.info", part, sizeof(part));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_part");
        p_diskobj("truncated image", d);
        if (d)
            FreeDiskObject(d);
        DeleteFile((STRPTR)"T:icprobe_part.info");
    }
    {
        /* header only, version 1, no image */
        static UBYTE hdronly[78];
        hdronly[0] = 0xe3;
        hdronly[1] = 0x10;
        hdronly[3] = 1;
        hdronly[48] = WBPROJECT;
        write_raw("T:icprobe_ho.info", hdronly, sizeof(hdronly));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_ho");
        p_diskobj("header only", d);
        if (d)
            FreeDiskObject(d);
        /* the same with a missing default tool string */
        hdronly[53] = 1;
        write_raw("T:icprobe_ho.info", hdronly, sizeof(hdronly));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_ho");
        p_diskobj("header only, default tool missing", d);
        if (d)
            FreeDiskObject(d);
        /* GADGIMAGE set, then other gadget flags */
        hdronly[53] = 0;
        hdronly[17] = GFLG_GADGIMAGE;
        write_raw("T:icprobe_ho.info", hdronly, sizeof(hdronly));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_ho");
        p_diskobj("header only, GADGIMAGE", d);
        if (d)
            FreeDiskObject(d);
        hdronly[17] = GFLG_GADGHIMAGE;
        write_raw("T:icprobe_ho.info", hdronly, sizeof(hdronly));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_ho");
        p_diskobj("header only, GADGHIMAGE", d);
        if (d)
            FreeDiskObject(d);
        hdronly[17] = 0;
        hdronly[16] = 0xff;
        write_raw("T:icprobe_ho.info", hdronly, sizeof(hdronly));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_ho");
        p_diskobj("header only, flags 0xff00", d);
        if (d)
            FreeDiskObject(d);
        hdronly[16] = 0;
        hdronly[17] = GFLG_GADGIMAGE;
        hdronly[53] = 1;            /* default tool announced, missing */
        write_raw("T:icprobe_ho.info", hdronly, sizeof(hdronly));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_ho");
        p_diskobj("GADGIMAGE, default tool missing", d);
        if (d)
            FreeDiskObject(d);
        hdronly[53] = 0;
        hdronly[25] = 1;            /* image announced, missing */
        write_raw("T:icprobe_ho.info", hdronly, sizeof(hdronly));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_ho");
        p_diskobj("GADGIMAGE, image missing", d);
        if (d)
            FreeDiskObject(d);
        DeleteFile((STRPTR)"T:icprobe_ho.info");
    }
    {
        /* garbage in pointer fields that have no file data */
        static UBYTE g[78 + 20 + 4];
        g[0] = 0xe3;
        g[1] = 0x10;
        g[3] = 1;
        g[7] = 0x11;                /* NextGadget */
        g[13] = 16;                 /* Width */
        g[15] = 2;                  /* Height */
        g[25] = 1;                  /* GadgetRender */
        g[33] = 0x22;               /* GadgetText */
        g[41] = 0x33;               /* SpecialInfo */
        g[48] = WBTOOL;
        g[17] = GFLG_GADGIMAGE;
        /* image: 16x2 depth 1, ImageData 0, NextImage set */
        g[78 + 5] = 16;
        g[78 + 7] = 2;
        g[78 + 9] = 1;
        g[78 + 14] = 1;
        g[78 + 19] = 0x44;
        g[98] = 0xf0;
        g[99] = 0x0f;
        g[100] = 0x55;
        g[101] = 0xaa;
        write_raw("T:icprobe_g.info", g, sizeof(g));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_g");
        p_diskobj("garbage pointers", d);
        if (d) {
            struct Image *im = (struct Image *)d->do_Gadget.GadgetRender;
            if (im && im->ImageData)
                P_BYTES("  image data", im->ImageData, 4);
            FreeDiskObject(d);
        }
        /* image struct fields as stored in the file */
        g[78 + 1] = 5;              /* LeftEdge */
        g[78 + 3] = 6;              /* TopEdge */
        g[78 + 14] = 0;             /* PlanePick */
        g[78 + 15] = 3;             /* PlaneOnOff */
        write_raw("T:icprobe_g.info", g, sizeof(g));
        SetIoErr(1234);
        d = GetDiskObject((STRPTR)"T:icprobe_g");
        p_diskobj("image fields", d);
        if (d)
            FreeDiskObject(d);
        DeleteFile((STRPTR)"T:icprobe_g.info");
    }
    SetIoErr(1234);
    P_BOOL("DeleteDiskObject(missing)", DeleteDiskObject((STRPTR)"T:icprobe_missing"));
    P_LONG("  IoErr", IoErr());

    P_SECTION("GetDiskObjectNew");
    write_raw("T:icprobe_file", (const UBYTE *)"hello", 5);
    {
        static const UBYTE hunk[] = {0, 0, 3, 0xf3, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
                                     0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 3, 0xe9, 0, 0, 0, 1,
                                     0x4e, 0x75, 0, 0, 0, 0, 3, 0xf2};
        write_raw("T:icprobe_exe", hunk, sizeof(hunk));
    }
    lock = CreateDir((STRPTR)"T:icprobe_dir");
    if (lock)
        UnLock(lock);
    {
        static const char *const names[] = {
            "T:icprobe_file", "T:icprobe_exe", "T:icprobe_dir", "T:icprobe_missing", "T:", "RAM:",
        };
        int i;
        for (i = 0; i < 6; i++) {
            SetIoErr(1234);
            d = GetDiskObjectNew((STRPTR)names[i]);
            probe_s("GetDiskObjectNew(");
            probe_s(names[i]);
            probe_s(")");
            p_diskobj("", d);
            if (d)
                FreeDiskObject(d);
        }
    }
    SetProtection((STRPTR)"T:icprobe_file", FIBF_EXECUTE);
    SetIoErr(0);
    d = GetDiskObjectNew((STRPTR)"T:icprobe_file");
    probe_s("GetDiskObjectNew(file, not executable) type = ");
    probe_dec(d ? d->do_Type : -1);
    probe_ch('\n');
    if (d)
        FreeDiskObject(d);
    /* an existing icon wins over the guess */
    init_dobj(&hand, WBKICK);
    PutDiskObject((STRPTR)"T:icprobe_dir", &hand);
    d = GetDiskObjectNew((STRPTR)"T:icprobe_dir");
    probe_s("GetDiskObjectNew(dir with kick icon) type = ");
    probe_dec(d ? d->do_Type : -1);
    probe_ch('\n');
    if (d)
        FreeDiskObject(d);
    DeleteDiskObject((STRPTR)"T:icprobe_dir");
    SetProtection((STRPTR)"T:icprobe_file", 0);
    DeleteFile((STRPTR)"T:icprobe_file");
    DeleteFile((STRPTR)"T:icprobe_exe");
    DeleteFile((STRPTR)"T:icprobe_dir");

    P_SECTION("PutDefDiskObject");
    for (t = WBDISK; t <= WBAPPICON; t++) {
        BOOL ok;
        init_dobj(&hand, (UBYTE)t);
        hand.do_DefaultTool = (STRPTR)"deftool";
        hand.do_StackSize = 1000 + t;
        if (t == WBDISK || t == WBDRAWER || t == WBGARBAGE)
            hand.do_DrawerData = &dd;
        SetIoErr(1234);
        ok = PutDefDiskObject(&hand);
        probe_s("PutDefDiskObject(type ");
        probe_dec(t);
        probe_s(") = ");
        probe_s(ok ? "TRUE" : "FALSE");
        p_kv("IoErr", IoErr());
        probe_ch('\n');
        d = GetDefDiskObject(t);
        p_diskobj("  GetDefDiskObject", d);
        if (d)
            FreeDiskObject(d);
    }
    init_dobj(&hand, 0);
    SetIoErr(1234);
    P_BOOL("PutDefDiskObject(type 0)", PutDefDiskObject(&hand));
    P_LONG("  IoErr", IoErr());
    init_dobj(&hand, 9);
    SetIoErr(1234);
    P_BOOL("PutDefDiskObject(type 9)", PutDefDiskObject(&hand));
    P_LONG("  IoErr", IoErr());
    init_dobj(&hand, 10);
    SetIoErr(1234);
    P_BOOL("PutDefDiskObject(type 10)", PutDefDiskObject(&hand));
    P_LONG("  IoErr", IoErr());
    init_dobj(&hand, 255);
    SetIoErr(1234);
    P_BOOL("PutDefDiskObject(type 255)", PutDefDiskObject(&hand));
    P_LONG("  IoErr", IoErr());
    list_defs(TRUE);

    restore_defaults();
    if (made_sys)
        DeleteFile((STRPTR)"ENV:Sys");
    CloseLibrary(IconBase);
    return 0;
}
