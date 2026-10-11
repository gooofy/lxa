#!/usr/bin/env python3
"""Generate src/rom/lxa_icon_defaults.h: the images of icon.library's
built-in default icons (GetDefDiskObject() without ENV:Sys/def_<type>.info).

The artwork is lxa's own (MIT, like the rest of lxa).  Only the image sizes
and depth follow AmigaOS 3.1, because programs see them through the
DiskObject; the pictures themselves are drawn here.

Pens are those of the Workbench 3.1 palette: 0 grey (background), 1 black,
2 white, 3 blue.  The images are shown on a hires screen whose pixels are
twice as tall as wide, so round shapes use rx = 2 * ry.

    python3 tools/gen_icon_defaults.py            # rewrite the header
    python3 tools/gen_icon_defaults.py --png F    # also render a preview
"""
import argparse
import math
import os

G, K, W, B = 0, 1, 2, 3

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(ROOT, "src", "rom", "lxa_icon_defaults.h")


class Img:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.p = [[G] * w for _ in range(h)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y][x] = c

    def get(self, x, y):
        return self.p[y][x] if 0 <= x < self.w and 0 <= y < self.h else None

    def fill(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, c)

    def hline(self, x0, x1, y, c):
        self.fill(min(x0, x1), y, max(x0, x1), y, c)

    def vline(self, x, y0, y1, c):
        self.fill(x, min(y0, y1), x, max(y0, y1), c)

    def box(self, x0, y0, x1, y1, c, fill=None):
        if fill is not None:
            self.fill(x0, y0, x1, y1, fill)
        self.hline(x0, x1, y0, c)
        self.hline(x0, x1, y1, c)
        self.vline(x0, y0, y1, c)
        self.vline(x1, y0, y1, c)

    def line(self, x0, y0, x1, y1, c):
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(n + 1):
            self.set(round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n), c)

    def shape(self, inside, c):
        """fill every pixel whose centre satisfies inside(x, y)"""
        for y in range(self.h):
            for x in range(self.w):
                if inside(x + 0.5, y + 0.5):
                    self.set(x, y, c)

    def outline(self, c, of=None):
        """draw pen c around every non-background region (4-neighbourhood)"""
        src = [row[:] for row in self.p]
        for y in range(self.h):
            for x in range(self.w):
                if src[y][x] != G:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < self.w and 0 <= yy < self.h and src[yy][xx] != G and \
                            (of is None or src[yy][xx] in of):
                        self.p[y][x] = c
                        break

    def shadow(self, c=K):
        """a one-pixel drop shadow to the lower right"""
        src = [row[:] for row in self.p]
        for y in range(self.h):
            for x in range(self.w):
                if src[y][x] == G and (src[y - 1][x - 2] if y > 0 and x > 1 else G) != G:
                    self.p[y][x] = c


# -- the pictures -------------------------------------------------------------

def disk():
    """3.5" floppy: blue body, metal shutter with its window, paper label"""
    im = Img(35, 17)
    im.box(1, 0, 32, 15, K, fill=B)
    for i in range(3):                       # chamfered top right corner
        im.hline(32 - 2 + i, 32, i, G)
    im.line(30, 0, 32, 2, K)
    im.box(9, 0, 23, 5, K, fill=G)           # shutter
    im.fill(18, 1, 20, 4, K)                 # its window
    im.hline(10, 16, 1, W)
    im.box(6, 8, 27, 15, K, fill=W)          # label
    im.hline(9, 24, 10, B)
    im.hline(9, 20, 12, B)
    im.fill(3, 13, 4, 14, K)                 # write-protect hole
    im.hline(2, 2, 1, W)
    im.vline(2, 1, 6, W)
    im.shadow()
    return im


