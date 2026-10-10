/*
 * Probe: commodities.library InvertKeyMap() - which InputEvent fields it
 * writes for plain, shifted, control, dead-key composed and unmappable
 * characters with the default keymap (NULL and AskKeyMapDefault()).
 * Output compared with AmigaOS 3.1.
 */
#include <devices/keymap.h>
#include <clib/keymap_protos.h>
#include <inline/keymap.h>
#include "cxprobe.h"

struct Library *KeymapBase;

static const UBYTE chars[] = {
    'a', 'A', 'z', '1', '!', '@', ' ', '\n', '\r', '\t', 0x08, 0x1b, 0x7f,
    0x03, 0x01, '~', '`', '[', '{', 0xe4, 0xc4, 0xdf, 0xe1, 0xe9, 0xf1,
    0xb4, 0xa7, 0x80, 0x9b, 0xa0, 0xff, 0x00
};
#define NCHARS (sizeof(chars) / sizeof(chars[0]))

static struct InputEvent ev;

static void inv(ULONG c, struct KeyMap *km)
{
    UBYTE *p = (UBYTE *)&ev;
    struct timeval before;
    BOOL r;
    int i;

    for (i = 0; i < (int)sizeof(ev); i++)
        p[i] = 0xee;
    before = ev.ie_TimeStamp;
    r = InvertKeyMap(c, &ev, km);
    probe_s("char ");
    probe_hex(c, 6);
    probe_s(r ? " = TRUE" : " = FALSE");
    probe_s(" next=");
    probe_s(ev.ie_NextEvent == (struct InputEvent *)0xeeeeeeee ? "kept" :
            (ev.ie_NextEvent ? "changed" : "NULL"));
    probe_s(" class=");
    probe_hex(ev.ie_Class, 2);
    probe_s(" sub=");
    probe_hex(ev.ie_SubClass, 2);
    probe_s(" code=");
    probe_hex(ev.ie_Code, 4);
    probe_s(" qual=");
    probe_hex(ev.ie_Qualifier, 4);
    probe_s(" prev=");
    probe_hex(ev.ie_Prev1DownCode, 2);
    probe_s("/");
    probe_hex(ev.ie_Prev1DownQual, 2);
    probe_s(",");
    probe_hex(ev.ie_Prev2DownCode, 2);
    probe_s("/");
    probe_hex(ev.ie_Prev2DownQual, 2);
    probe_s(" time=");
    probe_s(ev.ie_TimeStamp.tv_secs == before.tv_secs &&
            ev.ie_TimeStamp.tv_micro == before.tv_micro ? "kept" : "changed");
    probe_ch('\n');
}

int main(void)
{
    unsigned i;
    struct KeyMap *km;

    if (!cx_open())
        return 20;
    KeymapBase = OpenLibrary((STRPTR)"keymap.library", 37);
    if (!KeymapBase)
        return 20;
    km = AskKeyMapDefault();

    P_SECTION("InvertKeyMap, km NULL");
    for (i = 0; i < NCHARS; i++)
        inv(chars[i], NULL);
    P_SECTION("InvertKeyMap, default keymap");
    for (i = 0; i < NCHARS; i += 3)
        inv(chars[i], km);
    P_SECTION("InvertKeyMap, high bits of ansiCode");
    inv(0x161, NULL);
    inv(0x12341, NULL);
    P_SECTION("done");

    CloseLibrary(KeymapBase);
    CloseLibrary(CxBase);
    return 0;
}
