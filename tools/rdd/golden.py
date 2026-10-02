"""Reference goldens (Phase 215).

    python3 -m rdd promote <scenario.yaml> [--run DIR] --phase N
    python3 -m rdd golden <tests/golden/<app>/<scenario>> [--build DIR]
    python3 -m rdd golden --lint

A golden is a **reference** bundle checked into the repository:

    tests/golden/<app>/<scenario>/
        scenario.yaml        copy of the scenario (the golden replays this one)
        golden.json          ratchet: known divergences + pixel budget
        ref/<snapshot>/      reference bundle(s) (doc/rdd-snapshot.md)

`golden` replays the scenario on lxa and compares every snapshot with the
reference bundle.  It passes when lxa is at least as close to the reference
as when the golden was promoted:

  * every tree difference must be listed in `known_tree_diffs` with the same
    lxa value (each one carries the roadmap phase that owns the fix - AGENTS
    "Every Remaining Gap Is Visible");
  * the pen-index pixel difference must stay within `pixel_budget` (owned by
    `pixel_phase`);
  * stdout must equal the reference unless `stdout_known_diff` names a phase.

A known difference that disappears, or a pixel count below the budget, is
reported as `tighten` (re-run `promote` to ratchet the golden).  Goldens are
never edited to match lxa (RDD principle 4): only the lxa side of the ratchet
is recorded.

A golden may be quarantined only by a phase whose objective is equivalence:
`"disabled": {"phase": N, "reason": "..."}` (CTest marks it DISABLED; the
lint checks that the phase exists in roadmap.md).

Exit codes of `golden`: 0 pass, 1 regression, 77 skipped (app not installed).
"""

import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

from rdd.compare import compare_bundles
from rdd.scenario import APPS_DIR, ROOT, Scenario

GOLDEN_ROOT = os.path.join(ROOT, "tests", "golden")
BUNDLE_FILES = None   # copy the whole bundle directory


def app_name(scn):
    return scn.manifest_name or os.path.basename(scn.sample)


def golden_dir(scn, root=GOLDEN_ROOT):
    return os.path.join(root, app_name(scn), scn.name)


def _key(d):
    return (d["kind"], d["where"], d["field"], json.dumps(d["lxa"], sort_keys=True))


def _version():
    with open(os.path.join(ROOT, "src", "include", "lxa_version.h")) as f:
        m = re.search(r'LXA_VERSION_STRING\s+"([^"]+)"', f.read())
    return m.group(1) if m else "?"


# -- promote ----------------------------------------------------------------------

def promote(scenario_path, run_dir, phase, root=GOLDEN_ROOT, reason=None):
    """Copy the reference bundles of a twin run into a golden and record the
    current lxa divergence as the ratchet.  Returns the golden directory."""
    scn = Scenario(scenario_path)
    sdir = os.path.join(run_dir, scn.name)
    with open(os.path.join(sdir, "ref", "result.json")) as f:
        ref_res = json.load(f)
    if not ref_res.get("ok"):
        raise RuntimeError("reference run of %s failed; not promoting" % scn.name)
    gdir = golden_dir(scn, root)
    old = {}
    if os.path.exists(os.path.join(gdir, "golden.json")):
        with open(os.path.join(gdir, "golden.json")) as f:
            old = json.load(f)
        shutil.rmtree(os.path.join(gdir, "ref"), ignore_errors=True)
    os.makedirs(os.path.join(gdir, "ref"), exist_ok=True)
    shutil.copyfile(scn.path, os.path.join(gdir, "scenario.yaml"))
    snaps = {}
    for snap in ref_res.get("snapshots", []):
        shutil.copytree(os.path.join(sdir, "ref", snap), os.path.join(gdir, "ref", snap))
        lpath = os.path.join(sdir, "lxa", snap)
        entry = {"known_tree_diffs": [], "pixel_budget": 0, "pixel_phase": None, "stdout_known_diff": None}
        if os.path.exists(os.path.join(lpath, "meta.json")):
            res, _ = compare_bundles(lpath, os.path.join(gdir, "ref", snap))
            prev = {_key(d): d.get("phase") for d in
                    old.get("snapshots", {}).get(snap, {}).get("known_tree_diffs", [])}
            entry["known_tree_diffs"] = [dict(d, phase=prev.get(_key(d)) or phase) for d in res["tree"]]
            entry["pixel_budget"] = res.get("pixels", {}).get("diff_pixels") or 0
            if res.get("pixels", {}).get("size_mismatch"):
                entry["known_lxa_size"] = res["pixels"]["lxa_size"]
            entry["pixel_phase"] = (old.get("snapshots", {}).get(snap, {}).get("pixel_phase") or phase
                                    if entry["pixel_budget"] else None)
            if not res["stdout_equal"]:
                entry["stdout_known_diff"] = phase
        else:
            raise RuntimeError("no lxa bundle %s (run the twin runner with --backend both)" % lpath)
        snaps[snap] = entry
    doc = {"scenario": scn.name, "app": app_name(scn), "requires_apps": bool(scn.manifest),
           "promoted": datetime.date.today().isoformat(), "lxa_version": _version(),
           "reference": "AmigaOS 3.1 (KS 40.70), profile %s" % scn.profile,
           "snapshots": snaps, "disabled": old.get("disabled")}
    if reason:
        doc["note"] = reason
    with open(os.path.join(gdir, "golden.json"), "w") as f:
        json.dump(doc, f, indent=1, sort_keys=True)
        f.write("\n")
    return gdir


# -- check ------------------------------------------------------------------------

