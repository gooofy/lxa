/*
 * Probe (Phase 222f): iffparse.library writing - OpenIFF(IFFF_WRITE),
 * PushChunk/PopChunk (known and IFFSIZE_UNKNOWN sizes, odd lengths and pad
 * bytes, nested FORM/LIST/CAT/PROP, illegal nesting and bad IDs),
 * WriteChunkBytes/WriteChunkRecords (incl. writing past a fixed size),
 * stream-hook errors, the three seek modes, and InitIFFasDOS to a T: file.
 * Every hook call is logged.
 */
#include "iffprobe.h"

static struct MemStream ms;

/* a fresh handle on the memory stream, opened for writing */
static struct IFFHandle *wopen(const char *label, LONG flags)
{
    struct IFFHandle *iff = AllocIFF();
    LONG rc;
    ms_reset(&ms);
    iff->iff_Stream = (ULONG)&ms;
    InitIFF(iff, flags, &ip_streamhook);
    rc = OpenIFF(iff, IFFF_WRITE);
    probe_s(label);
    probe_s(": OpenIFF = ");
    ip_err(rc);
    probe_s(" flags=");
    probe_hex(iff->iff_Flags, 8);
    probe_ch('\n');
    ip_flushlog();
    return iff;
}

static void wclose(const char *label, struct IFFHandle *iff)
{
    CloseIFF(iff);
    probe_s(label);
    probe_s(": closed d=");
    probe_dec(iff->iff_Depth);
    probe_ch('\n');
    ip_flushlog();
    FreeIFF(iff);
    ip_bytes("  out", ms.data, ms.len);
}

static void push(struct IFFHandle *iff, ULONG type, ULONG id, LONG size)
{
    LONG rc = PushChunk(iff, type, id, size);
    probe_s("Push ");
    ip_id(id);
    probe_ch('/');
    ip_id(type);
    probe_ch(' ');
    probe_dec(size);
    ip_state("", iff, rc);
}

static void pop(struct IFFHandle *iff)
{
    ip_state("Pop", iff, PopChunk(iff));
}

static void wbytes(struct IFFHandle *iff, const char *s, LONG n)
{
    LONG rc = WriteChunkBytes(iff, (APTR)s, n);
    probe_s("WriteBytes ");
    probe_dec(n);
    ip_state("", iff, rc);
}

static void wrecs(struct IFFHandle *iff, const char *s, LONG size, LONG n)
{
    LONG rc = WriteChunkRecords(iff, (APTR)s, size, n);
    probe_s("WriteRecs ");
    probe_dec(size);
    probe_ch('x');
    probe_dec(n);
    ip_state("", iff, rc);
}

/* the same small ILBM in each seek mode */
static void simple(const char *label, LONG flags, LONG formsize, LONG bodysize)
{
    struct IFFHandle *iff;
    P_SECTION(label);
    iff = wopen(label, flags);
    push(iff, ID_ILBM, ID_FORM, formsize);
    push(iff, 0, ID_BMHD, 4);
    wbytes(iff, "abcd", 4);
    pop(iff);
    push(iff, ID_ILBM, ID_CMAP, IFFSIZE_UNKNOWN);
    wbytes(iff, "xyz", 3);
    pop(iff);
    push(iff, 0, ID_BODY, bodysize);
    wbytes(iff, "0123456", 7);
    wrecs(iff, "ABCDEF", 2, 3);
    pop(iff);
    pop(iff);
    pop(iff);
    wclose(label, iff);
}

