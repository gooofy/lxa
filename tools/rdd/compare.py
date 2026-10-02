"""Comparator (Phase 214): lxa bundle vs reference bundle.

Order of evidence (AGENTS.md §1a, RDD principle 2):
  1. tree diff   - structure: screens, windows, gadgets, menus, fonts
  2. pixel diff  - pen indices, window-relative, with a heat map of regions
  3. palette / Text() / stdout diffs
Vision review (skill rdd-review) only triages what these report.

    from rdd.compare import compare_bundles
    result = compare_bundles("out/x/lxa/startup", "out/x/ref/startup")
"""

import json
import os
from collections import Counter

from rdd import bundle

# Fields never compared (environment noise, not behaviour).  Paths are
# "screen.*" / "window.*" / "gadget.*" / "menu.*" / "item.*" keys.
IGNORE = {
    "screen.title",          # Workbench screen title changes with the boot state
    "screen.default_title",
    "window.app",            # lxa has no foreign (Workbench) windows
}

SCREEN_FIELDS = ["width", "height", "depth", "display_id", "flags", "bar_height", "bar_vborder",
                 "bar_hborder", "menu_vborder", "menu_hborder", "wbor"]
WINDOW_FIELDS = ["left", "top", "width", "height", "min_width", "min_height", "max_width",
                 "max_height", "flags", "idcmp", "border", "detail_pen", "block_pen"]
GADGET_FIELDS = ["id", "type", "flags", "activation", "left", "top", "width", "height"]
MENU_FIELDS = ["left", "top", "width", "height", "flags"]
ITEM_FIELDS = ["left", "top", "width", "height", "flags", "mutualexclude", "command", "text"]

CELL = 16   # heat-map cell size in pixels


def _load(path, name):
    p = os.path.join(path, name)
    if not os.path.exists(p):
        return None
    with open(p, encoding="utf-8") as f:
        if name.endswith(".jsonl"):
            return [json.loads(line) for line in f if line.strip()]
        if name.endswith(".json"):
            return json.load(f)
        return f.read()


def _font_key(f):
    return None if not f else "%s/%d" % (f.get("name"), f.get("ysize"))


class TreeDiff:
    def __init__(self):
        self.items = []

    def add(self, kind, where, field, lxa, ref):
        if "%s.%s" % (kind, field) in IGNORE:
            return
        self.items.append({"kind": kind, "where": where, "field": field, "lxa": lxa, "ref": ref})

    def fields(self, kind, where, a, b, names):
        for n in names:
            if a.get(n) != b.get(n):
                self.add(kind, where, n, a.get(n), b.get(n))


def _app_windows(screen):
    return [w for w in screen.get("windows", []) if w.get("app", True)]


def diff_items(d, where, a_items, b_items, depth=0):
    if len(a_items) != len(b_items):
        d.add("item", where, "count", len(a_items), len(b_items))
    for i, (ia, ib) in enumerate(zip(a_items, b_items)):
        w = "%s/%s" % (where, ib.get("text") or "#%d" % i)
        d.fields("item", w, ia, ib, ITEM_FIELDS)
        if depth == 0:
            diff_items(d, w, ia.get("sub", []), ib.get("sub", []), 1)


