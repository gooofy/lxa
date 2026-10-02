"""Compatibility database (Phase 230).

    python3 -m rdd compat [--run DIR] [--scenarios GLOB...] [--db apps/compat.yaml]
                          [--no-run] [-j N] [--fail-on-drop]
    python3 -m rdd dashboard [--db apps/compat.yaml] [--out build/rdd/dashboard.html]

``compat`` twin-runs the app scenarios (``tests/scenarios/apps/*.yaml``) on
lxa and the reference, compares them (``rdd report``), clusters the divergences
(``rdd cluster``) and rewrites ``apps/compat.yaml``.  With ``--no-run`` it
only consumes an existing run directory.

Every rating is *derived*, never hand-written.  Per scenario:

  untested  the reference itself did not get past launch/the first window
            (no oracle), or no run exists
  garbage   lxa crashed (no result) or never opened the window
  bronze    lxa opened the window but failed a later step the reference passed
  silver    every lxa step passed; tree/stdout divergences remain (each one is
            listed under ``divergences`` with its owning phase)
  gold      tree-equal to the reference in every snapshot; pixel differences
            only, within the golden's pixel budget when a golden exists
  platinum  every snapshot identical to the reference

An app's rating is the worst rating of its (tested) scenarios.  A divergence
is a cluster signature (``rdd cluster``) or a failed step; its phase comes
from a golden ratchet, a reviewed finding or the roadmap, otherwise from the
triage phase (231, "Shallow sweep & divergence ranking") whose job is to give
it a phase of its own.  There is no free-text "known issues" field.

``loop`` calls ``refresh()`` and fails when a rating drops.
"""

import datetime
import glob
import html
import json
import os
import re

import yaml

from rdd.scenario import ROOT, Scenario, ScenarioError

DB_PATH = os.path.join(ROOT, "apps", "compat.yaml")
APP_SCENARIOS = os.path.join(ROOT, "tests", "scenarios", "apps")
DASHBOARD = os.path.join(ROOT, "build", "rdd", "dashboard.html")
RATINGS = ["garbage", "bronze", "silver", "gold", "platinum"]   # worst .. best
UNTESTED = "untested"
TRIAGE_PHASE = 231


def rank(rating):
    return RATINGS.index(rating) if rating in RATINGS else -1


def lxa_version():
    with open(os.path.join(ROOT, "src", "include", "lxa_version.h")) as f:
        m = re.search(r'LXA_VERSION_STRING\s+"([^"]+)"', f.read())
    return m.group(1) if m else "?"


# -- scenario discovery -------------------------------------------------------------

def app_scenarios(paths=None):
    """{app manifest name -> [scenario path relative to ROOT]} for every
    scenario (default: tests/scenarios/apps/*.yaml) that runs a manifest app."""
    if paths is None:
        paths = sorted(glob.glob(os.path.join(APP_SCENARIOS, "*.yaml")))
    out = {}
    for p in paths:
        try:
            scn = Scenario(p)
        except (ScenarioError, OSError, ValueError):
            continue
        if scn.manifest_name:
            out.setdefault(scn.manifest_name, []).append(os.path.relpath(os.path.abspath(p), ROOT))
    return out


# -- rating one scenario ------------------------------------------------------------

def _reached(result, nsteps):
    """Index of the first failed step (nsteps when all passed); -1 = no result."""
    if not result or "steps" not in result:
        return -1
    for i, st in enumerate(result["steps"]):
        if not st.get("ok"):
            return i
    return nsteps if result.get("ok", True) else len(result["steps"])


def _first_error(result):
    if not result:
        return "no result"
    for st in result.get("steps", []):
        if not st.get("ok"):
            return "%s: %s" % (st.get("step"), st.get("error", "failed"))
    return result.get("error", "")


def window_step(steps):
    """Index of the step that proves 'the app is up': the first wait_window,
    else the launch (CLI tools)."""
    kinds = [k for k, _ in steps]
    return kinds.index("wait_window") if "wait_window" in kinds else 0


