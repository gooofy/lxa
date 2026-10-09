#!/usr/bin/env python3
"""Write the prefs configurations used by the Phase 236 goldens.

    python3 tests/prefs/mkprefs.py          # rewrites tests/prefs/<config>/*.prefs

The files follow the NDK prefs headers (prefs/*.h, IFF FORM PREF with a
PRHD chunk); the layout of each chunk was checked against files written by
the AmigaOS 3.1 Prefs editors on the reference machine.  The scenarios
tests/scenarios/gallery-prefs-*.yaml install a directory as ENVARC:Sys
(reference) / ENV:Sys (lxa).
"""

import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))


def chunk(cid, data):
    out = cid + struct.pack(">I", len(data)) + data
    return out + (b"\0" if len(data) & 1 else b"")


def form(*chunks):
    body = b"PREF" + chunk(b"PRHD", bytes(6)) + b"".join(chunks)
    return b"FORM" + struct.pack(">I", len(body)) + body


def cstr(s, n):
    b = s.encode("latin-1")
    assert len(b) < n
    return b + bytes(n - len(b))


# -- font.prefs: FontPrefs ------------------------------------------------------
FP_WBFONT, FP_SYSFONT, FP_SCREENFONT = 0, 1, 2
FPF_ROMFONT, FPF_DISKFONT = 1, 2


def font_chunk(typ, name, ysize, flags, fpen=1, bpen=0, drmd=1, style=0):
    return chunk(b"FONT", bytes(12) + struct.pack(">HHBBBBIHBB", 0, typ, fpen, bpen, drmd, 0, 0,
                                                    ysize, style, flags) + cstr(name, 128))


# -- palette.prefs: PalettePrefs ---------------------------------------------------
def palette(pens4, pens8, colors):
    def pens(p):
        p = list(p) + [0xffff]
        return b"".join(struct.pack(">H", v) for v in p + [0] * (32 - len(p)))
    cs = b""
    for i, (r, g, b) in enumerate(colors):
        cs += struct.pack(">hHHH", i, r * 0x101, g * 0x101, b * 0x101)
    cs += struct.pack(">hHHH", -1, 0, 0, 0)
    cs += bytes(32 * 8 - len(cs))
    return chunk(b"PALT", bytes(16) + pens(pens4) + pens(pens8) + cs)


# -- screenmode.prefs: ScreenModePrefs ----------------------------------------------
def screenmode(display_id, width, height, depth, control=1):
    return chunk(b"SCRM", bytes(16) + struct.pack(">IhhHH", display_id, width, height, depth, control))


# -- overscan.prefs: OverscanPrefs ---------------------------------------------------
def overscan(display_id, viewpos, text, standard):
    return chunk(b"OSCN", struct.pack(">II4HI", 0, 0xFEDCBA89, 0, 0, 0, 0, display_id)
                 + struct.pack(">hh", *viewpos) + struct.pack(">hh", *text)
                 + struct.pack(">hhhh", *standard))


# -- input.prefs: InputPrefs -----------------------------------------------------------
def tv(sec):
    s = int(sec)
    return struct.pack(">II", s, int(round((sec - s) * 1000000)))


def inputp(keymap, ticks, dclick, rptdelay, rptspeed, accel):
    return chunk(b"INPT", cstr(keymap, 16) + struct.pack(">H", ticks) + tv(dclick) + tv(rptdelay)
                 + tv(rptspeed) + struct.pack(">h", accel))


# -- pointer.prefs: PointerPrefs + RGBTable + planes --------------------------------
def pointer(which, hot, colors, plane0, plane1, height=24):
    rows0 = list(plane0) + [0] * (height - len(plane0))
    rows1 = list(plane1) + [0] * (height - len(plane1))
    data = b"".join(struct.pack(">H", v) for v in rows0) + b"".join(struct.pack(">H", v) for v in rows1)
    hdr = bytes(16) + struct.pack(">HHHHHHhh", which, 5, 16, height, 2, 0, hot[0], hot[1])
    return chunk(b"PNTR", hdr + b"".join(bytes(c) for c in colors) + data)


# -- locale.prefs: LocalePrefs + CountryPrefs -------------------------------------------
def localep(country, languages, gmt, cd):
    langs = b"".join(cstr(l, 30) for l in languages) + bytes(30 * (10 - len(languages)))
    c = bytes(16) + struct.pack(">IIB", cd["code"], cd["tel"], cd["ms"])
    for k, n in (("dt", 80), ("d", 40), ("t", 40), ("sdt", 80), ("sd", 40), ("st", 40)):
        c += cstr(cd[k], n)
    c += cstr(cd["dec"], 10) + cstr(cd["grp"], 10) + cstr(cd["fgrp"], 10)
    c += bytes(cd["grouping"]) + bytes(10 - len(cd["grouping"]))
    c += bytes(cd["fgrouping"]) + bytes(10 - len(cd["fgrouping"]))
    c += cstr(cd["mdec"], 10) + cstr(cd["mgrp"], 10) + cstr(cd["mfgrp"], 10)
    c += bytes(cd["mgrouping"]) + bytes(10 - len(cd["mgrouping"]))
    c += bytes(cd["mfgrouping"]) + bytes(10 - len(cd["mfgrouping"]))
    c += struct.pack(">BB", cd["fracdigits"], cd["intfracdigits"])
    c += cstr(cd["cs"], 10) + cstr(cd["smallcs"], 10) + cstr(cd["intcs"], 10)
    c += cstr(cd["possign"], 10) + struct.pack(">BBB", *cd["pos"])
    c += cstr(cd["negsign"], 10) + struct.pack(">BBB", *cd["neg"])
    c += struct.pack(">B", cd["ct"])
    body = bytes(16) + cstr(country, 32) + langs + struct.pack(">iI", gmt, 0) + c
    return chunk(b"LCLE", body)


