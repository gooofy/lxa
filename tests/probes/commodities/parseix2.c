/*
 * Probe: more commodities.library ParseIX() ground truth - key names and
 * qualifier synonyms not covered by parseix, class keywords in other
 * positions, negation of classes and keys, ParseIX(NULL), MatchIX() with
 * input expressions of other versions and filters made from empty
 * strings.  Output compared with AmigaOS 3.1.
 */
#include "cxprobe.h"

static const char *const descs[] = {
    "page_down", "pagedown", "pageup", "page_up", "cursor_left", "cursor_right",
    "left_command a", "right_command a", "lcommand", "rcommand", "lamiga", "ramiga",
    "pause", "print", "prtscr", "scroll_lock", "num_lock", "numlock",
    "ctrl rawkey a", "lshift rawkey a", "rawkey timer", "f1 ctrl", "newprefs a",
    "rawmouse upstroke mouse_leftpress", "rawmouse mouse_leftup", "rawmouse mouse_nopress",
    "rawmouse mouse_middlepress upstroke", "-rawkey a", "-timer", "-f1", "ctrl -f1",
    "upstroke -upstroke a", "-upstroke upstroke a", "keypad 1", "kp 1", "num_pad 1",
    "numericpad enter", "numpad return", "numpad f1", "numpad (", "numpad 1 ", "numpad -1",
    "lalt alt a", "ralt -alt a", "alt -lalt a", "caps -shift a", "shift -caps a",
    "capslock caps a", "B", "Z", "ctrl Q", "~", "`", "|", "\\", "'", "\"", ";", ":",
    "<", ">", "?", "=", "_", "^", "&", "%", "$", "#", "!", "@", "[", "]", "{", "}",
    "\t", "a\t", "\ta", ",a", ",,a", "ctrl,,a", "ctrl , a", "ctrl\t\ta",
    "help ", "help,", "help\t", "f1\n", "esc x", "rawkey", "rawkey ", "rawmouse ",
    "timer ", "event ", "diskinserted ctrl", "pointerpos lbutton", "newprefs -ctrl",
    "rawkey upstroke", "rawkey -upstroke", "rawkey repeat", "rawkey -repeat",
    "relativemouse", "-relativemouse", "lbutton", "-lbutton", "rawmouse -lbutton",
    "f1 f2", "f1 a", "a f1", "ctrl f1 f2", "upstroke upstroke a",
    "space", "spacebar", "comma", "ctrl comma", "ctrl space", "backspace", "delete",
    "insert", "home", "end", "break", "help", "f11", "f12", "up", "down",
    "mouse_leftpress", "rawmouse mouse_rightpress", "rawmouse f1", "rawmouse esc",
    "rawmouse ctrl mouse_leftpress", "timer lbutton", "timer -lbutton",
    "Rawkey CTRL A", "RAWMOUSE MOUSE_LEFTPRESS", "TIMER", "NumPad 1", "LALT RALT A",
    "upstroke ctrl a", "upstroke shift", "timer upstroke", "rawmouse upstroke",
    "-upstroke ctrl a", "numpad upstroke 1", "upstroke numpad 1", "\xa0", "a\xa0",
    "ctrl\xa0" "a", "\x7f", "\x01", "a\x01", "ctrl\x01" "a", "f1\x01", "\x1f", "ctrl\x1f" "a",
    "mid_button a", "middle a", "commodore a", "lcommand -rcommand a", "ctrl -numpad 1",
    "-numpad 1", "numpad -numpad 1", "\xe4\xe4", "ctrl a\x01", "ctrl f1 ",
    NULL
};

static IX ix;

static void probe_esc(const char *d)
{
    const UBYTE *u = (const UBYTE *)d;
    for (; *u; u++) {
        if (*u < 0x20 || *u >= 0x7f || *u == '"' || *u == '\\') {
            probe_s("\\x");
            probe_ch("0123456789abcdef"[*u >> 4]);
            probe_ch("0123456789abcdef"[*u & 15]);
        } else
            probe_ch(*u);
    }
}