def evaluate(golden, snap, res):
    """Apply one snapshot's ratchet to a compare result -> (failures, tighten)."""
    entry = golden["snapshots"][snap]
    known = {_key(d): d for d in entry.get("known_tree_diffs", [])}
    fails, tighten = [], []
    seen = set()
    for d in res["tree"]:
        k = _key(d)
        if k in known:
            seen.add(k)
        else:
            fails.append("%s: new tree diff %s.%s lxa=%r ref=%r" % (snap, d["where"], d["field"], d["lxa"], d["ref"]))
    for k, d in known.items():
        if k not in seen:
            tighten.append("%s: known diff gone: %s.%s (phase %s)" % (snap, d["where"], d["field"], d.get("phase")))
    px = res.get("pixels")
    budget = entry.get("pixel_budget", 0)
    if px is not None:
        if px.get("size_mismatch") and px["lxa_size"] != entry.get("known_lxa_size"):
            fails.append("%s: screen size %s vs reference %s" % (snap, px["lxa_size"], px["ref_size"]))
        elif not px.get("size_mismatch") and entry.get("known_lxa_size"):
            tighten.append("%s: snapshot size now equals the reference" % snap)
        if px["diff_pixels"] > budget:
            fails.append("%s: %d pixels differ, budget %d" % (snap, px["diff_pixels"], budget))
        elif px["diff_pixels"] < budget:
            tighten.append("%s: %d pixels differ, budget %d" % (snap, px["diff_pixels"], budget))
    if not res["stdout_equal"] and not entry.get("stdout_known_diff"):
        fails.append("%s: stdout differs from the reference" % snap)
    return fails, tighten


def check(gdir, build=None, keep=None):
    with open(os.path.join(gdir, "golden.json")) as f:
        golden = json.load(f)
    scn = Scenario(os.path.join(gdir, "scenario.yaml"))
    if scn.manifest and not os.path.isdir(scn.app_host_dir()):
        print("SKIP %s: %s not installed (LXA_APPS=%s)" % (scn.name, scn.manifest["dir"], APPS_DIR))
        return 77
    out = keep or tempfile.mkdtemp(prefix="rdd-golden-")
    env = dict(os.environ, PYTHONPATH=os.path.join(ROOT, "tools") + os.pathsep + os.environ.get("PYTHONPATH", ""))
    cmd = [sys.executable, "-m", "rdd.backend_lxa", scn.path, out]
    if build:
        cmd += ["--build", build]
    proc = subprocess.run(cmd, env=env, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          universal_newlines=True)
    try:
        with open(os.path.join(out, "result.json")) as f:
            result = json.load(f)
    except (OSError, ValueError):
        print(proc.stdout[-4000:])
        print("FAIL %s: lxa run produced no result" % scn.name)
        return 1
    fails, tighten = [], []
    if not result.get("ok"):
        bad = [s for s in result["steps"] if not s.get("ok")]
        fails.append("lxa run failed: %s" % (bad[0] if bad else result.get("error")))
    for snap in golden["snapshots"]:
        lpath = os.path.join(out, snap)
        if not os.path.exists(os.path.join(lpath, "meta.json")):
            fails.append("%s: no lxa snapshot" % snap)
            continue
        res, _ = compare_bundles(lpath, os.path.join(gdir, "ref", snap))
        f, t = evaluate(golden, snap, res)
        fails += f
        tighten += t
    for t in tighten:
        print("tighten %s/%s: %s" % (golden["app"], golden["scenario"], t))
    for f in fails:
        print("FAIL %s/%s: %s" % (golden["app"], golden["scenario"], f))
    if not keep:
        shutil.rmtree(out, ignore_errors=True)
    if fails:
        return 1
    print("PASS %s/%s" % (golden["app"], golden["scenario"]))
    return 0


# -- lint -------------------------------------------------------------------------

def all_goldens(root=GOLDEN_ROOT):
    out = []
    if os.path.isdir(root):
        for app in sorted(os.listdir(root)):
            for scn in sorted(os.listdir(os.path.join(root, app))):
                d = os.path.join(root, app, scn)
                if os.path.exists(os.path.join(d, "golden.json")):
                    out.append(d)
    return out


def lint(root=GOLDEN_ROOT, roadmap=os.path.join(ROOT, "roadmap.md")):
    """Every golden is well-formed and every phase it names is scheduled."""
    with open(roadmap) as f:
        phases = set(re.findall(r"^### Phase (\d+)", f.read(), re.M))
    problems = []
    for d in all_goldens(root):
        with open(os.path.join(d, "golden.json")) as f:
            g = json.load(f)
        rel = os.path.relpath(d, root)
        for snap, e in g["snapshots"].items():
            if not os.path.exists(os.path.join(d, "ref", snap, "meta.json")):
                problems.append("%s: missing reference bundle %s" % (rel, snap))
            for k in e.get("known_tree_diffs", []):
                if str(k.get("phase")) not in phases:
                    problems.append("%s: known diff %s.%s owned by unscheduled phase %s"
                                    % (rel, k["where"], k["field"], k.get("phase")))
            if e.get("pixel_budget") and str(e.get("pixel_phase")) not in phases:
                problems.append("%s: pixel budget of %s owned by unscheduled phase %s"
                                % (rel, snap, e.get("pixel_phase")))
            if e.get("stdout_known_diff") and str(e["stdout_known_diff"]) not in phases:
                problems.append("%s: stdout diff owned by unscheduled phase %s" % (rel, e["stdout_known_diff"]))
        dis = g.get("disabled")
        if dis and str(dis.get("phase")) not in phases:
            problems.append("%s: disabled by unscheduled phase %s" % (rel, dis.get("phase")))
    return problems