def rate_scenario(steps, lxa_res, ref_res, compares, golden=None):
    """-> (rating, evidence dict).

    steps:     normalised scenario steps [(kind, args)]
    lxa_res / ref_res: result.json of each backend (None = missing)
    compares:  {snapshot: compare.json dict} for snapshots present on both
    golden:    golden.json dict of this scenario, or None
    """
    n = len(steps)
    w = window_step(steps)
    lr, rr = _reached(lxa_res, n), _reached(ref_res, n)
    ev = {"lxa": "ok" if lr == n else ("crashed (no result)" if lr < 0 else _first_error(lxa_res)),
          "ref": "ok" if rr == n else ("no result" if rr < 0 else _first_error(ref_res))}
    if rr <= w:
        return UNTESTED, dict(ev, reason="reference did not reach step %d (%s)" % (w, steps[w][0]))
    if lr <= w:
        return "garbage", dict(ev, reason="lxa crashed" if lr < 0 else "lxa failed at %s" % steps[max(lr, 0)][0])
    if lr < n and lr < rr:
        return "bronze", dict(ev, reason="lxa failed at step %d (%s), reference passed it" % (lr, steps[lr][0]))
    verdicts = {s: c.get("verdict") for s, c in sorted(compares.items())}
    ev["verdicts"] = verdicts
    if lr < n or not compares:
        return "silver", dict(ev, reason="no snapshot to compare" if not compares else "lxa stopped early")
    if all(v == "identical" for v in verdicts.values()) and rr == n:
        return "platinum", ev
    tree_equal = all(not c.get("tree") for c in compares.values())
    stdout_equal = all(c.get("stdout_equal", True) for c in compares.values())
    if tree_equal and stdout_equal:
        if golden:
            over = [s for s, c in compares.items() if s in golden.get("snapshots", {}) and
                    (c.get("pixels") or {}).get("diff_pixels", 0) > golden["snapshots"][s].get("pixel_budget", 0)]
            if over:
                return "silver", dict(ev, reason="pixel diff above the golden budget in %s" % ", ".join(over))
        return "gold", ev
    return "silver", ev


def rate_app(scenario_ratings):
    """Worst tested scenario rating; untested when none was tested."""
    tested = [r for r in scenario_ratings if r != UNTESTED]
    if not tested:
        return UNTESTED
    return min(tested, key=rank)


# -- reading a run --------------------------------------------------------------------

def _load_json(path):
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def _golden_for(scn):
    from rdd import golden as gmod
    return _load_json(os.path.join(gmod.golden_dir(scn), "golden.json"))


def divergences_for(scenario_name, clusters):
    """[{signature, phase, owned}] of the clusters touching this scenario."""
    out = []
    for c in clusters:
        if scenario_name not in c.get("apps", []):
            continue
        phases = c.get("phases") or []
        out.append({"signature": c["signature"], "phase": phases[0] if phases else TRIAGE_PHASE,
                    "owned": bool(phases)})
    return out


def evaluate_run(run_dir, scenario_paths, clusters):
    """{app: entry} for every app that has at least one scenario in the run."""
    apps = {}
    for app, paths in sorted(scenario_paths.items()):
        per = []
        for rel in paths:
            scn = Scenario(os.path.join(ROOT, rel))
            sdir = os.path.join(run_dir, scn.name)
            if not os.path.isdir(sdir):
                continue
            lres = _load_json(os.path.join(sdir, "lxa", "result.json"))
            rres = _load_json(os.path.join(sdir, "ref", "result.json"))
            compares = {}
            cdir = os.path.join(sdir, "compare")
            if os.path.isdir(cdir):
                for snap in sorted(os.listdir(cdir)):
                    c = _load_json(os.path.join(cdir, snap, "compare.json"))
                    if c is not None:
                        compares[snap] = c
            rating, ev = rate_scenario(scn.steps, lres, rres, compares, _golden_for(scn))
            divs = divergences_for(scn.name, clusters)
            if rating in ("garbage", "bronze"):
                divs.insert(0, {"signature": "%s: %s" % (scn.name, ev["lxa"]), "phase": TRIAGE_PHASE,
                                "owned": False})
            per.append((rel, rating, ev, divs))
        if not per:
            continue
        divs, seen = [], set()
        for _, _, _, ds in per:
            for d in ds:
                if d["signature"] not in seen:
                    seen.add(d["signature"])
                    divs.append(d)
        apps[app] = {"rating": rate_app([r for _, r, _, _ in per]),
                     "scenarios": {rel: dict(ev, rating=r) for rel, r, ev, _ in per},
                     "divergences": divs}
    return apps


# -- the database -------------------------------------------------------------------

def load_db(path=DB_PATH):
    try:
        with open(path) as f:
            return yaml.safe_load(f) or {}
    except OSError:
        return {}


