"""rdd - reference-driven development twin runner (Phases 213+).

    cd tools && python3 -m rdd run <scenario.yaml>... [--backend lxa|ref|both]
                                   [--out DIR] [-j N] [--no-cache]
               python3 -m rdd report [DIR] [--html FILE]
               python3 -m rdd promote <scenario.yaml>... --phase N [--run DIR]
               python3 -m rdd golden <golden-dir>... | --all | --lint
               python3 -m rdd cluster [DIR] [--findings F.yaml...]
               python3 -m rdd suite-ref [--filter RX] [--capture]
               python3 -m rdd loop [scenario.yaml...] [--out DIR] [--report FILE]

(or tools/rdd.sh run ...). Each scenario runs once per backend in its own
process; reference runs are cached (see rdd.backend_ref).  Output:

    <out>/<scenario>/<backend>/<snapshot>/   snapshot bundles
    <out>/<scenario>/<backend>/result.json   per-step results
    <out>/summary.json
"""

import argparse
import concurrent.futures
import json
import os
import subprocess
import sys

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(TOOLS)


def run_backend(backend, scenario, out, build, no_cache):
    from rdd.scenario import Scenario
    scn = Scenario(scenario)
    bout = os.path.join(out, scn.name, backend)
    cmd = [sys.executable, "-m", "rdd.backend_%s" % backend, scenario, bout]
    if build:
        cmd += ["--build", build]
    if no_cache and backend == "ref":
        cmd.append("--no-cache")
    env = dict(os.environ, PYTHONPATH=TOOLS + os.pathsep + os.environ.get("PYTHONPATH", ""))
    log = os.path.join(out, scn.name, "%s.log" % backend)
    os.makedirs(os.path.dirname(log), exist_ok=True)
    with open(log, "w") as lf:
        subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT, env=env, cwd=ROOT)
    try:
        with open(os.path.join(bout, "result.json")) as f:
            res = json.load(f)
    except (OSError, ValueError):
        res = {"backend": backend, "scenario": scn.name, "ok": False,
               "error": "no result (see %s)" % os.path.relpath(log, out)}
    return res


def cmd_run(a):
    backends = ["lxa", "ref"] if a.backend == "both" else [a.backend]
    os.makedirs(a.out, exist_ok=True)
    jobs = [(b, s) for s in a.scenarios for b in backends]
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as ex:
        futs = {ex.submit(run_backend, b, s, a.out, a.build, a.no_cache): (b, s) for b, s in jobs}
        for fut in concurrent.futures.as_completed(futs):
            r = fut.result()
            results.append(r)
            failed = [st for st in r.get("steps", []) if not st.get("ok")]
            print("%-32s %-4s %s %s%s" % (r.get("scenario"), r.get("backend"),
                                          "OK  " if r.get("ok") else "FAIL",
                                          ",".join(r.get("snapshots", [])),
                                          (" (cached)" if r.get("cached") else "") +
                                          ("  " + (failed[0].get("error") if failed else r.get("error", ""))
                                           if not r.get("ok") else "")))
    with open(os.path.join(a.out, "summary.json"), "w") as f:
        json.dump(sorted(results, key=lambda r: (r.get("scenario", ""), r.get("backend", ""))), f, indent=1)
    return 0 if all(r.get("ok") for r in results) else 1


def cmd_report(a):
    from rdd import report
    rows = report.build(a.out, a.html)
    for r in rows:
        print("%-28s %-12s %-9s tree=%-3d pixels=%s" % (r["scenario"], r["snapshot"], r["verdict"],
                                                         r["tree_diffs"], r["diff_pixels"]))
    print("report: %s" % (a.html or os.path.join(a.out, "report.html")))
    return 0


def cmd_promote(a):
    from rdd import golden
    for s in a.scenarios:
        print("promoted %s" % os.path.relpath(golden.promote(s, a.run, a.phase, reason=a.note), ROOT))
    return 0


def cmd_golden(a):
    from rdd import golden
    if a.lint:
        probs = golden.lint()
        for p in probs:
            print(p)
        print("%d goldens, %d problems" % (len(golden.all_goldens()), len(probs)))
        return 1 if probs else 0
    dirs = golden.all_goldens() if a.all else [os.path.abspath(d) for d in a.dirs]
    build = os.path.abspath(a.build) if a.build else None
    rcs = [golden.check(d, build, a.keep) for d in dirs]
    return 1 if 1 in rcs else (77 if rcs and all(rc == 77 for rc in rcs) else 0)