# the AmigaOS 3.1 default pointer (as written by the Pointer editor)
STD_P0 = [0xc000, 0x7000, 0x3c00, 0x3f00, 0x1fc0, 0x1fc0, 0x0f00, 0x0d80, 0x04c0, 0x0460, 0x0020]
STD_P1 = [0x4000, 0xb000, 0x4c00, 0x4300, 0x20c0, 0x2000, 0x1100, 0x1280, 0x0940, 0x08a0, 0x0040]
BUSY_P0 = [0x0400, 0x0000, 0x0100, 0x0000, 0x07c0, 0x1ff0, 0x3ff8, 0x3ff8, 0x7ffc, 0x7efc, 0x7ffc,
           0x3ff8, 0x3ff8, 0x1ff0, 0x07c0]
BUSY_P1 = [0x07c0, 0x07c0, 0x0380, 0x07e0, 0x1ff8, 0x3fec, 0x7fde, 0x7fbe, 0xff7f, 0xffff, 0xffff,
           0x7ffe, 0x7ffe, 0x3ffc, 0x1ff8, 0x07e0]


def write(config, name, data):
    d = os.path.join(HERE, config)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name), "wb") as f:
        f.write(data)


def main():
    # fontpal: non-default fonts and palette (needs the Workbench 3.1 fonts)
    write("fontpal", "font.prefs", form(
        font_chunk(FP_WBFONT, "times.font", 11, FPF_DISKFONT, drmd=1),
        font_chunk(FP_SYSFONT, "topaz.font", 9, FPF_ROMFONT),
        font_chunk(FP_SCREENFONT, "helvetica.font", 13, FPF_DISKFONT)))
    #          detail block text shine shadow fill filltext bg hilite bardetail barblock bartrim
    pens4 = [0, 1, 1, 2, 1, 2, 1, 0, 3, 1, 2, 1]
    pens8 = [0, 1, 1, 2, 1, 5, 2, 0, 3, 1, 2, 1]
    write("fontpal", "palette.prefs", form(palette(pens4, pens8, [
        (0x88, 0x99, 0xaa), (0x11, 0x22, 0x33), (0xee, 0xdd, 0xcc), (0x55, 0x77, 0xcc),
        (0xcc, 0x33, 0x33), (0x33, 0xcc, 0x33), (0x33, 0x33, 0xcc), (0xcc, 0xcc, 0x33)])))

    # sys: Workbench mode/depth, overscan, input, locale, pointer, and a
    # palette for the 8-colour Workbench
    write("sys", "palette.prefs", form(palette(
        [0, 1, 1, 2, 1, 3, 1, 0, 2, 1, 2, 1],
        [0, 1, 1, 7, 1, 5, 2, 0, 6, 4, 2, 1], [
            (0x99, 0x99, 0x99), (0x00, 0x00, 0x22), (0xff, 0xff, 0xee), (0x44, 0x66, 0x99),
            (0xaa, 0x22, 0x88), (0x22, 0x88, 0xaa), (0x88, 0xaa, 0x22), (0xff, 0x88, 0x00)])))
    write("sys", "screenmode.prefs", form(screenmode(0x29000, -1, -1, 3)))
    write("sys", "overscan.prefs", form(overscan(0x29000, (129, 44), (672, 270), (-16, -7, 687, 276))))
    write("sys", "input.prefs", form(inputp("d", 2, 0.8, 0.3, 0.04, 1)))
    write("sys", "locale.prefs", form(localep("deutschland", ["english"], -60, {
        "code": 0x44000000, "tel": 49, "ms": 0,
        "dt": "%A, %e. %B %Y %H:%M", "d": "%A, %e. %B %Y", "t": "%H:%M:%S",
        "sdt": "%d.%m.%y %H:%M", "sd": "%d.%m.%y", "st": "%H:%M",
        "dec": ",", "grp": ".", "fgrp": "", "grouping": [3], "fgrouping": [0],
        "mdec": ",", "mgrp": ".", "mfgrp": "", "mgrouping": [3], "mfgrouping": [0],
        "fracdigits": 2, "intfracdigits": 2, "cs": "DM", "smallcs": "Pf", "intcs": "DEM",
        "possign": "", "pos": (1, 1, 1), "negsign": "-", "neg": (1, 1, 1), "ct": 1})))
    write("sys", "pointer.prefs", form(
        pointer(0, (1, 1), [(0x00, 0x99, 0x00), (0x00, 0x00, 0x00), (0xff, 0xff, 0x88)],
                [v >> 1 for v in STD_P0], [v >> 1 for v in STD_P1]),
        pointer(1, (-5, 0), [(0xe0, 0x40, 0x40), (0x00, 0x00, 0x00), (0xe0, 0xe0, 0xc0)],
                BUSY_P0, BUSY_P1)))


if __name__ == "__main__":
    main()
