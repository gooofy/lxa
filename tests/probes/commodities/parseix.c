/*
 * Probe: commodities.library ParseIX() on many input description strings
 * (classes, qualifiers, synonyms, upstroke, key names, keymap characters,
 * separators, errors) and MatchIX() of input events against parsed
 * expressions.  Output compared with AmigaOS 3.1.
 */
#include "cxprobe.h"

static const char *const descs[] = {
    "", " ", "a", "A", "z", "1", "ctrl a", "control a", "CTRL A", "Ctrl  a",
    "  ctrl a", "ctrl,a", "ctrl a b", "ctrl\ta", "control alt f1",
    "rawkey lshift rshift x", "shift a", "-shift a", "caps a", "-caps a",
    "alt f10", "-alt f10", "lalt -ralt f10", "lcommand m", "ramiga space",
    "left_amiga right_amiga q", "lamiga rcommand q", "upstroke a", "-upstroke a",
    "ctrl upstroke a", "rawmouse leftbutton", "rawmouse mouse_leftpress",
    "rawmouse lbutton mouse_leftpress", "rawmouse mouse_rightpress",
    "rawmouse rbutton midbutton", "rawmouse -upstroke mouse_middlepress",
    "diskinserted", "diskremoved", "newprefs", "pointerpos", "timer", "event",
    "rawkey", "ctrl alt del", "ctrl alt delete", "help", "f11", "f12", "home",
    "end", "page_up", "insert", "numericpad 1", "numpad 5", "numeric_pad (",
    "ctrl 1", "esc", "escape", "return", "enter", "tab", "backspace", "del",
    "up", "down", "left", "right", "cursor_up", "cursor_down", "comma",
    "space", "spacebar", "break", "ctrl ,", "ctrl .", "ctrl -", "ctrl *",
    "ctrl (", "ctrl +", "ctrl /", "ctrl @", "ctrl !", "ctrl ~", "ctrl [",
    "bogus", "ctrl bogus", "bogus a", "repeat a", "relativemouse a",
    "capslock a", "caps_lock a", "lalt ralt a", "shift alt caps a",
    "left_shift right_shift left_alt right_alt a", "lbutton rbutton mbutton a",
    "leftbutton rightbutton middlebutton a", "left_button right_button a",
    "middle_button a", "interrupt a", "multibroadcast a",
    "\xe4", "ctrl \xe4", "\xdf", "\xb4", "rawkey -repeat a", "-ctrl a",
    "-lshift -rshift a", "ctrl -", "a ctrl", "ctrlx a", "ctrl ax",
    "f1x", "f", "f0", "f13", "rawkey rawkey a", "timer ctrl", "event a",
    "numpad -", "numpad .", "numpad /", "numpad *", "numpad +", "numpad )",
    "numpad 0", "numpad 9", "numpad enter", "numpad a", "numpad ,", "numericpad", 
    "rawmouse a", "rawmouse 1", "timer a", "-a", "ctrl -a", "event", "SHIFT A",
    "Upstroke a", "UPSTROKE", "ctrl upstroke", "-upstroke", "--ctrl a", "- ctrl a",
    "ctrl - a", "ctrl a,", "ctrl a ", "ctrl a\n", "lalt-ctrl a", "( ", ")", "*", "+",
    "/", ".", "0", "9", "ctrl 0", "rawkey f1", "f10", "F1", "Help", "DEL", "spacebar x",
    "mouse_leftpress", "ctrl mouse_rightpress", "rawkey mouse_middlepress",
    "spacebar ", "f1,", "ctrl ", "upstroke bogus", "rawmouse", "\x80", "\x9b",
    "rawmouse mouse_leftpress x", "numpad 5 ", "numpad 55", "ctrl ctrl a",
    "a,", "-", "shift", "-shift", "lshift -lshift a", "-lshift lshift a",
    NULL
};

static IX ix;

static void probe_esc(const char *d)
{
    const UBYTE *u = (const UBYTE *)d;
    for (; *u; u++) {
        if (*u < 0x20 || *u >= 0x7f) {
            probe_s("\\x");
            probe_ch("0123456789abcdef"[*u >> 4]);
            probe_ch("0123456789abcdef"[*u & 15]);
        } else
            probe_ch(*u);
    }
}