static void dosfile(void)
{
    struct IFFHandle *iff;
    BPTR fh;
    LONG rc, n;
    static UBYTE buf[256];

    P_SECTION("dos");
    fh = Open((CONST_STRPTR)"T:iffprobe_w.iff", MODE_NEWFILE);
    P_BOOL("Open", fh != 0);
    if (!fh)
        return;
    iff = AllocIFF();
    iff->iff_Stream = (ULONG)fh;
    InitIFFasDOS(iff);
    P_HEX("flags after InitIFFasDOS", iff->iff_Flags);
    rc = OpenIFF(iff, IFFF_WRITE);
    P_LONG("OpenIFF", rc);
    P_HEX("flags after OpenIFF", iff->iff_Flags);
    P_LONG("Push CAT", PushChunk(iff, ID_ILBM, ID_CAT, IFFSIZE_UNKNOWN));
    P_LONG("Push FORM", PushChunk(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN));
    P_LONG("Push BMHD", PushChunk(iff, 0, ID_BMHD, IFFSIZE_UNKNOWN));
    P_LONG("Write", WriteChunkBytes(iff, "hello", 5));
    P_LONG("Pop BMHD", PopChunk(iff));
    P_LONG("Seek pos", Seek(fh, 0, OFFSET_CURRENT));
    P_LONG("Pop FORM", PopChunk(iff));
    P_LONG("Push FORM2", PushChunk(iff, ID_8SVX, ID_FORM, 16));
    P_LONG("Push BODY", PushChunk(iff, 0, ID_BODY, 3));
    P_LONG("Write", WriteChunkBytes(iff, "xyzw", 4));
    P_LONG("Pop BODY", PopChunk(iff));
    P_LONG("Pop FORM2", PopChunk(iff));
    P_LONG("Pop CAT", PopChunk(iff));
    P_LONG("Seek pos", Seek(fh, 0, OFFSET_CURRENT));
    CloseIFF(iff);
    P_LONG("Seek pos after CloseIFF", Seek(fh, 0, OFFSET_CURRENT));
    FreeIFF(iff);
    Close(fh);
    fh = Open((CONST_STRPTR)"T:iffprobe_w.iff", MODE_OLDFILE);
    n = Read(fh, buf, sizeof(buf));
    Close(fh);
    ip_bytes("file", buf, n);
    DeleteFile((CONST_STRPTR)"T:iffprobe_w.iff");
}