def diff_trees(lt, rt):
    d = TreeDiff()
    ls, rs = lt.get("screens", []), rt.get("screens", [])
    # A screen without application windows that exists on one side only is
    # environment (lxa opens Workbench lazily; the reference always has it).
    lt_titles = {s.get("title") for s in ls}
    rs = [s for s in rs if _app_windows(s) or s.get("title") in lt_titles or len(ls) >= len(rs)]
    if len(ls) != len(rs):
        d.add("screen", "/", "count", len(ls), len(rs))
    # match screens by title when possible, else by position
    for si, rsc in enumerate(rs):
        lsc = next((s for s in ls if s.get("title") == rsc.get("title")), ls[si] if si < len(ls) else None)
        where = "screen[%s]" % (rsc.get("title") or si)
        if lsc is None:
            d.add("screen", where, "missing", None, rsc.get("title"))
            continue
        d.fields("screen", where, lsc, rsc, SCREEN_FIELDS)
        if _font_key(lsc.get("font")) != _font_key(rsc.get("font")):
            d.add("screen", where, "font", _font_key(lsc.get("font")), _font_key(rsc.get("font")))
        lw, rw = _app_windows(lsc), _app_windows(rsc)
        if len(lw) != len(rw):
            d.add("window", where, "count", len(lw), len(rw))
        for wi, rwin in enumerate(rw):
            lwin = next((w for w in lw if w.get("title") == rwin.get("title")), lw[wi] if wi < len(lw) else None)
            wwhere = "%s/window[%s]" % (where, rwin.get("title") or wi)
            if lwin is None:
                d.add("window", wwhere, "missing", None, rwin.get("title"))
                continue
            if lwin.get("title") != rwin.get("title"):
                d.add("window", wwhere, "title", lwin.get("title"), rwin.get("title"))
            d.fields("window", wwhere, lwin, rwin, WINDOW_FIELDS)
            if _font_key(lwin.get("font")) != _font_key(rwin.get("font")):
                d.add("window", wwhere, "font", _font_key(lwin.get("font")), _font_key(rwin.get("font")))
            lg, rg = lwin.get("gadgets", []), rwin.get("gadgets", [])
            if len(lg) != len(rg):
                d.add("gadget", wwhere, "count", len(lg), len(rg))
            # application gadgets: zero-size gadgets (GadTools' context
            # gadget) are only counted; the rest are paired by GadgetID when
            # the IDs are unique on both sides, else by order
            lapp = [g for g in lg if not g["type"] & 0x8000 and (g["width"] or g["height"])]
            rapp = [g for g in rg if not g["type"] & 0x8000 and (g["width"] or g["height"])]
            lids, rids = [g["id"] for g in lapp], [g["id"] for g in rapp]
            if len(set(lids)) == len(lids) and len(set(rids)) == len(rids):
                lbyid = {g["id"]: g for g in lapp}
                pairs = [(lbyid.get(g["id"]), g) for g in rapp]
                for g in lapp:
                    if g["id"] not in set(rids):
                        d.add("gadget", "%s/gadget[id=%d]" % (wwhere, g["id"]), "extra", g["id"], None)
            else:
                pairs = list(zip(lapp, rapp))
            for gi, (ga, gb) in enumerate(pairs):
                if ga is None:
                    d.add("gadget", "%s/gadget[id=%d]" % (wwhere, gb["id"]), "missing", None, gb["id"])
                    continue
                gw = "%s/gadget[%d id=%d]" % (wwhere, gi, gb["id"])
                d.fields("gadget", gw, ga, gb, GADGET_FIELDS)
                ta = [t.get("text") for t in ga.get("text", [])]
                tb = [t.get("text") for t in gb.get("text", [])]
                if ta != tb:
                    d.add("gadget", gw, "text", ta, tb)
            lsys = {g["type"]: g for g in lg if g["type"] & 0x8000}
            for g in rg:
                if not g["type"] & 0x8000:
                    continue
                ga = lsys.get(g["type"])
                gw = "%s/sysgadget[0x%04x]" % (wwhere, g["type"])
                if ga is None:
                    d.add("gadget", gw, "missing", None, g["type"])
                else:
                    d.fields("gadget", gw, ga, g, ["left", "top", "width", "height", "flags"])
            lm, rm = lwin.get("menus", []), rwin.get("menus", [])
            if len(lm) != len(rm):
                d.add("menu", wwhere, "count", len(lm), len(rm))
            for ma, mb in zip(lm, rm):
                mw = "%s/menu[%s]" % (wwhere, mb.get("title"))
                if ma.get("title") != mb.get("title"):
                    d.add("menu", mw, "title", ma.get("title"), mb.get("title"))
                d.fields("menu", mw, ma, mb, MENU_FIELDS)
                diff_items(d, mw, ma.get("items", []), mb.get("items", []))
    return d.items