def folder(open_=False):
    """a file folder with a tab; selected: opened, papers inside"""
    im = Img(57, 14)
    im.box(2, 1, 17, 3, K, fill=B)           # tab
    im.box(2, 3, 53, 12, K, fill=B)          # back
    im.hline(3, 16, 2, W)
    if open_:
        im.box(9, 2, 47, 9, K, fill=W)       # papers sticking out
        im.hline(13, 40, 4, B)
        im.hline(13, 34, 6, B)
        for y in range(6, 13):               # front flap, tilted forward
            d = (12 - y) // 2
            im.hline(2 + d, 53 - d, y, B)
            im.set(2 + d, y, K)
            im.set(53 - d, y, K)
        im.hline(5, 50, 6, K)
        im.hline(6, 49, 7, W)
        im.hline(2, 53, 12, K)
    else:
        im.hline(3, 52, 4, W)                # rim of the front
        im.hline(22, 33, 8, K)               # label holder
        im.hline(22, 33, 10, K)
        im.vline(22, 8, 10, K)
        im.vline(33, 8, 10, K)
        im.hline(23, 32, 9, W)
    im.shadow()
    return im


def tool():
    """a program: a window with a cog wheel"""
    im = Img(54, 22)
    im.box(4, 0, 49, 20, K, fill=W)
    im.fill(5, 1, 48, 3, B)                  # title bar
    im.hline(4, 49, 4, K)
    im.box(6, 1, 9, 3, K, fill=W)            # close gadget
    im.box(44, 1, 47, 3, K, fill=W)          # depth gadget

    cx, cy = 26.5, 12.5

    def cog(x, y):
        dx, dy = (x - cx) / 2.0, y - cy
        r = math.hypot(dx, dy)
        a = math.atan2(dy, dx)
        tooth = 1.0 if math.cos(6 * a) > -0.15 else 0.0
        return r <= 5.0 + 2.2 * tooth

    im.shape(cog, K)
    im.shape(lambda x, y: math.hypot((x - cx) / 2.0, y - cy) <= 3.9, B)
    im.shape(lambda x, y: math.hypot((x - cx) / 2.0, y - cy) <= 1.7, W)
    im.shadow()
    return im