int main(void)
{
    struct IFFHandle *iff;

    if (!ip_open())
        return 20;

    simple("rseek", IFFF_RSEEK, IFFSIZE_UNKNOWN, IFFSIZE_UNKNOWN);
    simple("fseek", IFFF_FSEEK, IFFSIZE_UNKNOWN, IFFSIZE_UNKNOWN);
    simple("noseek", 0, IFFSIZE_UNKNOWN, IFFSIZE_UNKNOWN);
    simple("noseek-known", 0, 50, 13);
    simple("rseek-known", IFFF_RSEEK, 50, 13);
    simple("rseek-small-body", IFFF_RSEEK, IFFSIZE_UNKNOWN, 9);
    simple("rseek-wrong-form", IFFF_RSEEK, 20, 13);
    simple("fseek-known", IFFF_FSEEK, 50, 13);
    simple("both", IFFF_FSEEK | IFFF_RSEEK, IFFSIZE_UNKNOWN, IFFSIZE_UNKNOWN);
    simple("rseek-big-form", IFFF_RSEEK, 60, 13);

    P_SECTION("odd");
    iff = wopen("odd", IFFF_RSEEK);
    push(iff, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_CHRS, IFFSIZE_UNKNOWN);
    wbytes(iff, "a", 1);
    wbytes(iff, "bc", 2);
    wbytes(iff, "", 0);
    wbytes(iff, "d", -1);
    pop(iff);
    push(iff, 0, ID_CHRS, 1);
    wbytes(iff, "q", 1);
    pop(iff);
    push(iff, 0, ID_ANNO, 0);
    pop(iff);
    push(iff, 0, ID_ANNO, IFFSIZE_UNKNOWN);
    pop(iff);
    pop(iff);
    wclose("odd", iff);

    P_SECTION("records");
    iff = wopen("records", IFFF_RSEEK);
    push(iff, ID_TEST, ID_FORM, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_DATA, 10);
    wrecs(iff, "AAABBBCCCDDD", 3, 4);
    wrecs(iff, "EEE", 3, 1);
    wrecs(iff, "x", 1, 0);
    wbytes(iff, "zzzz", 4);
    pop(iff);
    push(iff, 0, ID_NAME, 4);
    wbytes(iff, "toolong", 7);
    wbytes(iff, "more", 4);
    pop(iff);
    pop(iff);
    wclose("records", iff);

    P_SECTION("nested");
    iff = wopen("nested", IFFF_RSEEK);
    push(iff, ID_ILBM, ID_LIST, IFFSIZE_UNKNOWN);
    push(iff, ID_ILBM, ID_PROP, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_BMHD, 2);
    wbytes(iff, "PP", 2);
    pop(iff);
    pop(iff);
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_BODY, IFFSIZE_UNKNOWN);
    wbytes(iff, "b1", 2);
    pop(iff);
    pop(iff);
    push(iff, ID_ILBM, ID_CAT, IFFSIZE_UNKNOWN);
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_BODY, IFFSIZE_UNKNOWN);
    wbytes(iff, "b2b", 3);
    pop(iff);
    pop(iff);
    pop(iff);
    pop(iff);
    wclose("nested", iff);

    P_SECTION("illegal");
    iff = wopen("illegal", IFFF_RSEEK);
    pop(iff);
    wbytes(iff, "nochunk", 7);
    wrecs(iff, "nochunk", 1, 7);
    push(iff, 0, ID_BMHD, 4);                       /* leaf at top level */
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);   /* poisoned after that? */
    wclose("illegal", iff);
    {
        static const ULONG cases[][7] = {
            /* outer type, outer id, inner type, inner id, inner2 type, inner2 id */
            { 0, 0, ID_ILBM, ID_PROP, 0, 0 },
            { 0, 0, MAKE_ID('i','l','b','m'), ID_FORM, 0, 0 },
            { 0, 0, MAKE_ID(' ','A','B','C'), ID_FORM, 0, 0 },
            { 0, 0, MAKE_ID(' ',' ',' ',' '), ID_FORM, 0, 0 },
            { 0, 0, MAKE_ID('i','l','b','m'), ID_LIST, 0, 0 },
            { 0, 0, MAKE_ID('i','l','b','m'), ID_CAT, 0, 0 },
            { 0, 0, ID_ILBM, MAKE_ID('X','Y','Z','1'), 0, 0 },
            { 0, 0, ID_ILBM, MAKE_ID('F','O','R','1'), 0, 0 },
            { 0, 0, ID_ILBM, MAKE_ID('f','o','r','m'), 0, 0 },
            { ID_ILBM, ID_FORM, ID_ILBM, ID_PROP, 0, 0 },
            { ID_ILBM, ID_FORM, 0, MAKE_ID('a','\001','c','d'), 0, 0 },
            { ID_ILBM, ID_FORM, 0, MAKE_ID(' ','A','B','C'), 0, 0 },
            { ID_ILBM, ID_FORM, 0, MAKE_ID('A','B','C',' '), 0, 0 },
            { ID_ILBM, ID_FORM, 0, MAKE_ID('a','b','c','d'), 0, 0 },
            { ID_ILBM, ID_FORM, 0, MAKE_ID(' ',' ',' ',' '), 0, 0 },
            { ID_ILBM, ID_FORM, MAKE_ID('i','l','b','m'), ID_FORM, 0, 0 },
            { ID_ILBM, ID_FORM, ID_ILBM, ID_LIST, 0, 0 },
            { ID_ILBM, ID_FORM, ID_ILBM, ID_CAT, 0, 0 },
            { ID_ILBM, ID_FORM, ID_8SVX, ID_FORM, 0, 0 },
            { ID_ILBM, ID_FORM, 0, ID_BODY, 0, ID_DATA },
            { ID_ILBM, ID_FORM, 0, ID_BODY, ID_ILBM, ID_FORM },
            { ID_ILBM, ID_LIST, 0, ID_BMHD, 0, 0 },
            { ID_ILBM, ID_LIST, MAKE_ID('a','b','c','d'), ID_PROP, 0, 0 },
            { ID_ILBM, ID_LIST, ID_8SVX, ID_PROP, 0, 0 },
            { ID_ILBM, ID_LIST, ID_ILBM, ID_PROP, 0, ID_BMHD },
            { ID_ILBM, ID_LIST, ID_ILBM, ID_PROP, ID_ILBM, ID_PROP },
            { ID_ILBM, ID_LIST, ID_ILBM, ID_PROP, ID_ILBM, ID_LIST },
            { ID_ILBM, ID_CAT, ID_ILBM, ID_PROP, 0, 0 },
            { ID_ILBM, ID_CAT, 0, ID_BMHD, 0, 0 },
            { ID_ILBM, ID_CAT, ID_8SVX, ID_LIST, ID_8SVX, ID_PROP },
        };
        int k, j;
        for (k = 0; k < (int)(sizeof(cases) / sizeof(cases[0])); k++) {
            iff = wopen("case", IFFF_FSEEK);
            for (j = 0; j < 6; j += 2)
                if (cases[k][j + 1])
                    push(iff, cases[k][j], cases[k][j + 1], IFFSIZE_UNKNOWN);
            wbytes(iff, "zz", 2);
            pop(iff);
            pop(iff);
            pop(iff);
            wclose("case", iff);
        }
    }
    P_SECTION("header past known size");
    iff = wopen("hdr", IFFF_RSEEK);
    push(iff, ID_ILBM, ID_FORM, 10);
    push(iff, 0, ID_BODY, 0);
    pop(iff);
    push(iff, 0, ID_BODY, 0);
    wbytes(iff, "q", 1);
    pop(iff);
    pop(iff);
    wclose("hdr", iff);
    iff = wopen("hdr2", IFFF_RSEEK);
    push(iff, ID_ILBM, ID_FORM, 16);
    push(iff, 0, ID_BODY, 6);
    wbytes(iff, "123", 3);
    pop(iff);
    pop(iff);
    wclose("hdr2", iff);

    P_SECTION("list-content");
    iff = wopen("list-content", IFFF_RSEEK);
    push(iff, ID_ILBM, ID_LIST, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_BMHD, 2);                       /* leaf directly in LIST */
    push(iff, ID_ILBM, ID_PROP, IFFSIZE_UNKNOWN);
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);   /* FORM in PROP */
    push(iff, 0, ID_BMHD, 2);
    wbytes(iff, "zz", 2);
    pop(iff);
    pop(iff);
    push(iff, MAKE_ID('8','S','V','X'), ID_LIST, IFFSIZE_UNKNOWN);
    push(iff, MAKE_ID('8','S','V','X'), ID_CAT, IFFSIZE_UNKNOWN);
    pop(iff);
    pop(iff);
    pop(iff);
    wclose("list-content", iff);

    P_SECTION("cat-content");
    iff = wopen("cat-content", IFFF_RSEEK);
    push(iff, MAKE_ID(' ',' ',' ',' '), ID_CAT, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_BMHD, 2);                       /* leaf directly in CAT */
    push(iff, ID_ILBM, ID_PROP, IFFSIZE_UNKNOWN);   /* PROP in CAT */
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);
    pop(iff);
    push(iff, MAKE_ID('8','S','V','X'), ID_FORM, IFFSIZE_UNKNOWN);
    pop(iff);
    pop(iff);
    wclose("cat-content", iff);

    P_SECTION("write-errors");
    iff = wopen("werr1", IFFF_RSEEK);
    ms_fail(&ms, IFFCMD_WRITE, 1, 42);
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);
    ms_fail(&ms, IFFCMD_WRITE, 3, -77);
    push(iff, 0, ID_BODY, IFFSIZE_UNKNOWN);
    wbytes(iff, "abcd", 4);
    wbytes(iff, "efgh", 4);
    ms_fail(&ms, IFFCMD_SEEK, 1, 5);
    pop(iff);
    pop(iff);
    pop(iff);
    wclose("werr1", iff);

    iff = wopen("werr2", IFFF_RSEEK);
    push(iff, ID_ILBM, ID_FORM, 12);
    push(iff, 0, ID_BODY, 4);
    ms_fail(&ms, IFFCMD_WRITE, 3, 1);
    wbytes(iff, "abcd", 4);
    pop(iff);
    pop(iff);
    wclose("werr2", iff);

    iff = wopen("werr-seek", IFFF_RSEEK);
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_BODY, IFFSIZE_UNKNOWN);
    wbytes(iff, "ab", 2);
    ms_fail(&ms, IFFCMD_SEEK, 1, 9);
    pop(iff);
    pop(iff);
    push(iff, 0, ID_BODY, IFFSIZE_UNKNOWN);
    pop(iff);
    pop(iff);
    wclose("werr-seek", iff);

    iff = wopen("werr-flush", 0);
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_BODY, IFFSIZE_UNKNOWN);
    wbytes(iff, "ab", 2);
    pop(iff);
    ms_fail(&ms, IFFCMD_WRITE, 3, 8);
    pop(iff);
    pop(iff);
    wclose("werr-flush", iff);

    iff = wopen("werr-body", IFFF_RSEEK);
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_BODY, IFFSIZE_UNKNOWN);
    ms_fail(&ms, IFFCMD_WRITE, 4, 8);
    wbytes(iff, "ab", 2);
    wbytes(iff, "cd", 2);
    pop(iff);
    pop(iff);
    wclose("werr-body", iff);

    P_SECTION("close with open chunks");
    iff = wopen("rseek", IFFF_RSEEK);
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_BODY, IFFSIZE_UNKNOWN);
    wbytes(iff, "abc", 3);
    wclose("rseek", iff);
    iff = wopen("noseek", 0);
    push(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN);
    push(iff, 0, ID_BODY, IFFSIZE_UNKNOWN);
    wbytes(iff, "abc", 3);
    wclose("noseek", iff);
    iff = wopen("known", IFFF_RSEEK);
    push(iff, ID_ILBM, ID_FORM, 16);
    push(iff, 0, ID_BODY, 4);
    wbytes(iff, "abc", 3);
    wclose("known", iff);

    P_SECTION("init-fail");
    {
        LONG rc;
        iff = AllocIFF();
        ms_reset(&ms);
        ms_fail(&ms, IFFCMD_INIT, 1, 1234);
        iff->iff_Stream = (ULONG)&ms;
        InitIFF(iff, IFFF_FSEEK, &ip_streamhook);
        rc = OpenIFF(iff, IFFF_WRITE);
        probe_s("OpenIFF with failing INIT = ");
        ip_err(rc);
        probe_s(" flags=");
        probe_hex(iff->iff_Flags, 8);
        probe_ch('\n');
        ip_flushlog();
        FreeIFF(iff);

    }

    P_SECTION("flags");
    {
        iff = AllocIFF();
        P_HEX("AllocIFF flags", iff->iff_Flags);
        P_LONG("AllocIFF depth", iff->iff_Depth);
        P_HEX("AllocIFF stream", iff->iff_Stream);
        InitIFF(iff, 0xffffffff, &ip_streamhook);
        P_HEX("InitIFF(~0) flags", iff->iff_Flags);
        InitIFF(iff, IFFF_WRITE | IFFF_FSEEK, &ip_streamhook);
        P_HEX("InitIFF(WRITE|FSEEK) flags", iff->iff_Flags);
        iff->iff_Flags |= 0x00010000;
        InitIFF(iff, IFFF_RSEEK, &ip_streamhook);
        P_HEX("InitIFF(RSEEK) after reserved bit", iff->iff_Flags);
        ms_reset(&ms);
        iff->iff_Stream = (ULONG)&ms;
        P_LONG("OpenIFF(WRITE)", OpenIFF(iff, IFFF_WRITE));
        P_HEX("flags", iff->iff_Flags);
        CloseIFF(iff);
        P_HEX("flags after CloseIFF", iff->iff_Flags);
        P_LONG("OpenIFF(READ)", OpenIFF(iff, IFFF_READ));
        P_HEX("flags", iff->iff_Flags);
        CloseIFF(iff);
        P_LONG("OpenIFF(0x1235)", OpenIFF(iff, 0x1235));
        P_HEX("flags", iff->iff_Flags);
        CloseIFF(iff);
        ip_flushlog();
        FreeIFF(iff);
    }

    dosfile();

    P_SECTION("null");
    P_LONG("OpenIFF(NULL)", OpenIFF(NULL, IFFF_WRITE));
    CloseIFF(NULL);
    FreeIFF(NULL);
    P_STR("CloseIFF/FreeIFF(NULL)", "ok");

    CloseLibrary(IFFParseBase);
    return 0;
}
