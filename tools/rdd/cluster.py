"""Cluster divergences across scenarios by root cause (Phase 216).

    python3 -m rdd cluster [RUN_DIR] [--findings F.yaml ...] [--out DIR]

Input: the compare results of a twin run (`rdd report` output) and any
reviewed `findings.yaml`.  Output (`--out`, default <run>/clusters):

    clusters.json      every cluster: signature, suspect, apps, evidence, owner
    phase-stubs.md     a roadmap phase stub for each unowned cluster that
                       affects >= 2 apps (the coordinator reviews these with
                       the user before they go into roadmap.md)

Tree differences are clustered mechanically: the same (kind, field, lxa
value, reference value) in several scenarios is one root cause.  Gadget and
menu geometry diffs are folded per scenario (a layout shift moves every
gadget; one cluster, not 40).  Reviewed findings are clustered by their
`suspect` (library/function) and category.  A cluster is *owned* when a golden
records one of its diffs as a known divergence with a phase, a finding names
a phase, or the roadmap already mentions its signature.
"""

import argparse
import json
import os
import re
from collections import defaultdict

import yaml

from rdd.scenario import ROOT

SUSPECT = {
    "screen": ("intuition", "OpenScreenTagList"),
    "window": ("intuition", "OpenWindowTagList"),
    "sysgadget": ("intuition", "OpenWindowTagList (system gadgets)"),
    "gadget": ("gadtools/intuition", "CreateGadgetA / AddGList"),
    "menu": ("intuition/gadtools", "SetMenuStrip / LayoutMenusA"),
    "item": ("intuition/gadtools", "SetMenuStrip / LayoutMenusA"),
    "palette": ("graphics/intuition", "screen colour table"),
    "text": ("application rendering", "Text()"),
    "stdout": ("dos", "output"),
}

GEOMETRY = {"left", "top", "width", "height"}
BITFIELDS = {"flags", "type", "activation", "idcmp"}


def _fmt(v):
    return json.dumps(v, sort_keys=True) if not isinstance(v, str) else v


def signatures(scenario, res):
    """compare.json -> [(signature, suspect, evidence, golden key or None)]"""
    out = []
    folded = defaultdict(int)
    folded_keys = {}
    for d in res.get("tree", []):
        kind = "sysgadget" if "/sysgadget[" in d["where"] else d["kind"]
        if kind in ("gadget", "item", "menu") and d["field"] in GEOMETRY:
            folded[(kind, d["field"])] += 1
            folded_keys.setdefault((kind, d["field"]), (scenario, d["where"], d["field"]))
            continue
        if d["field"] == "text":
            sig = "%s.text differs" % kind
        elif d["field"] in BITFIELDS and isinstance(d["lxa"], int) and isinstance(d["ref"], int):
            miss, extra = d["ref"] & ~d["lxa"], d["lxa"] & ~d["ref"]
            sig = "%s.%s:%s%s" % (kind, d["field"], " lxa lacks 0x%x" % miss if miss else "",
                                 " lxa adds 0x%x" % extra if extra else "")
        elif d["field"] == "missing" and kind == "sysgadget":
            sig = "sysgadget type 0x%04x missing in lxa" % d["ref"]
        elif d["field"] in ("max_width", "max_height") and d["lxa"] == 65535:
            sig = "%s.%s: lxa 65535, reference = window size" % (kind, d["field"])
        else:
            sig = "%s.%s: lxa %s, reference %s" % (kind, d["field"], _fmt(d["lxa"]), _fmt(d["ref"]))
        out.append((sig, SUSPECT.get(kind), "%s %s.%s lxa=%s ref=%s" % (
            scenario, d["where"], d["field"], _fmt(d["lxa"]), _fmt(d["ref"])),
            (scenario, d["where"], d["field"])))
    for (kind, field), n in sorted(folded.items()):
        out.append(("%s geometry (%s) differs" % (kind, field), SUSPECT.get(kind),
                    "%s: %d %s.%s diffs" % (scenario, n, kind, field), folded_keys[(kind, field)]))
    for p in res.get("palette", []):
        out.append(("palette pen %d: lxa %s, reference %s" % (p["pen"], p["lxa"], p["ref"]),
                    SUSPECT["palette"], "%s pen %d" % (scenario, p["pen"]), None))
    missing = res.get("text", {}).get("missing_in_lxa") or []
    if missing:
        out.append(("%s: Text() strings missing in lxa" % scenario, SUSPECT["text"],
                    "%s: %s" % (scenario, ", ".join(missing[:8])), None))
    if not res.get("stdout_equal", True):
        out.append(("%s: stdout differs" % scenario, SUSPECT["stdout"], scenario, None))
    return out


def golden_owners():
    """{evidence key -> phase} from the known divergences recorded in goldens."""
    from rdd import golden
    owners = {}
    for gdir in golden.all_goldens():
        with open(os.path.join(gdir, "golden.json")) as f:
            g = json.load(f)
        for e in g["snapshots"].values():
            for d in e.get("known_tree_diffs", []):
                owners[(g["scenario"], d["where"], d["field"])] = d.get("phase")
    return owners


