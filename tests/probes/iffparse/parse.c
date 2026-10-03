/*
 * Probe (Phase 222f): iffparse.library reading - ParseIFF in SCAN, STEP
 * and RAWSTEP mode over well-formed and malformed files (every event with
 * the context stack and the stream-hook calls), the three seek modes,
 * ReadChunkBytes/ReadChunkRecords (incl. past the chunk end), PushChunk and
 * PopChunk in read mode, stream read/seek errors and InitIFFasDOS.
 */
#include "iffprobe.h"

static struct MemStream ms;

static struct IFFHandle *ropen(const char *label, LONG flags)
{
    struct IFFHandle *iff = AllocIFF();
    LONG rc;
    ms_rewind(&ms);
    iff->iff_Stream = (ULONG)&ms;
    InitIFF(iff, flags, &ip_streamhook);
    rc = OpenIFF(iff, IFFF_READ);
    ip_state(label, iff, rc);
    return iff;
}

static void rclose(struct IFFHandle *iff)
{
    CloseIFF(iff);
    ip_state("close", iff, 0);
    FreeIFF(iff);
}

/* parse to the end (or max events), printing every event */
static void run(struct IFFHandle *iff, LONG control, int max)
{
    static const char *const mode[] = { "scan", "step", "raw" };
    LONG rc;
    int i;
    for (i = 0; i < max; i++) {
        rc = ParseIFF(iff, control);
        ip_state(mode[control], iff, rc);
        if (rc != 0 && rc != IFFERR_EOC)
            break;
    }
}

/* FORM ILBM { BMHD(4) CMAP(3+pad) BODY(6) } */
static void build_ilbm(void)
{
    ms_reset(&ms);
    ms_id(&ms, ID_FORM); ms_id(&ms, 42); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BMHD); ms_id(&ms, 4); ms_str(&ms, "bmhd", 4);
    ms_id(&ms, ID_CMAP); ms_id(&ms, 3); ms_str(&ms, "rgb", 3); ms_pad(&ms);
    ms_id(&ms, ID_BODY); ms_id(&ms, 6); ms_str(&ms, "body01", 6);
}

/* printable characters as they are, others as '~' */
static void pchar(UBYTE c)
{
    probe_ch((c >= 0x20 && c < 0x7f) ? (char)c : '~');
}

static void readbytes(struct IFFHandle *iff, LONG n)
{
    UBYTE buf[32];
    LONG rc;
    int i;
    for (i = 0; i < 32; i++)
        buf[i] = '.';
    rc = ReadChunkBytes(iff, buf, n);
    probe_s("ReadBytes ");
    probe_dec(n);
    probe_s(" -> ");
    ip_err(rc);
    probe_s(" \"");
    for (i = 0; i < (rc > 0 ? rc : 0) && i < 32; i++)
        pchar(buf[i]);
    probe_ch('"');
    ip_state("", iff, rc);
}

static void readrecs(struct IFFHandle *iff, LONG size, LONG n)
{
    UBYTE buf[32];
    LONG rc;
    int i;
    for (i = 0; i < 32; i++)
        buf[i] = '.';
    rc = ReadChunkRecords(iff, buf, size, n);
    probe_s("ReadRecs ");
    probe_dec(size);
    probe_ch('x');
    probe_dec(n);
    probe_s(" -> ");
    ip_err(rc);
    probe_s(" \"");
    for (i = 0; i < 16; i++)
        pchar(buf[i]);
    probe_ch('"');
    ip_state("", iff, rc);
}

static void malformed(const char *label, LONG control)
{
    struct IFFHandle *iff = ropen(label, IFFF_RSEEK);
    run(iff, control, 20);
    rclose(iff);
}