def cmd_loop(a):
    """One autonomous iteration (Phase 216): twin-run, compare, replay the
    goldens, cluster divergences, write a sweep report."""
    import datetime
    import glob
    from rdd import cluster, golden, report
    scen = a.scenarios or sorted(glob.glob(os.path.join(ROOT, "tests", "scenarios", "*.yaml")))
    a.scenarios = [os.path.abspath(s) for s in scen]
    a.backend, a.no_cache = "both", False
    run_rc = cmd_run(a)
    rows = report.build(a.out, None)
    gres = {}
    for d in golden.all_goldens():
        gres[os.path.relpath(d, golden.GOLDEN_ROOT)] = golden.check(d, a.build)
    findings = sorted(glob.glob(os.path.join(ROOT, "doc", "findings", "*.yaml")))
    cl = cluster.cluster_run(a.out, findings)
    cdir = os.path.join(a.out, "clusters")
    os.makedirs(cdir, exist_ok=True)
    with open(os.path.join(cdir, "clusters.json"), "w") as f:
        json.dump(cl, f, indent=1)
    stubs = cluster.phase_stubs(cl, a.first_phase)
    with open(os.path.join(cdir, "phase-stubs.md"), "w") as f:
        f.write(stubs)
    today = datetime.date.today().isoformat()
    md = ["# RDD loop %s" % today, "",
          "Generated by `python3 -m rdd loop` (Phase 216). Twin run of %d scenarios." % len(scen), "",
          "## Scenarios", "", "| scenario | snapshot | verdict | tree diffs | pixel diffs |", "|---|---|---|---|---|"]
    md += ["| %s | %s | %s | %d | %s |" % (r["scenario"], r["snapshot"], r["verdict"], r["tree_diffs"],
                                          r["diff_pixels"]) for r in rows]
    md += ["", "## Goldens", "", "| golden | result |", "|---|---|"]
    md += ["| %s | %s |" % (k, {0: "pass", 1: "**FAIL**", 77: "skipped"}.get(v, v)) for k, v in sorted(gres.items())]
    md += ["", "## Divergence clusters (>= 2 scenarios)", "", "| scenarios | owner | signature | suspect |",
           "|---|---|---|---|"]
    md += ["| %d | %s | %s | %s |" % (len(c["apps"]), ", ".join(map(str, c["phases"])) or "**unowned**",
                                     c["signature"].replace("|", "/"), " / ".join(c["suspect"] or []))
           for c in cl if len(c["apps"]) >= 2]
    md += ["", "## Phase stubs", "", stubs.split("\n", 3)[-1]]
    path = a.report or os.path.join(a.out, "loop-%s.md" % today)
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w") as f:
        f.write("\n".join(md))
    print("loop report: %s" % path)
    failed_goldens = [k for k, v in gres.items() if v == 1]
    unowned = [c for c in cl if not c["owned"] and len(c["apps"]) >= 2]
    print("goldens failing: %d, unowned clusters: %d" % (len(failed_goldens), len(unowned)))
    return 1 if failed_goldens or unowned or run_rc else 0


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "suite-ref":
        from rdd import suite_ref
        return suite_ref.main(sys.argv[2:])
    if len(sys.argv) > 1 and sys.argv[1] == "cluster":
        from rdd import cluster
        return cluster.main(sys.argv[2:])
    ap = argparse.ArgumentParser(prog="rdd")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("run", help="run scenarios on lxa and/or the reference")
    p.add_argument("scenarios", nargs="+")
    p.add_argument("--backend", default="both", choices=["lxa", "ref", "both"])
    p.add_argument("--out", default=os.path.join(ROOT, "build", "rdd"))
    p.add_argument("--build", default=None)
    p.add_argument("-j", "--jobs", type=int, default=8)
    p.add_argument("--no-cache", action="store_true")
    p.set_defaults(fn=cmd_run)
    p = sub.add_parser("report", help="compare lxa and reference bundles of a run")
    p.add_argument("out", nargs="?", default=os.path.join(ROOT, "build", "rdd"))
    p.add_argument("--html")
    p.set_defaults(fn=cmd_report)
    p = sub.add_parser("promote", help="turn reference bundles of a run into goldens")
    p.add_argument("scenarios", nargs="+")
    p.add_argument("--run", default=os.path.join(ROOT, "build", "rdd"))
    p.add_argument("--phase", type=int, required=True, help="phase owning the current divergence")
    p.add_argument("--note")
    p.set_defaults(fn=cmd_promote)
    p = sub.add_parser("loop", help="run, compare, check goldens, cluster, report")
    p.add_argument("scenarios", nargs="*")
    p.add_argument("--out", default=os.path.join(ROOT, "build", "rdd"))
    p.add_argument("--build", default=None)
    p.add_argument("-j", "--jobs", type=int, default=8)
    p.add_argument("--report")
    p.add_argument("--first-phase", type=int, default=290)
    p.set_defaults(fn=cmd_loop)
    p = sub.add_parser("golden", help="replay goldens on lxa")
    p.add_argument("dirs", nargs="*")
    p.add_argument("--all", action="store_true")
    p.add_argument("--lint", action="store_true")
    p.add_argument("--build")
    p.add_argument("--keep", help="keep the lxa bundles in this directory")
    p.set_defaults(fn=cmd_golden)
    a = ap.parse_args()
    a.scenarios = [os.path.abspath(s) for s in getattr(a, "scenarios", [])]
    if getattr(a, "build", None):
        a.build = os.path.abspath(a.build)
    if getattr(a, "report", None):
        a.report = os.path.abspath(a.report)
    if hasattr(a, "out"):
        a.out = os.path.abspath(a.out)
    if hasattr(a, "run"):
        a.run = os.path.abspath(a.run)
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
