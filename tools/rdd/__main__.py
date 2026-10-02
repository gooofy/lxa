"""rdd - reference-driven development twin runner (Phases 213+).

    cd tools && python3 -m rdd run <scenario.yaml>... [--backend lxa|ref|both]
                                   [--out DIR] [-j N] [--no-cache]

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


def main():
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
    a = ap.parse_args()
    a.scenarios = [os.path.abspath(s) for s in getattr(a, "scenarios", [])]
    a.out = os.path.abspath(a.out)
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