def cluster_run(run_dir, finding_files=(), roadmap=os.path.join(ROOT, "roadmap.md")):
    with open(os.path.join(run_dir, "compare-summary.json")) as f:
        rows = json.load(f)
    with open(roadmap) as f:
        rm = f.read()
    clusters = {}
    owners = golden_owners()

    def add(sig, suspect, scenario, evidence, phase=None, category=None):
        c = clusters.setdefault(sig, {"signature": sig, "suspect": suspect, "apps": [], "evidence": [],
                                      "phases": [], "category": category})
        if scenario not in c["apps"]:
            c["apps"].append(scenario)
        c["evidence"].append(evidence)
        if phase and phase not in c["phases"]:
            c["phases"].append(phase)

    for r in rows:
        with open(os.path.join(run_dir, r["compare"])) as f:
            res = json.load(f)
        for sig, suspect, ev, key in signatures(r["scenario"], res):
            add(sig, list(suspect) if suspect else None, r["scenario"], ev, owners.get(key))
    for path in finding_files:
        with open(path) as f:
            doc = yaml.safe_load(f)
        for fd in doc.get("findings", []):
            sus = fd.get("suspect") or {}
            sig = "finding: %s %s/%s" % (fd.get("category"), sus.get("library"), sus.get("function"))
            scn = fd.get("scenario") or doc.get("scenario")
            add(sig, [sus.get("library"), sus.get("function")], scn,
                "%s: %s" % (fd["id"], fd["description"].strip().split("\n")[0]),
                (fd.get("action") or {}).get("phase"), fd.get("category"))
    out = sorted(clusters.values(), key=lambda c: (-len(c["apps"]), c["signature"]))
    for c in out:
        if not c["phases"]:
            c["phases"] = owning_phases(c, rm)
        c["owned"] = bool(c["phases"])
    return out


def owning_phases(c, roadmap_text):
    """Phases whose roadmap text already names this divergence."""
    probes = []
    m = re.match(r"(\w+)\.(\w+): lxa (.*), reference (.*)", c["signature"])
    pm = re.match(r"palette pen \d+: lxa \[(\d+), (\d+), (\d+)\]", c["signature"])
    if pm:
        probes.append("`%s,%s,%s`" % pm.groups())
    if m:
        probes += ["`%s`" % m.group(3), m.group(3)] if len(m.group(3)) > 3 else []
        if re.match(r"^\d+$", m.group(3)) and int(m.group(3)) > 9:
            probes.append("0x%x" % int(m.group(3)))
        probes += ["%s %s" % (m.group(2), m.group(3))]
    phases = []
    cur = None
    for line in roadmap_text.split("\n"):
        h = re.match(r"^### Phase (\d+)", line)
        if h:
            cur = int(h.group(1))
            continue
        if cur and any(p and p in line for p in probes) and cur not in phases:
            phases.append(cur)
    return phases


def phase_stubs(clusters, first_number):
    parts = ["# Phase stubs generated by `rdd cluster`", "",
             "Review with the user, then move accepted stubs into `roadmap.md` "
             "(renumber to fit the milestone). Clusters with one app stay as TODOs "
             "of the app's phase.", ""]
    n = first_number
    for c in clusters:
        if c["owned"] or len(c["apps"]) < 2:
            continue
        lib, fn = c["suspect"] or ("?", "?")
        parts += ["### Phase %d — %s: %s" % (n, lib, c["signature"]),
                  "**Class**: Compatibility. Seen in %d scenarios: %s." % (len(c["apps"]), ", ".join(c["apps"])),
                  "- [ ] Suspect `%s` / `%s`. Confirm on the reference (probe or trace) before changing lxa." % (lib, fn)]
        parts += ["- [ ] Evidence: %s" % e for e in c["evidence"][:6]]
        parts += ["", "**Test gate**: the divergence disappears from every listed golden "
                      "(`rdd golden` prints `tighten`; re-promote).", ""]
        n += 1
    if n == first_number:
        parts.append("(no unowned multi-app clusters)")
    return "\n".join(parts) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(prog="rdd cluster")
    ap.add_argument("run", nargs="?", default=os.path.join(ROOT, "build", "rdd"))
    ap.add_argument("--findings", nargs="*", default=[])
    ap.add_argument("--out")
    ap.add_argument("--first-phase", type=int, default=290)
    a = ap.parse_args(argv)
    out = a.out or os.path.join(a.run, "clusters")
    os.makedirs(out, exist_ok=True)
    cl = cluster_run(a.run, a.findings)
    with open(os.path.join(out, "clusters.json"), "w") as f:
        json.dump(cl, f, indent=1)
    with open(os.path.join(out, "phase-stubs.md"), "w") as f:
        f.write(phase_stubs(cl, a.first_phase))
    for c in cl:
        print("%-3d %-8s %s" % (len(c["apps"]), ",".join(map(str, c["phases"])) or "UNOWNED", c["signature"][:110]))
    print("%d clusters, %d unowned with >=2 scenarios -> %s" % (
        len(cl), sum(1 for c in cl if not c["owned"] and len(c["apps"]) >= 2), os.path.join(out, "phase-stubs.md")))
    return 0