def project():
    """a document: page with a folded corner and lines of text"""
    im = Img(54, 22)
    x0, x1, y0, y1, ear = 13, 40, 0, 20, 6
    im.fill(x0, y0, x1, y1, W)
    for i in range(ear // 2 + 1):            # folded corner (2:1 pixels)
        im.hline(x1 - ear + 2 * i + 1, x1, y0 + i, G)
    im.hline(x0, x1 - ear, y0, K)
    im.vline(x0, y0, y1, K)
    im.hline(x0, x1, y1, K)
    im.vline(x1, y0 + ear // 2, y1, K)
    im.line(x1 - ear, y0, x1, y0 + ear // 2, K)
    im.vline(x1 - ear, y0, y0 + ear // 2, K)
    im.hline(x1 - ear, x1, y0 + ear // 2, K)
    for x in range(x1 - ear + 1, x1):
        for y in range(y0 + 1, y0 + ear // 2):
            if im.get(x, y) == W and (x - (x1 - ear)) >= 2 * (y - y0):
                im.set(x, y, B)
    im.fill(17, 3, 29, 4, B)                 # heading
    for y, x_end in ((7, 36), (9, 36), (11, 33), (13, 36), (15, 30)):
        im.hline(17, x_end, y, K)
    im.shadow()
    return im


def basket(full=False):
    """a wire waste basket; selected: crumpled paper in it"""
    im = Img(51, 31)
    top, bot = 7, 29

    def body(x, y):
        if not (top <= y <= bot):
            return False
        t = (y - top) / (bot - top)
        half = 20 - 6 * t
        return abs(x - 25.5) <= half

    im.shape(body, W)
    for y in range(top, bot + 1):            # wire slats
        t = (y - top) / (bot - top)
        half = 20 - 6 * t
        for k in range(-3, 4):
            im.set(round(25.5 + k * half / 3.6 - 0.5), y, B)
    for y in (13, 21):                       # bands
        t = (y - top) / (bot - top)
        half = 20 - 6 * t
        im.hline(round(25.5 - half), round(25.5 + half) - 1, y, B)
    im.outline(K)
    def ell(rx, ry):
        return lambda x, y: ((x - 25.5) / rx) ** 2 + ((y - 7.0) / ry) ** 2 <= 1
    im.shape(ell(22.0, 3.2), K)              # rim
    im.shape(ell(20.6, 2.3), B)
    im.shape(ell(17.0, 1.2), K)              # the inside
    if full:
        def paper(cx, cy, rx, ry):
            return lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1
        for cx, cy, rx, ry in ((17, 4.5, 8, 3.6), (31, 3.5, 9, 3.4), (24, 6, 7, 2.6)):
            im.shape(paper(cx + 0.5, cy, rx + 1.6, ry + 0.9), K)
            im.shape(paper(cx + 0.5, cy, rx, ry), W)
        for x0, y0, x1, y1 in ((13, 4, 18, 6), (28, 2, 33, 5), (21, 6, 26, 7), (34, 4, 36, 2)):
            im.line(x0, y0, x1, y1, K)
    im.shadow()
    return im


IMAGES = [
    ("disk", disk),
    ("drawer", folder),
    ("drawer_sel", lambda: folder(True)),
    ("tool", tool),
    ("project", project),
    ("trashcan", basket),
    ("trashcan_sel", lambda: basket(True)),
]


# -- output -------------------------------------------------------------------

def planes(im):
    wpr = (im.w + 15) // 16
    words = []
    for plane in range(2):
        for y in range(im.h):
            for wx in range(wpr):
                v = 0
                for bit in range(16):
                    x = wx * 16 + bit
                    if x < im.w and (im.p[y][x] >> plane) & 1:
                        v |= 0x8000 >> bit
                words.append(v)
    return words


def header(images):
    out = ["/*",
           " * Built-in default icon images of icon.library (GetDefDiskObject() when",
           " * ENV:Sys/def_<type>.info does not exist).  lxa's own artwork, generated",
           " * by tools/gen_icon_defaults.py - edit that script, not this file.  The",
           " * sizes (2 bitplanes) are those of AmigaOS 3.1 (tests/probes/icon/diskobj.c).",
           " */", ""]
    for name, im in images:
        words = planes(im)
        out.append("/* %d x %d */" % (im.w, im.h))
        out.append("static const UWORD g_icon_img_%s[%d] = {" % (name, len(words)))
        for i in range(0, len(words), 12):
            out.append("    " + " ".join("0x%04x," % w for w in words[i:i + 12]))
        out.append("};")
        out.append("")
    return "\n".join(out)


def preview(images, path):
    from PIL import Image, ImageDraw
    pal = [(149, 149, 149), (0, 0, 0), (255, 255, 255), (59, 103, 162)]
    sx, sy, pad = 6, 12, 20
    tiles = []
    for name, im in images:
        t = Image.new("RGB", (im.w, im.h))
        for y in range(im.h):
            for x in range(im.w):
                t.putpixel((x, y), pal[im.p[y][x]])
        tiles.append((name, t.resize((im.w * sx, im.h * sy), Image.NEAREST)))
    out = Image.new("RGB", (sum(t.width for _, t in tiles) + pad * (len(tiles) + 1),
                            max(t.height for _, t in tiles) + pad * 2 + 20), pal[0])
    dr = ImageDraw.Draw(out)
    x = pad
    for name, t in tiles:
        out.paste(t, (x, pad))
        dr.text((x, pad + t.height + 6), name, fill=(0, 0, 0))
        x += t.width + pad
    out.save(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--png", help="also write a preview image")
    ap.add_argument("--out", default=HEADER)
    a = ap.parse_args()
    images = [(n, f()) for n, f in IMAGES]
    with open(a.out, "w") as f:
        f.write(header(images))
    if a.png:
        preview(images, a.png)


if __name__ == "__main__":
    main()