def merge(old_db, evaluated, manifests, version, today=None):
    """New DB: evaluated apps are replaced, others keep their old entry;
    every manifest app is present (untested if never run)."""
    today = today or datetime.date.today().isoformat()
    old_apps = (old_db or {}).get("apps", {}) or {}
    apps = {}
    for name in sorted(manifests, key=str.lower):
        m = manifests[name]
        if name in evaluated:
            e = evaluated[name]
            apps[name] = {"name": m.get("name", name), "version": m.get("version", ""),
                          "rating": e["rating"], "last_tested": version,
                          "scenarios": e["scenarios"], "divergences": e["divergences"]}
        elif name in old_apps:
            apps[name] = old_apps[name]
        else:
            apps[name] = {"name": m.get("name", name), "version": m.get("version", ""),
                          "rating": UNTESTED, "last_tested": None,
                          "scenarios": {}, "divergences": []}
    counts = {r: 0 for r in RATINGS[::-1] + [UNTESTED]}
    for e in apps.values():
        counts[e["rating"]] = counts.get(e["rating"], 0) + 1
    return {"generated": today, "lxa_version": version, "ratings": counts, "apps": apps}


def drops(old_db, new_db):
    """[(app, old rating, new rating)] for every app whose rating got worse."""
    out = []
    old_apps = (old_db or {}).get("apps", {}) or {}
    for name, e in sorted(new_db.get("apps", {}).items()):
        o = old_apps.get(name)
        if o and o.get("rating") in RATINGS and rank(e["rating"]) < rank(o["rating"]):
            out.append((name, o["rating"], e["rating"]))
    return out


HEADER = ("# lxa compatibility database (Phase 230).  GENERATED by `python3 -m rdd compat`\n"
          "# from twin runs of the app scenarios - never edit ratings by hand.\n"
          "# Ratings: platinum > gold > silver > bronze > garbage; untested = no oracle/run.\n"
          "# Each divergence names the roadmap phase that owns it (231 = triage).\n")


def write_db(db, path=DB_PATH):
    with open(path, "w") as f:
        f.write(HEADER)
        yaml.safe_dump(db, f, sort_keys=False, default_flow_style=False, allow_unicode=True, width=110)


def refresh(run_dir, scenario_paths=None, db_path=DB_PATH, clusters=None):
    """Re-derive the DB from a twin run (report and clusters must exist or
    are computed).  Returns (new_db, drops)."""
    from rdd import cluster, corpus, report
    if not os.path.exists(os.path.join(run_dir, "compare-summary.json")):
        report.build(run_dir, None)
    if clusters is None:
        cl_path = os.path.join(run_dir, "clusters", "clusters.json")
        if os.path.exists(cl_path):
            clusters = _load_json(cl_path) or []
        else:
            findings = sorted(glob.glob(os.path.join(ROOT, "doc", "findings", "*.yaml")))
            clusters = cluster.cluster_run(run_dir, findings)
    scenario_paths = scenario_paths if scenario_paths is not None else app_scenarios()
    evaluated = evaluate_run(run_dir, scenario_paths, clusters)
    old = load_db(db_path)
    new = merge(old, evaluated, corpus.apps(corpus.load()), lxa_version())
    write_db(new, db_path)
    return new, drops(old, new)


# -- dashboard ----------------------------------------------------------------------

COLORS = {"platinum": "--c-platinum", "gold": "--c-gold", "silver": "--c-silver", "bronze": "--c-bronze",
          "garbage": "--c-garbage", "untested": "--c-untested"}