static void parse(const char *d)
{
    LONG r;
    UBYTE *p = (UBYTE *)&ix;
    int i;
    for (i = 0; i < (int)sizeof(ix); i++)
        p[i] = 0xee;
    ix.ix_Version = IX_VERSION;
    r = ParseIX((STRPTR)d, &ix);
    probe_ch('"');
    probe_esc(d);
    probe_s("\" = ");
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

static struct InputEvent ev;

static void match(const char *d, UBYTE cls, UWORD code, UWORD qual)
{
    ix.ix_Version = IX_VERSION;
    ParseIX((STRPTR)d, &ix);
    ev.ie_NextEvent = NULL;
    ev.ie_Class = cls;
    ev.ie_SubClass = 0;
    ev.ie_Code = code;
    ev.ie_Qualifier = qual;
    ev.ie_EventAddress = NULL;
    probe_s("MatchIX \"");
    probe_esc(d);
    probe_s("\" class=");
    probe_hex(cls, 2);
    probe_s(" code=");
    probe_hex(code, 4);
    probe_s(" qual=");
    probe_hex(qual, 4);
    probe_s(MatchIX(&ev, &ix) ? " = TRUE\n" : " = FALSE\n");
}

int main(void)
{
    int i;

    if (!cx_open())
        return 20;

    P_SECTION("ParseIX");
    for (i = 0; descs[i]; i++)
        parse(descs[i]);

    P_SECTION("MatchIX");
    match("ctrl a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CONTROL);
    match("ctrl a", IECLASS_RAWKEY, 0x20, 0);
    match("ctrl a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CONTROL | IEQUALIFIER_LSHIFT);
    match("ctrl a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CONTROL | IEQUALIFIER_REPEAT);
    match("ctrl a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CONTROL | IEQUALIFIER_LEFTBUTTON);
    match("ctrl a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CONTROL | IEQUALIFIER_NUMERICPAD);
    match("ctrl a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CONTROL | IEQUALIFIER_RELATIVEMOUSE);
    match("ctrl a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CONTROL | IEQUALIFIER_INTERRUPT);
    match("ctrl a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CONTROL | IEQUALIFIER_MULTIBROADCAST);
    match("ctrl a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CONTROL | IEQUALIFIER_CAPSLOCK);
    match("ctrl a", IECLASS_RAWKEY, 0xa0, IEQUALIFIER_CONTROL);
    match("ctrl a", IECLASS_RAWMOUSE, 0x20, IEQUALIFIER_CONTROL);
    match("ctrl a", IECLASS_RAWKEY, 0x21, IEQUALIFIER_CONTROL);
    match("shift a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_LSHIFT);
    match("shift a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_RSHIFT);
    match("shift a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT);
    match("shift a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CAPSLOCK);
    match("shift a", IECLASS_RAWKEY, 0x20, 0);
    match("caps a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CAPSLOCK);
    match("caps a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_LSHIFT);
    match("caps a", IECLASS_RAWKEY, 0x20, 0);
    match("alt a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_RALT);
    match("alt a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_LALT);
    match("-shift a", IECLASS_RAWKEY, 0x20, 0);
    match("-shift a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_LSHIFT);
    match("-shift a", IECLASS_RAWKEY, 0x20, IEQUALIFIER_CONTROL);
    match("-upstroke a", IECLASS_RAWKEY, 0xa0, 0);
    match("-upstroke a", IECLASS_RAWKEY, 0x20, 0);
    match("upstroke a", IECLASS_RAWKEY, 0xa0, 0);
    match("upstroke a", IECLASS_RAWKEY, 0x20, 0);
    match("rawmouse lbutton mouse_leftpress", IECLASS_RAWMOUSE, IECODE_LBUTTON, IEQUALIFIER_LEFTBUTTON);
    match("rawmouse lbutton mouse_leftpress", IECLASS_RAWMOUSE, IECODE_LBUTTON, IEQUALIFIER_LEFTBUTTON | IEQUALIFIER_RELATIVEMOUSE);
    match("rawmouse mouse_leftpress", IECLASS_RAWMOUSE, IECODE_LBUTTON, IEQUALIFIER_LEFTBUTTON);
    match("rawmouse mouse_leftpress", IECLASS_RAWMOUSE, IECODE_LBUTTON, 0);
    match("timer", IECLASS_TIMER, 0, 0);
    match("timer", IECLASS_TIMER, 0, IEQUALIFIER_LEFTBUTTON);
    match("timer", IECLASS_RAWKEY, 0, 0);
    match("diskinserted", IECLASS_DISKINSERTED, 0, 0);
    match("bogus", IECLASS_RAWKEY, 0, 0);
    match("", IECLASS_RAWKEY, 0, 0);

    P_SECTION("MatchIX with hand-made IX");
    ix.ix_Version = IX_VERSION;
    ix.ix_Class = IECLASS_NULL;
    ix.ix_Code = 0;
    ix.ix_CodeMask = 0;
    ix.ix_Qualifier = 0;
    ix.ix_QualMask = 0;
    ix.ix_QualSame = 0;
    ev.ie_Class = IECLASS_RAWKEY;
    ev.ie_Code = 0x33;
    ev.ie_Qualifier = 0x1234;
    P_BOOL("class NULL vs rawkey", MatchIX(&ev, &ix));
    ev.ie_Class = IECLASS_TIMER;
    P_BOOL("class NULL vs timer", MatchIX(&ev, &ix));
    ix.ix_Class = IECLASS_RAWKEY;
    ix.ix_Code = 0x30;
    ix.ix_CodeMask = 0xf0;
    ev.ie_Class = IECLASS_RAWKEY;
    P_BOOL("code mask 0xf0, 0x33 vs 0x30", MatchIX(&ev, &ix));
    ix.ix_CodeMask = 0xff;
    P_BOOL("code mask 0xff, 0x33 vs 0x30", MatchIX(&ev, &ix));
    ix.ix_CodeMask = 0;
    ix.ix_Qualifier = IEQUALIFIER_LSHIFT;
    ix.ix_QualMask = IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT;
    ix.ix_QualSame = IXSYM_SHIFT;
    ev.ie_Qualifier = IEQUALIFIER_RSHIFT;
    P_BOOL("qualsame shift: ix lshift vs rshift", MatchIX(&ev, &ix));
    ix.ix_Qualifier = IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT;
    P_BOOL("qualsame shift: ix both vs rshift", MatchIX(&ev, &ix));
    ix.ix_QualSame = 0;
    P_BOOL("no qualsame: ix both vs rshift", MatchIX(&ev, &ix));
    P_SECTION("done");

    CloseLibrary(CxBase);
    return 0;
}