static void show_ix(LONG r)
{
    probe_dec(r);
    probe_s(" v=");
    probe_dec(ix.ix_Version);
    probe_s(" class=");
    probe_hex(ix.ix_Class, 2);
    probe_s(" code=");
    probe_hex(ix.ix_Code, 4);
    probe_s("/");
    probe_hex(ix.ix_CodeMask, 4);
    probe_s(" qual=");
    probe_hex(ix.ix_Qualifier, 4);
    probe_s("/");
    probe_hex(ix.ix_QualMask, 4);
    probe_s(" same=");
    probe_hex(ix.ix_QualSame, 4);
    probe_ch('\n');
}

static void fill(UBYTE v)
{
    UBYTE *p = (UBYTE *)&ix;
    int i;
    for (i = 0; i < (int)sizeof(ix); i++)
        p[i] = v;
}

static void parse(const char *d)
{
    LONG r;
    fill(0xee);
    ix.ix_Version = IX_VERSION;
    r = ParseIX((STRPTR)d, &ix);
    probe_ch('"');
    probe_esc(d);
    probe_s("\" = ");
    show_ix(r);
}

static struct InputEvent ev;

static void match_ver(UBYTE ver)
{
    ix.ix_Version = ver;
    ix.ix_Class = IECLASS_RAWKEY;
    ix.ix_Code = 0x20;
    ix.ix_CodeMask = 0xffff;
    ix.ix_Qualifier = 0;
    ix.ix_QualMask = 0;
    ix.ix_QualSame = 0;
    ev.ie_NextEvent = NULL;
    ev.ie_Class = IECLASS_RAWKEY;
    ev.ie_Code = 0x20;
    ev.ie_Qualifier = 0;
    probe_s("MatchIX ix_Version ");
    probe_dec(ver);
    probe_s(MatchIX(&ev, &ix) ? " = TRUE\n" : " = FALSE\n");
}

static void show(const char *label, CxObj *co)
{
    probe_s(label);
    if (!co) {
        probe_s(": NULL\n");
        return;
    }
    probe_s(": type=");
    probe_dec(CxObjType(co));
    probe_s(" error=");
    probe_dec(CxObjError(co));
    probe_ch('\n');
}

int main(void)
{
    int i;
    LONG r;
    CxObj *co;

    if (!cx_open())
        return 20;

    P_SECTION("ParseIX");
    for (i = 0; descs[i]; i++)
        parse(descs[i]);

    P_SECTION("ParseIX(NULL)");
    fill(0xee);
    ix.ix_Version = IX_VERSION;
    r = ParseIX(NULL, &ix);
    probe_s("NULL = ");
    show_ix(r);

    P_SECTION("ParseIX with other ix_Version");
    fill(0xee);
    ix.ix_Version = 1;
    r = ParseIX((STRPTR)"ctrl a", &ix);
    probe_s("v1 \"ctrl a\" = ");
    show_ix(r);
    fill(0);
    r = ParseIX((STRPTR)"ctrl a", &ix);
    probe_s("v0 \"ctrl a\" = ");
    show_ix(r);

    P_SECTION("MatchIX versions");
    match_ver(0);
    match_ver(1);
    match_ver(2);
    match_ver(3);

    P_SECTION("filters");
    co = CxFilter((STRPTR)"");
    show("CxFilter(\"\")", co);
    SetFilter(co, (STRPTR)" ");
    show("SetFilter(\" \")", co);
    SetFilter(co, (STRPTR)"ctrl a b");
    show("SetFilter(\"ctrl a b\")", co);
    SetFilter(co, (STRPTR)"numpad a");
    show("SetFilter(\"numpad a\")", co);
    SetFilter(co, (STRPTR)"rawkey");
    show("SetFilter(\"rawkey\")", co);
    DeleteCxObj(co);

    P_SECTION("done");

    CloseLibrary(CxBase);
    return 0;
}
