/*
 * Probe (Phase 222f): iffparse.library clipboard streams - OpenClipboard,
 * InitIFFasClip, writing an FTXT clip and reading it back with ParseIFF,
 * CloseClipboard (incl. NULL).  Uses PRIMARY_CLIP: lxa's clipboard.device
 * opens only unit 0 so far (Phase 255).
 */
#include <devices/clipboard.h>
#include "iffprobe.h"

#define UNIT PRIMARY_CLIP

int main(void)
{
    struct ClipboardHandle *ch;
    struct IFFHandle *iff;
    struct ContextNode *cn;
    UBYTE buf[32];
    LONG rc;
    int i;

    if (!ip_open())
        return 20;

    ch = OpenClipboard(UNIT);
    P_NULL("OpenClipboard", ch);
    if (!ch)
        return 0;
    P_LONG("cbh_Req.io_Error", ch->cbh_Req.io_Error);

    iff = AllocIFF();
    iff->iff_Stream = (ULONG)ch;
    InitIFFasClip(iff);
    P_HEX("flags after InitIFFasClip", iff->iff_Flags);

    P_SECTION("write");
    P_LONG("OpenIFF", OpenIFF(iff, IFFF_WRITE));
    P_LONG("Push FORM", PushChunk(iff, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN));
    P_LONG("Push CHRS", PushChunk(iff, 0, ID_CHRS, IFFSIZE_UNKNOWN));
    P_LONG("Write", WriteChunkBytes(iff, "Hello clip", 10));
    P_LONG("Write", WriteChunkBytes(iff, "!", 1));
    P_LONG("Pop CHRS", PopChunk(iff));
    P_LONG("Push ANNO", PushChunk(iff, 0, ID_ANNO, 3));
    P_LONG("Write", WriteChunkBytes(iff, "xyz", 3));
    P_LONG("Pop ANNO", PopChunk(iff));
    P_LONG("Pop FORM", PopChunk(iff));
    CloseIFF(iff);

    P_SECTION("read");
    P_LONG("OpenIFF", OpenIFF(iff, IFFF_READ));
    P_LONG("StopChunk", StopChunk(iff, ID_FTXT, ID_CHRS));
    P_LONG("PropChunk", PropChunk(iff, ID_FTXT, ID_ANNO));
    P_LONG("StopOnExit", StopOnExit(iff, ID_FTXT, ID_FORM));
    for (i = 0; i < 6; i++) {
        rc = ParseIFF(iff, IFFPARSE_SCAN);
        probe_s("ParseIFF = ");
        ip_err(rc);
        probe_s(" top=");
        cn = CurrentChunk(iff);
        ip_cn(cn);
        probe_ch('\n');
        if (rc == 0 && cn && cn->cn_ID == ID_CHRS) {
            LONG n = ReadChunkBytes(iff, buf, sizeof(buf));
            P_LONG("  ReadChunkBytes", n);
            if (n > 0)
                P_BYTES("  data", buf, n);
        }
        if (rc == IFFERR_EOC) {
            struct StoredProperty *sp = FindProp(iff, ID_FTXT, ID_ANNO);
            P_LONG("  FindProp ANNO size", sp ? sp->sp_Size : -1);
        }
        if (rc != 0 && rc != IFFERR_EOC)
            break;
    }
    CloseIFF(iff);

    P_SECTION("read again");
    P_LONG("OpenIFF", OpenIFF(iff, IFFF_READ));
    for (i = 0; i < 12; i++) {
        rc = ParseIFF(iff, IFFPARSE_RAWSTEP);
        probe_s("raw = ");
        ip_err(rc);
        probe_s(" top=");
        ip_cn(CurrentChunk(iff));
        probe_ch('\n');
        if (rc != 0 && rc != IFFERR_EOC)
            break;
    }
    CloseIFF(iff);
    FreeIFF(iff);

    CloseClipboard(ch);
    CloseClipboard(NULL);
    P_STR("CloseClipboard", "done");

    CloseLibrary(IFFParseBase);
    return 0;
}