int main(void)
{
    struct IFFHandle *iff;

    if (!ip_open())
        return 20;

    P_SECTION("rawstep rseek");
    build_ilbm();
    iff = ropen("open", IFFF_RSEEK);
    run(iff, IFFPARSE_RAWSTEP, 20);
    run(iff, IFFPARSE_RAWSTEP, 2);
    rclose(iff);

    P_SECTION("rawstep fseek");
    iff = ropen("open", IFFF_FSEEK);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);

    P_SECTION("rawstep noseek");
    iff = ropen("open", 0);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);

    P_SECTION("step");
    iff = ropen("open", IFFF_RSEEK);
    run(iff, IFFPARSE_STEP, 20);
    rclose(iff);

    P_SECTION("scan");
    iff = ropen("open", IFFF_RSEEK);
    run(iff, IFFPARSE_SCAN, 3);
    run(iff, IFFPARSE_SCAN, 1);
    rclose(iff);

    P_SECTION("read in chunks");
    iff = ropen("open", IFFF_RSEEK);
    readbytes(iff, 4);                  /* before any ParseIFF */
    run(iff, IFFPARSE_RAWSTEP, 1);      /* FORM */
    readbytes(iff, 4);                  /* reading a FORM */
    run(iff, IFFPARSE_RAWSTEP, 1);      /* BMHD */
    readbytes(iff, 1);
    readbytes(iff, 2);
    readbytes(iff, 5);
    readbytes(iff, 1);
    readbytes(iff, 0);
    run(iff, IFFPARSE_RAWSTEP, 2);      /* EOC BMHD, CMAP */
    readrecs(iff, 2, 5);
    readrecs(iff, 2, 1);
    readbytes(iff, 10);
    readrecs(iff, 2, 1);
    run(iff, IFFPARSE_RAWSTEP, 2);      /* EOC CMAP, BODY */
    readrecs(iff, 4, 1);
    readrecs(iff, 0, 3);
    readrecs(iff, 3, 0);
    readrecs(iff, 1, 1);
    readbytes(iff, -1);
    run(iff, IFFPARSE_RAWSTEP, 1);      /* EOC BODY */
    readbytes(iff, 1);
    run(iff, IFFPARSE_RAWSTEP, 1);      /* EOC FORM */
    readbytes(iff, 1);
    run(iff, IFFPARSE_RAWSTEP, 2);
    rclose(iff);

    P_SECTION("noseek partial read");
    iff = ropen("open", 0);
    run(iff, IFFPARSE_RAWSTEP, 2);
    readbytes(iff, 1);
    run(iff, IFFPARSE_RAWSTEP, 3);
    readbytes(iff, 3);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);

    P_SECTION("pushchunk in read mode");
    iff = ropen("open", IFFF_RSEEK);
    ip_state("PushChunk", iff, PushChunk(iff, 0, 0, 0));
    ip_state("PushChunk", iff, PushChunk(iff, 0, 0, 0));
    readbytes(iff, 4);
    ip_state("PopChunk", iff, PopChunk(iff));
    ip_state("PushChunk", iff, PushChunk(iff, 0, 0, 0));
    ip_state("PopChunk", iff, PopChunk(iff));
    ip_state("PopChunk", iff, PopChunk(iff));
    ip_state("PopChunk", iff, PopChunk(iff));
    rclose(iff);

    iff = ropen("open", IFFF_RSEEK);
    run(iff, IFFPARSE_RAWSTEP, 1);
    ip_state("PushChunk in FORM", iff, PushChunk(iff, 0, 0, 0));
    readbytes(iff, 2);
    ip_state("PopChunk", iff, PopChunk(iff));
    ip_state("PushChunk", iff, PushChunk(iff, 0, 0, 0));
    run(iff, IFFPARSE_RAWSTEP, 3);
    ip_state("PopChunk mid-chunk", iff, PopChunk(iff));
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);

    P_SECTION("big chunk");
    ms_reset(&ms);
    ms_id(&ms, ID_FORM); ms_id(&ms, 4 + 8 + 700 + 8 + 2); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 700);
    {
        int k;
        for (k = 0; k < 700; k++)
            ms.data[ms.len++] = (UBYTE)k;
    }
    ms_id(&ms, ID_CMAP); ms_id(&ms, 2); ms_str(&ms, "zz", 2);
    iff = ropen("noseek", 0);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);
    iff = ropen("noseek partial", 0);
    run(iff, IFFPARSE_RAWSTEP, 2);
    readbytes(iff, 3);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);
    iff = ropen("fseek", IFFF_FSEEK);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);
    iff = ropen("noseek close", 0);
    run(iff, IFFPARSE_RAWSTEP, 2);
    rclose(iff);

    build_ilbm();
    P_SECTION("write in read mode");
    iff = ropen("open", IFFF_RSEEK);
    run(iff, IFFPARSE_RAWSTEP, 2);
    ip_state("WriteChunkBytes", iff, WriteChunkBytes(iff, "xx", 2));
    ip_state("WriteChunkRecords", iff, WriteChunkRecords(iff, "xx", 1, 2));
    rclose(iff);

    P_SECTION("read errors");
    iff = ropen("open", IFFF_RSEEK);
    ms_fail(&ms, IFFCMD_READ, 3, 99);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);
    iff = ropen("open", IFFF_RSEEK);
    ms_fail(&ms, IFFCMD_SEEK, 1, -3);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);
    iff = ropen("open", IFFF_RSEEK);
    run(iff, IFFPARSE_RAWSTEP, 2);
    ms_fail(&ms, IFFCMD_READ, 4, 1);
    readbytes(iff, 2);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);
    iff = ropen("open", 0);
    ms_fail(&ms, IFFCMD_READ, 5, 7);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);

    P_SECTION("nested");
    /* LIST ILBM { PROP ILBM { BMHD(2) } FORM ILBM { BODY(1) }
     *             CAT 8SVX { FORM 8SVX { BODY(2) } } } */
    ms_reset(&ms);
    ms_id(&ms, ID_LIST); ms_id(&ms, 4 + 22 + 22 + 34); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_PROP); ms_id(&ms, 14); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BMHD); ms_id(&ms, 2); ms_str(&ms, "pp", 2);
    ms_id(&ms, ID_FORM); ms_id(&ms, 14); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 1); ms_str(&ms, "b", 1); ms_pad(&ms);
    ms_id(&ms, ID_CAT); ms_id(&ms, 26); ms_id(&ms, ID_8SVX);
    ms_id(&ms, ID_FORM); ms_id(&ms, 14); ms_id(&ms, ID_8SVX);
    ms_id(&ms, ID_BODY); ms_id(&ms, 2); ms_str(&ms, "s1", 2);
    iff = ropen("open", IFFF_RSEEK);
    run(iff, IFFPARSE_RAWSTEP, 40);
    rclose(iff);
    iff = ropen("open", 0);
    run(iff, IFFPARSE_STEP, 40);
    rclose(iff);

    P_SECTION("two top-level forms");
    {
        LONG first;
        build_ilbm();
        first = ms.len;
        ms_id(&ms, ID_FORM); ms_id(&ms, 12); ms_id(&ms, ID_TEST);
        ms_id(&ms, ID_DATA); ms_id(&ms, 0);
        iff = ropen("open", IFFF_RSEEK);
        run(iff, IFFPARSE_SCAN, 3);
        P_LONG("stream pos", ms.pos);
        rclose(iff);
        (void)first;
    }

    P_SECTION("malformed");
    ms_reset(&ms);
    malformed("empty", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_str(&ms, "FORM", 4); ms_str(&ms, "\0\0", 2);
    malformed("trunc-size", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 4);
    malformed("trunc-type", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 4); ms_id(&ms, ID_ILBM);
    malformed("empty-form", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 0); ms_id(&ms, ID_ILBM);
    malformed("form-size0", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 3); ms_id(&ms, ID_ILBM);
    malformed("form-size3", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 0xfffffff0); ms_id(&ms, ID_ILBM);
    malformed("form-negsize", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_str(&ms, "ABCD", 4); ms_id(&ms, 4); ms_str(&ms, "xxxx", 4);
    malformed("not-iff", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_PROP); ms_id(&ms, 4); ms_id(&ms, ID_ILBM);
    malformed("top-prop", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_str(&ms, "This is plain text, not an IFF file.", 36);
    malformed("text", IFFPARSE_SCAN);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 12); ms_str(&ms, "ilbm", 4);
    ms_id(&ms, ID_BODY); ms_id(&ms, 0);
    malformed("bad-type", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 12); ms_str(&ms, " ABC", 4);
    ms_id(&ms, ID_BODY); ms_id(&ms, 0);
    malformed("space-type", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 12); ms_id(&ms, ID_ILBM);
    ms_str(&ms, "B\001DY", 4); ms_id(&ms, 0);
    malformed("bad-id", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 20); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 100); ms_str(&ms, "12345678", 8);
    malformed("chunk-too-big", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 30); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 6); ms_str(&ms, "123456", 6);
    malformed("truncated-file", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 30); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 6); ms_str(&ms, "123456", 6);
    malformed("truncated-file scan", IFFPARSE_SCAN);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 17); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 5); ms_str(&ms, "12345", 5); ms_pad(&ms);
    malformed("odd-form", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 14); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 1); ms_str(&ms, "1", 1);
    ms_id(&ms, ID_DATA); ms_id(&ms, 0);
    malformed("form-ends-mid-pad", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 10); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 0);
    malformed("form-ends-mid-header", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 16); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_FORM); ms_id(&ms, 4); ms_id(&ms, ID_TEST);
    malformed("form-in-form", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 16); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_PROP); ms_id(&ms, 4); ms_id(&ms, ID_ILBM);
    malformed("prop-in-form", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_LIST); ms_id(&ms, 14); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 2); ms_str(&ms, "12", 2);
    malformed("leaf-in-list", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_CAT); ms_id(&ms, 14); ms_str(&ms, "    ", 4);
    ms_id(&ms, ID_BODY); ms_id(&ms, 2); ms_str(&ms, "12", 2);
    malformed("leaf-in-cat", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_LIST); ms_id(&ms, 16); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_PROP); ms_id(&ms, 4); ms_str(&ms, "abcd", 4);
    malformed("prop-bad-type", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_LIST); ms_id(&ms, 28); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_PROP); ms_id(&ms, 16); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_FORM); ms_id(&ms, 4); ms_id(&ms, ID_ILBM);
    malformed("form-in-prop", IFFPARSE_RAWSTEP);

    P_SECTION("continue after error");
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 30); ms_id(&ms, ID_ILBM);
    ms_str(&ms, "B\001DY", 4); ms_id(&ms, 2); ms_str(&ms, "xy", 2);
    ms_id(&ms, ID_DATA); ms_id(&ms, 2); ms_str(&ms, "ab", 2);
    iff = ropen("bad-id", IFFF_RSEEK);
    run(iff, IFFPARSE_RAWSTEP, 1);
    run(iff, IFFPARSE_RAWSTEP, 1);
    run(iff, IFFPARSE_RAWSTEP, 1);
    run(iff, IFFPARSE_RAWSTEP, 1);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 34); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_PROP); ms_id(&ms, 6); ms_id(&ms, ID_ILBM); ms_str(&ms, "xy", 2);
    ms_id(&ms, ID_DATA); ms_id(&ms, 2); ms_str(&ms, "ab", 2);
    iff = ropen("prop-in-form", IFFF_RSEEK);
    run(iff, IFFPARSE_RAWSTEP, 1);
    run(iff, IFFPARSE_RAWSTEP, 1);
    run(iff, IFFPARSE_RAWSTEP, 1);
    run(iff, IFFPARSE_RAWSTEP, 1);
    run(iff, IFFPARSE_RAWSTEP, 20);
    rclose(iff);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 2); ms_id(&ms, ID_ILBM);
    malformed("form-size2", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 0x7fffffff); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 2); ms_str(&ms, "ab", 2);
    malformed("form-size-max", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 20); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 0x80000000); ms_str(&ms, "abcdefgh", 8);
    malformed("chunk-negsize", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 20); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 9); ms_str(&ms, "abcdefgh", 8);
    malformed("chunk-pad-past-end", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 20); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_BODY); ms_id(&ms, 8); ms_str(&ms, "abcdefgh", 8);
    malformed("chunk-exact", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_LIST); ms_id(&ms, 12); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_CAT); ms_id(&ms, 0);
    malformed("cat-size0-in-list", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_CAT); ms_id(&ms, 16); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_FORM); ms_id(&ms, 4); ms_id(&ms, ID_8SVX);
    malformed("cat-other-type", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_LIST); ms_id(&ms, 28); ms_id(&ms, ID_ILBM);
    ms_id(&ms, ID_PROP); ms_id(&ms, 4); ms_id(&ms, ID_8SVX);
    ms_id(&ms, ID_FORM); ms_id(&ms, 4); ms_id(&ms, ID_8SVX);
    malformed("list-other-types", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_CAT); ms_id(&ms, 4); ms_str(&ms, "    ", 4);
    malformed("cat-blank", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 4); ms_str(&ms, "    ", 4);
    malformed("form-blank", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 4); ms_str(&ms, "FORM", 4);
    malformed("form-type-form", IFFPARSE_RAWSTEP);
    ms_reset(&ms); ms_id(&ms, ID_FORM); ms_id(&ms, 4); ms_str(&ms, "AB1 ", 4);
    malformed("form-type-ab1", IFFPARSE_RAWSTEP);

    P_SECTION("dos");
    {
        BPTR fh = Open((CONST_STRPTR)"T:iffprobe_r.iff", MODE_NEWFILE);
        build_ilbm();
        Write(fh, ms.data, ms.len);
        Close(fh);
        fh = Open((CONST_STRPTR)"T:iffprobe_r.iff", MODE_OLDFILE);
        iff = AllocIFF();
        iff->iff_Stream = (ULONG)fh;
        InitIFFasDOS(iff);
        P_LONG("OpenIFF", OpenIFF(iff, IFFF_READ));
        P_LONG("pos", Seek(fh, 0, OFFSET_CURRENT));
        run(iff, IFFPARSE_RAWSTEP, 2);
        P_LONG("pos", Seek(fh, 0, OFFSET_CURRENT));
        readbytes(iff, 2);
        run(iff, IFFPARSE_RAWSTEP, 3);
        P_LONG("pos", Seek(fh, 0, OFFSET_CURRENT));
        run(iff, IFFPARSE_RAWSTEP, 20);
        P_LONG("pos", Seek(fh, 0, OFFSET_CURRENT));
        CloseIFF(iff);
        P_LONG("pos after CloseIFF", Seek(fh, 0, OFFSET_CURRENT));
        P_LONG("OpenIFF again", OpenIFF(iff, IFFF_READ));
        run(iff, IFFPARSE_SCAN, 2);
        CloseIFF(iff);
        FreeIFF(iff);
        Close(fh);
        /* a truncated DOS file */
        fh = Open((CONST_STRPTR)"T:iffprobe_r.iff", MODE_NEWFILE);
        Write(fh, ms.data, 30);
        Close(fh);
        fh = Open((CONST_STRPTR)"T:iffprobe_r.iff", MODE_OLDFILE);
        iff = AllocIFF();
        iff->iff_Stream = (ULONG)fh;
        InitIFFasDOS(iff);
        P_LONG("OpenIFF trunc", OpenIFF(iff, IFFF_READ));
        run(iff, IFFPARSE_RAWSTEP, 20);
        CloseIFF(iff);
        FreeIFF(iff);
        Close(fh);
        DeleteFile((CONST_STRPTR)"T:iffprobe_r.iff");
    }

    CloseLibrary(IFFParseBase);
    return 0;
}