CSS = """
:root{--bg:#fbfbfa;--fg:#1d1d1b;--muted:#6b6b66;--line:#e2e1dc;--card:#ffffff;
--c-platinum:#3b6fd6;--c-gold:#c99a12;--c-silver:#8a8f98;--c-bronze:#b06a35;--c-garbage:#c2413b;--c-untested:#d3d2cc}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#171716;--fg:#ecebe6;--muted:#a3a29b;
--line:#33332f;--card:#1f1f1d;--c-platinum:#6d97ea;--c-gold:#e0b432;--c-silver:#a9aeb6;--c-bronze:#cf8a55;
--c-garbage:#e0645d;--c-untested:#4a4a45}}
:root[data-theme="dark"]{--bg:#171716;--fg:#ecebe6;--muted:#a3a29b;--line:#33332f;--card:#1f1f1d;
--c-platinum:#6d97ea;--c-gold:#e0b432;--c-silver:#a9aeb6;--c-bronze:#cf8a55;--c-garbage:#e0645d;--c-untested:#4a4a45}
*{box-sizing:border-box}
body{margin:0;padding:24px 16px;background:var(--bg);color:var(--fg);font:14px/1.45 system-ui,sans-serif}
main{max-width:1100px;margin:0 auto}
h1{font-size:22px;margin:0 0 4px}.sub{color:var(--muted);margin:0 0 20px}
.tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:8px;margin-bottom:12px}
.tile{background:var(--card);border:1px solid var(--line);border-radius:8px;padding:10px 12px}
.tile b{display:block;font-size:22px;font-variant-numeric:tabular-nums}
.tile span{color:var(--muted);font-size:12px;display:flex;align-items:center;gap:6px}
.dot{width:10px;height:10px;border-radius:50%;display:inline-block}
.bar{display:flex;height:12px;border-radius:6px;overflow:hidden;margin:0 0 24px;gap:2px}
.bar div{height:100%}
.wrap{overflow-x:auto}
table{border-collapse:collapse;width:100%;background:var(--card);border:1px solid var(--line);border-radius:8px}
th,td{text-align:left;padding:6px 10px;border-bottom:1px solid var(--line);vertical-align:top}
th{font-size:12px;color:var(--muted);font-weight:600}
td.r{white-space:nowrap}
.badge{display:inline-flex;align-items:center;gap:6px;font-weight:600}
ul{margin:0;padding-left:16px}li{margin:1px 0}
.ph{font-variant-numeric:tabular-nums;color:var(--muted);white-space:nowrap}
code{font-size:12px}
"""


def render_dashboard(db, path=DASHBOARD):
    apps = db.get("apps", {})
    counts = db.get("ratings", {})
    total = sum(counts.values()) or 1
    order = RATINGS[::-1] + [UNTESTED]
    parts = ["<!doctype html><html lang=en><head><meta charset=utf-8>",
             "<meta name=viewport content='width=device-width,initial-scale=1'>",
             "<title>lxa Compatibility</title><style>%s</style></head><body><main>" % CSS,
             "<h1>lxa app compatibility</h1>",
             "<p class=sub>%d apps &middot; lxa %s &middot; generated %s from <code>apps/compat.yaml</code> "
             "(ratings derived from twin runs against AmigaOS 3.1)</p>" % (
                 len(apps), html.escape(str(db.get("lxa_version"))), html.escape(str(db.get("generated"))))]
    parts.append("<div class=tiles>")
    for r in order:
        parts.append("<div class=tile><b>%d</b><span><i class=dot style='background:var(%s)'></i>%s</span></div>"
                     % (counts.get(r, 0), COLORS[r], r))
    parts.append("</div><div class=bar role=img aria-label='rating distribution'>")
    for r in order:
        if counts.get(r):
            parts.append("<div title='%s: %d' style='width:%.2f%%;background:var(%s)'></div>"
                         % (r, counts[r], 100.0 * counts[r] / total, COLORS[r]))
    parts.append("</div><div class=wrap><table><tr><th>app</th><th>rating</th><th>version</th>"
                 "<th>last tested</th><th>scenarios</th><th>divergences (owning phase)</th></tr>")
    for name, e in sorted(apps.items(), key=lambda kv: (-rank(kv[1]["rating"]), kv[0].lower())):
        scen = "".join("<li><code>%s</code> %s</li>" % (html.escape(os.path.basename(s)), html.escape(v.get("rating", "")))
                       for s, v in (e.get("scenarios") or {}).items())
        divs = "".join("<li><span class=ph>Phase %s%s</span> %s</li>" % (
            d.get("phase"), "" if d.get("owned") else " (triage)", html.escape(d["signature"]))
            for d in (e.get("divergences") or [])[:12])
        more = len(e.get("divergences") or []) - 12
        if more > 0:
            divs += "<li>&hellip; %d more</li>" % more
        parts.append("<tr><td><b>%s</b><br><span class=ph>%s</span></td>"
                     "<td class=r><span class=badge><i class=dot style='background:var(%s)'></i>%s</span></td>"
                     "<td>%s</td><td>%s</td><td><ul>%s</ul></td><td><ul>%s</ul></td></tr>" % (
                         html.escape(name), html.escape(str(e.get("name", ""))), COLORS.get(e["rating"], "--c-untested"),
                         e["rating"], html.escape(str(e.get("version", ""))), html.escape(str(e.get("last_tested") or "-")),
                         scen, divs))
    parts.append("</table></div></main></body></html>")
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w") as f:
        f.write("\n".join(parts))
    return path
