"""Comparison report (Phase 214).

    python3 -m rdd report <run-out-dir> [--html FILE]

For every scenario snapshot present on both backends:
  <out>/<scenario>/compare/<snapshot>/compare.json   full comparison
  <out>/<scenario>/compare/<snapshot>/composite.png  lxa | reference | diff,
        nearest-neighbour x2 with a 16 px coordinate grid (for vision review)
and for the whole run:
  <out>/report.html    side-by-side page for humans
  <out>/compare-summary.json   one line per snapshot for agents
"""

import base64
import html
import io
import json
import os

from rdd import bundle
from rdd.compare import CELL, compare_bundles

try:
    from PIL import Image, ImageDraw
except ImportError:      # composites need Pillow; the JSON part works without
    Image = None

SCALE = 2


def _pen_image(path):
    w, h, pens = bundle.read_palette_png(os.path.join(path, "screen.png"))
    with open(os.path.join(path, "palette.json")) as f:
        pal = json.load(f)
    img = Image.frombytes("P", (w, h), pens)
    flat = [c for rgb in pal for c in rgb]
    img.putpalette(flat + [0] * (768 - len(flat)))
    return img.convert("RGB")


def _grid(img):
    d = ImageDraw.Draw(img)
    w, h = img.size
    for x in range(0, w, CELL * SCALE):
        d.line([(x, 0), (x, h)], fill=(255, 255, 0) if (x // (CELL * SCALE)) % 4 == 0 else (90, 90, 140))
    for y in range(0, h, CELL * SCALE):
        d.line([(0, y), (w, y)], fill=(255, 255, 0) if (y // (CELL * SCALE)) % 4 == 0 else (90, 90, 140))
    return img


def composite(lpath, rpath, diff_mask, out_png):
    a, b = _pen_image(lpath), _pen_image(rpath)
    w, h = min(a.width, b.width), min(a.height, b.height)
    dimg = b.crop((0, 0, w, h)).convert("L").point(lambda v: 40 + v // 3).convert("RGB")
    px = dimg.load()
    for y in range(h):
        for x in range(w):
            if diff_mask[y * w + x]:
                px[x, y] = (255, 0, 0)
    tiles = [a, b, dimg]
    tiles = [_grid(t.resize((t.width * SCALE, t.height * SCALE), Image.NEAREST)) for t in tiles]
    gap = 8
    label_h = 14
    cw = sum(t.width for t in tiles) + gap * (len(tiles) - 1)
    ch = max(t.height for t in tiles) + label_h
    comp = Image.new("RGB", (cw, ch), (255, 0, 255))
    d = ImageDraw.Draw(comp)
    x = 0
    for t, label in zip(tiles, ("lxa", "reference", "diff (red = pen differs)")):
        d.rectangle([x, 0, x + t.width, label_h], fill=(0, 0, 0))
        d.text((x + 3, 1), "%s   grid %d px" % (label, CELL), fill=(255, 255, 255))
        comp.paste(t, (x, label_h))
        x += t.width + gap
    comp.save(out_png)
    return out_png


def _b64(path):
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode()


def build(out_dir, html_path=None):
    rows = []
    for scn in sorted(os.listdir(out_dir)):
        sdir = os.path.join(out_dir, scn)
        ldir, rdir = os.path.join(sdir, "lxa"), os.path.join(sdir, "ref")
        if not (os.path.isdir(ldir) and os.path.isdir(rdir)):
            continue
        snaps = sorted(set(os.listdir(ldir)) & set(os.listdir(rdir)))
        for snap in snaps:
            lp, rp = os.path.join(ldir, snap), os.path.join(rdir, snap)
            if not os.path.exists(os.path.join(lp, "meta.json")):
                continue
            res, mask = compare_bundles(lp, rp)
            cdir = os.path.join(sdir, "compare", snap)
            os.makedirs(cdir, exist_ok=True)
            if mask is not None and Image is not None:
                res["composite"] = composite(lp, rp, mask, os.path.join(cdir, "composite.png"))
            with open(os.path.join(cdir, "compare.json"), "w") as f:
                json.dump(res, f, indent=1)
            px = res.get("pixels", {})
            rows.append({"scenario": scn, "snapshot": snap, "verdict": res["verdict"],
                         "tree_diffs": len(res["tree"]), "diff_pixels": px.get("diff_pixels"),
                         "diff_ratio": px.get("diff_ratio"), "regions": len(px.get("regions", [])),
                         "palette_diffs": len(res.get("palette", [])),
                         "text_missing": len(res["text"]["missing_in_lxa"]),
                         "text_extra": len(res["text"]["extra_in_lxa"]),
                         "stdout_equal": res["stdout_equal"],
                         "compare": os.path.relpath(os.path.join(cdir, "compare.json"), out_dir),
                         "composite": os.path.relpath(res["composite"], out_dir) if res.get("composite") else None})
    with open(os.path.join(out_dir, "compare-summary.json"), "w") as f:
        json.dump(rows, f, indent=1)
    html_path = html_path or os.path.join(out_dir, "report.html")
    _write_html(out_dir, rows, html_path)
    return rows


def _write_html(out_dir, rows, path):
    parts = ["<!doctype html><meta charset=utf-8><title>RDD report</title><style>"
             "body{font-family:sans-serif;margin:16px}table{border-collapse:collapse}"
             "td,th{padding:2px 8px;border-bottom:1px solid #ccc;text-align:left}"
             ".ok{color:#070}.bad{color:#b00}img{max-width:100%;image-rendering:pixelated}"
             "details{margin:8px 0}</style><h1>lxa vs AmigaOS 3.1 reference</h1>",
             "<table><tr><th>scenario</th><th>snapshot</th><th>verdict</th><th>tree diffs</th>"
             "<th>pixel diffs</th><th>regions</th><th>palette</th><th>text missing/extra</th>"
             "<th>stdout</th></tr>"]
    for r in rows:
        cls = "ok" if r["verdict"] == "identical" else "bad"
        parts.append("<tr><td><a href='#%s-%s'>%s</a></td><td>%s</td><td class=%s>%s</td><td>%d</td>"
                     "<td>%s</td><td>%s</td><td>%d</td><td>%d / %d</td><td>%s</td></tr>" % (
                         html.escape(r["scenario"]), html.escape(r["snapshot"]), html.escape(r["scenario"]),
                         html.escape(r["snapshot"]), cls, r["verdict"], r["tree_diffs"],
                         "-" if r["diff_pixels"] is None else "%d (%.2f%%)" % (r["diff_pixels"], 100 * r["diff_ratio"]),
                         r["regions"], r["palette_diffs"], r["text_missing"], r["text_extra"],
                         "equal" if r["stdout_equal"] else "differs"))
    parts.append("</table>")
    for r in rows:
        with open(os.path.join(out_dir, r["compare"])) as f:
            res = json.load(f)
        parts.append("<h2 id='%s-%s'>%s / %s</h2>" % (html.escape(r["scenario"]), html.escape(r["snapshot"]),
                                                   html.escape(r["scenario"]), html.escape(r["snapshot"])))
        if r["composite"]:
            parts.append("<img src='data:image/png;base64,%s'>" % _b64(os.path.join(out_dir, r["composite"])))
        if res["tree"]:
            parts.append("<details open><summary>%d tree differences</summary><table>"
                         "<tr><th>where</th><th>field</th><th>lxa</th><th>reference</th></tr>" % len(res["tree"]))
            for t in res["tree"][:200]:
                parts.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % tuple(
                    html.escape(str(v)) for v in (t["where"], t["field"], t["lxa"], t["ref"])))
            parts.append("</table></details>")
        if res.get("pixels", {}).get("regions"):
            parts.append("<details><summary>%d differing regions</summary><pre>%s</pre></details>" % (
                len(res["pixels"]["regions"]), html.escape(json.dumps(res["pixels"]["regions"][:50], indent=1))))
        if res["text"]["missing_in_lxa"] or res["text"]["extra_in_lxa"]:
            parts.append("<details><summary>Text() differences</summary><pre>%s</pre></details>" % html.escape(
                json.dumps(res["text"], indent=1, ensure_ascii=False)))
    with open(path, "w") as f:
        f.write("\n".join(parts))
