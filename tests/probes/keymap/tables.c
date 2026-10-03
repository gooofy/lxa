/*
 * Probe (Phase 222f): keymap.library default keymap contents.
 * AskKeyMapDefault() tables: key types, plain mappings, string and
 * dead-key descriptors (decoded), capsable/repeatable bitmaps, table
 * hashes; SetKeyMapDefault() round trip.  Output compared with AmigaOS 3.1.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <devices/keymap.h>
#include <clib/exec_protos.h>
#include <clib/keymap_protos.h>
#include <inline/exec.h>
#include <inline/keymap.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *KeymapBase;

static const UBYTE nstr[8] = {1, 2, 2, 4, 2, 4, 4, 8};
static LONG dead_table_size;

static void dump_key(LONG code, UBYTE type, ULONG map)
{
    LONG n = nstr[type & 7], i, j;
    const UBYTE *d = (const UBYTE *)map;

    probe_s("key ");
    probe_hex(code, 2);
    probe_s(" type ");
    probe_hex(type, 2);
    if (type & KCF_NOP) {
        probe_s(" nop");
    } else if (type & KCF_STRING) {
        probe_s(" str");
        for (i = 0; i < n; i++) {
            probe_s(" [");
            for (j = 0; j < d[2 * i]; j++) {
                if (j)
                    probe_ch(' ');
                probe_hex(d[d[2 * i + 1] + j], 2);
            }
            probe_ch(']');
        }
    } else if (type & KCF_DEAD) {
        probe_s(" dead");
        for (i = 0; i < n; i++) {
            probe_ch(' ');
            if (d[2 * i] == 0) {
                probe_hex(d[2 * i + 1], 2);
            } else if (d[2 * i] == DPF_DEAD) {
                probe_s("D");
                probe_hex(d[2 * i + 1], 2);
            } else if (d[2 * i] == DPF_MOD) {
                probe_s("M[");
                for (j = 0; j < dead_table_size; j++) {
                    if (j)
                        probe_ch(' ');
                    probe_hex(d[d[2 * i + 1] + j], 2);
                }
                probe_ch(']');
            } else {
                probe_s("?");
                probe_hex(d[2 * i], 2);
                probe_hex(d[2 * i + 1], 2);
            }
        }
    } else {
        probe_s(" map ");
        probe_hex(map, 8);
    }
    probe_ch('\n');
}

static void scan_dead(UBYTE type, ULONG map, LONG *maxidx, LONG *maxfac)
{
    LONG n = nstr[type & 7], i;
    const UBYTE *d = (const UBYTE *)map;
    if ((type & KCF_NOP) || !(type & KCF_DEAD))
        return;
    for (i = 0; i < n; i++)
        if (d[2 * i] == DPF_DEAD) {
            LONG idx = d[2 * i + 1] & DP_2DINDEXMASK, fac = d[2 * i + 1] >> DP_2DFACSHIFT;
            if (idx > *maxidx)
                *maxidx = idx;
            if (fac > *maxfac)
                *maxfac = fac;
        }
}

int main(void)
{
    struct KeyMap *km, *km2, mine;
    LONG i, maxidx = 0, maxfac = 0;

    KeymapBase = OpenLibrary((STRPTR)"keymap.library", 37);
    if (!KeymapBase)
        return 20;

    P_LONG("lib_Version", KeymapBase->lib_Version);

    km = AskKeyMapDefault();
    P_NULL("AskKeyMapDefault", km);
    if (!km)
        return 20;

    for (i = 0; i < 0x40; i++)
        scan_dead(km->km_LoKeyMapTypes[i], km->km_LoKeyMap[i], &maxidx, &maxfac);
    for (i = 0; i < 0x38; i++)
        scan_dead(km->km_HiKeyMapTypes[i], km->km_HiKeyMap[i], &maxidx, &maxfac);
    dead_table_size = (maxidx + 1) * (maxfac ? maxfac : 1);
    P_LONG("max dead index", maxidx);
    P_LONG("max double-dead factor", maxfac);

    P_SECTION("bitmaps");
    P_BYTES("LoKeyMapTypes", km->km_LoKeyMapTypes, 0x40);
    P_BYTES("HiKeyMapTypes", km->km_HiKeyMapTypes, 0x38);
    P_BYTES("LoCapsable", km->km_LoCapsable, 8);
    P_BYTES("HiCapsable", km->km_HiCapsable, 7);
    P_BYTES("LoRepeatable", km->km_LoRepeatable, 8);
    P_BYTES("HiRepeatable", km->km_HiRepeatable, 7);
    P_HEX("LoKeyMapTypes hash", probe_hash(km->km_LoKeyMapTypes, 0x40));
    P_HEX("HiKeyMapTypes hash", probe_hash(km->km_HiKeyMapTypes, 0x38));

    P_SECTION("keys");
    for (i = 0; i < 0x40; i++)
        dump_key(i, km->km_LoKeyMapTypes[i], km->km_LoKeyMap[i]);
    for (i = 0; i < 0x38; i++)
        dump_key(0x40 + i, km->km_HiKeyMapTypes[i], km->km_HiKeyMap[i]);

    P_SECTION("SetKeyMapDefault");
    mine = *km;
    SetKeyMapDefault(&mine);
    km2 = AskKeyMapDefault();
    P_BOOL("Ask after Set returns new map", km2 == &mine);
    SetKeyMapDefault(km);
    km2 = AskKeyMapDefault();
    P_BOOL("Ask after restore returns original", km2 == km);
    P_BOOL("second Ask returns same", AskKeyMapDefault() == km);

    CloseLibrary(KeymapBase);
    return 0;
}