def diff_pixels(lpath, rpath):
    """Pen-index diff of two bundles' screen.png (same geometry required)."""
    lw, lh, lp = bundle.read_palette_png(os.path.join(lpath, "screen.png"))
    rw, rh, rp = bundle.read_palette_png(os.path.join(rpath, "screen.png"))
    res = {"lxa_size": [lw, lh], "ref_size": [rw, rh]}
    if (lw, lh) != (rw, rh):
        res["size_mismatch"] = True
        w, h = min(lw, rw), min(lh, rh)
    else:
        w, h = lw, lh
    diff = bytearray(w * h)
    count = 0
    cells = Counter()
    for y in range(h):
        lo, ro = y * lw, y * rw
        for x in range(w):
            if lp[lo + x] != rp[ro + x]:
                diff[y * w + x] = 1
                count += 1
                cells[(x // CELL, y // CELL)] += 1
    res.update({"width": w, "height": h, "diff_pixels": count,
                "diff_ratio": round(count / float(w * h), 5) if w * h else 0.0,
                "cells": [{"x": cx * CELL, "y": cy * CELL, "w": CELL, "h": CELL, "pixels": n}
                          for (cx, cy), n in sorted(cells.items())]})
    res["regions"] = _regions(cells)
    return res, diff


def _regions(cells):
    """Group adjacent differing heat-map cells into rectangles."""
    todo = set(cells)
    regions = []
    while todo:
        start = todo.pop()
        stack, comp = [start], [start]
        while stack:
            cx, cy = stack.pop()
            for n in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                if n in todo:
                    todo.remove(n)
                    stack.append(n)
                    comp.append(n)
        xs = [c[0] for c in comp]
        ys = [c[1] for c in comp]
        regions.append({"x": min(xs) * CELL, "y": min(ys) * CELL,
                        "w": (max(xs) - min(xs) + 1) * CELL, "h": (max(ys) - min(ys) + 1) * CELL,
                        "pixels": sum(cells[c] for c in comp)})
    return sorted(regions, key=lambda r: -r["pixels"])


def diff_palette(lpal, rpal):
    out = []
    for i in range(min(len(lpal or []), len(rpal or []))):
        if lpal[i] != rpal[i]:
            out.append({"pen": i, "lxa": lpal[i], "ref": rpal[i]})
    return out


def diff_text(ltext, rtext):
    """The reference hook only sees Text() calls made through the library
    vector (application code); lxa's also sees ROM-internal calls (e.g.
    gadget labels).  `missing_in_lxa` is therefore the meaningful direction."""
    lc = Counter(r["text"] for r in ltext or [])
    rc = Counter(r["text"] for r in rtext or [])
    return {"missing_in_lxa": sorted((rc - lc).elements()),
            "extra_in_lxa": sorted((lc - rc).elements())}


def compare_bundles(lpath, rpath):
    res = {"lxa": lpath, "ref": rpath}
    lt, rt = _load(lpath, "tree.json"), _load(rpath, "tree.json")
    res["tree"] = diff_trees(lt or {}, rt or {})
    pixel_diff = None
    if os.path.exists(os.path.join(lpath, "screen.png")) and os.path.exists(os.path.join(rpath, "screen.png")):
        res["pixels"], pixel_diff = diff_pixels(lpath, rpath)
        res["palette"] = diff_palette(_load(lpath, "palette.json"), _load(rpath, "palette.json"))
    res["text"] = diff_text(_load(lpath, "text.jsonl"), _load(rpath, "text.jsonl"))
    lo, ro = _load(lpath, "stdout.txt"), _load(rpath, "stdout.txt")
    res["stdout_equal"] = (lo or "") == (ro or "")
    if not res["stdout_equal"]:
        res["stdout"] = {"lxa": lo, "ref": ro}
    px = res.get("pixels", {})
    res["verdict"] = ("identical" if not res["tree"] and not px.get("diff_pixels") and res["stdout_equal"]
                      else "tree" if res["tree"] else "pixels" if px.get("diff_pixels") else "output")
    return res, pixel_diff
