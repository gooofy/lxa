"""Rendering conformance gallery page (Phase 223).

    cd tools && python3 -m rdd gallery [RUN_DIR] [--html FILE]

Collects every gallery golden (tests/golden/*/gallery-*) and shows, per
snapshot, the composite (lxa | reference | pen diff) of a run directory
together with the verdict recorded in golden.json: "pixel-identical" when the
golden has neither a pixel budget nor known tree differences, otherwise the
remaining difference and the roadmap phase that owns it.
"""

import glob
import html
import json
import os

from rdd.golden import GOLDEN_ROOT


def _verdict(entry):
    tree = entry.get("known_tree_diffs", [])
    budget = entry.get("pixel_budget", 0)
    if not tree and not budget:
        return "identical", "pixel-identical"
    parts = []
    if budget:
        parts.append("%d px (Phase %s)" % (budget, entry.get("pixel_phase")))
    if tree:
        phases = sorted({d.get("phase") for d in tree}, key=str)
        parts.append("%d tree diffs (Phase %s)" % (len(tree), ", ".join(str(p) for p in phases)))
    return "owned", "; ".join(parts)


def collect(root=GOLDEN_ROOT):
    rows = []
    for gj in sorted(glob.glob(os.path.join(root, "*", "gallery-*", "golden.json"))):
        with open(gj) as f:
            g = json.load(f)
        for snap, entry in g.get("snapshots", {}).items():
            cls, text = _verdict(entry)
            rows.append({"scenario": g.get("scenario"), "snapshot": snap,
                         "class": cls, "verdict": text,
                         "disabled": g.get("disabled")})
    return rows


def build(run_dir, html_path=None, root=GOLDEN_ROOT):
    rows = collect(root)
    html_path = html_path or os.path.join(run_dir, "gallery.html")
    base = os.path.dirname(os.path.abspath(html_path))
    n_ident = sum(1 for r in rows if r["class"] == "identical")
    out = ["<!doctype html><html><head><meta charset='utf-8'>",
           "<meta name='viewport' content='width=device-width, initial-scale=1'>",
           "<title>Rendering Gallery</title><style>",
           ":root{--bg:#fff;--fg:#222;--ok:#1a7f37;--own:#9a6700;--line:#ddd}",
           "@media (prefers-color-scheme: dark){:root{--bg:#161616;--fg:#ddd;--ok:#4ac26b;--own:#d4a72c;--line:#333}}",
           "body{background:var(--bg);color:var(--fg);font:14px system-ui,sans-serif;margin:16px}",
           "section{border-top:1px solid var(--line);padding:8px 0}",
           "img{max-width:100%;image-rendering:pixelated;display:block}",
           ".identical{color:var(--ok)}.owned{color:var(--own)}code{font-size:13px}",
           "</style></head><body>",
           "<h1>Rendering conformance gallery</h1>",
           "<p>%d snapshots, %d pixel-identical to AmigaOS 3.1; every other one "
           "lists the phase that owns its remaining difference. Each image: lxa | "
           "reference | pen difference.</p>" % (len(rows), n_ident)]
    for r in rows:
        comp = os.path.join(run_dir, r["scenario"], "compare", r["snapshot"], "composite.png")
        out.append("<section><h3><code>%s</code> / %s</h3>" % (
            html.escape(r["scenario"]), html.escape(r["snapshot"])))
        out.append("<p class='%s'>%s%s</p>" % (r["class"], html.escape(r["verdict"]),
                                               " (disabled)" if r["disabled"] else ""))
        if os.path.exists(comp):
            out.append("<img loading='lazy' alt='composite' src='%s'>" %
                       html.escape(os.path.relpath(comp, base)))
        else:
            out.append("<p>(no composite in this run)</p>")
        out.append("</section>")
    out.append("</body></html>")
    with open(html_path, "w") as f:
        f.write("\n".join(out))
    return rows, html_path
