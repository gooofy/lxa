"""Run lxa's m68k test programs on the AmigaOS 3.1 reference (Phase 220).

    python3 -m rdd suite-ref [--filter REGEX] [--out DIR] [-j N] [--no-cache]
                             [--capture]      # write expected.ref.out files

Every test program listed in samples/CMakeLists.txt (`<src>|main|Tests/...`)
runs once on the reference (exit code + stdout) and once on lxa.  Reference
runs are batched: each pool instance boots once and runs a chunk of programs
with RUN + WAIT_EXIT; a hang or a crash reboots the instance.  Results are
cached per binary in ~/.cache/lxa/rdd/suite-ref/.

Classification per program (doc: roadmap Phase 220):
  pass         rc 0 on the reference and on lxa, output equal
  output       both rc 0, stdout differs (after normalisation)
  ref-fail     non-zero rc on the reference: the test's expectation is wrong
               (or it depends on lxa-only behaviour -> tests/ref_suite.yaml)
  ref-hang     no exit on the reference within the timeout (interactive /
               needs a host driver)
  ref-crash    the program crashed the reference (task held or system hang)
  interactive  needs a host-side driver (tests/ref_suite.yaml, owning phase)
  lxa-only     depends on lxa-only behaviour (tests/ref_suite.yaml, reason)
  lxa-fail     reference passes, lxa does not

API conformance probes (tests/probes/<lib>/<name>.c, Phase 222) are
included; `--capture-ref` writes their <name>.ref.out from the reference
alone.  `--capture` writes tests/<area>/<test>/expected.ref.out from the
**reference** output of every program classified pass/output whose output is
deterministic (identical on two reference runs).
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

from rdd.backend_ref import (CACHE as REF_CACHE, EPOCH, REFCTL, REFSYS, Agent, AgentError,
                             refsys_fingerprint, take_instance)
from rdd.scenario import ROOT

CACHE = os.path.join(os.path.dirname(REF_CACHE), "suite-ref")
CONFIG = os.path.join(ROOT, "tests", "ref_suite.yaml")


def programs(build):
    out = []
    with open(os.path.join(ROOT, "samples", "CMakeLists.txt")) as f:
        for m in re.finditer(r'"\.\./(tests/[^|"]+)\|main\|(Tests/[^"]+)"', f.read()):
            src, rel = m.group(1), m.group(2)
            host = os.path.join(build, "target", "samples", "Samples", rel)
            if os.path.exists(host):
                out.append({"name": rel, "src": src, "host": host,
                            "expected": os.path.join(ROOT, src, "expected.ref.out")})
    # API conformance probes (Phase 222): tests/probes/<lib>/<name>.c
    pdir = os.path.join(ROOT, "tests", "probes")
    for lib in sorted(os.listdir(pdir)) if os.path.isdir(pdir) else []:
        if not os.path.isdir(os.path.join(pdir, lib)):
            continue
        for fn in sorted(os.listdir(os.path.join(pdir, lib))):
            if fn.endswith(".c"):
                name = fn[:-2]
                rel = "Tests/Probes/%s/%s" % (lib, name)
                host = os.path.join(build, "target", "samples", "Samples", rel)
                if os.path.exists(host):
                    out.append({"name": rel, "src": "tests/probes/%s" % lib, "host": host, "probe": True,
                                "expected": os.path.join(pdir, lib, name + ".ref.out")})
    return out


def _hash(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()[:16]


def _flat(name):
    return name.replace("/", "_")


NORMALISE = [
    (re.compile(r"0x[0-9a-fA-F]{6,8}\b"), "0xADDR"),
    (re.compile(r"\$[0-9a-fA-F]{6,8}\b"), "$ADDR"),
    (re.compile(r"\r"), ""),
]


def normalise(text, probe=False):
    """Probes print values, never pointers: only line endings are normalised."""
    for rx, rep in NORMALISE[2:] if probe else NORMALISE:
        text = rx.sub(rep, text)
    return text.rstrip() + "\n" if text.strip() else ""


# -- reference --------------------------------------------------------------------

def ref_cache_path(p, fp):
    return os.path.join(CACHE, "%s-%s-%s-v2.json" % (_flat(p["name"]), _hash(p["host"]), fp))


def run_ref_chunk(chunk, timeout_ms, profile="aga"):
    """Run a list of programs on one reference instance -> {name: result}."""
    results = {}
    todo = list(chunk)
    while todo:
        inst, lock = take_instance()
        exch = os.path.join(REFSYS, "inst", str(inst), "exchange")
        agent = None
        try:
            subprocess.run([REFCTL, "stop", "--id", str(inst)], stdout=subprocess.DEVNULL)
            shutil.rmtree(exch, ignore_errors=True)
            os.makedirs(os.path.join(exch, "out"))
            # the whole Tests tree at SYS:Tests, as on lxa (tests load siblings)
            host = todo[0]["host"]
            tests_dir = host[:host.rindex("/Tests/")] + "/Tests"
            r = subprocess.run([REFCTL, "boot", "--id", str(inst), "--profile", profile,
                                "--overlay", "%s=Tests" % tests_dir], capture_output=True, text=True)
            if r.returncode:
                raise AgentError("boot failed: %s" % r.stdout[-300:])
            agent = Agent(inst)
            agent.cmd("PING")
            agent.cmd("SETCLOCK %d" % EPOCH)
            while todo:
                p = todo[0]
                prog = "SYS:" + p["name"]
                outf = "out/%s.txt" % _flat(p["name"])
                res = {"name": p["name"]}
                t0 = time.time()
                try:
                    try:
                        agent.cmd("RUN >%s %s" % (outf, prog))
                    except AgentError as e1:     # serial noise right after a reboot
                        if "unknown command" not in str(e1):
                            raise
                        agent.cmd("RUN >%s %s" % (outf, prog))
                    lines = agent.cmd("WAIT_EXIT %d" % timeout_ms, timeout=timeout_ms / 1000 + 30)
                    res["rc"] = int([ln for ln in lines if ln.startswith("RC ")][0].split()[1])
                    res["status"] = "exit"
                except AgentError as e:
                    if "held" in str(e):
                        res["status"] = "crash"
                        res["error"] = "task held (Software Failure)"
                    elif "timeout" in str(e) and "WAIT_EXIT" in str(e):
                        res["status"] = "hang"
                        try:
                            agent.cmd("PING", timeout=5)
                        except AgentError:
                            res["status"] = "crash"
                            res["error"] = "system hang (agent unresponsive)"
                        if res["status"] == "hang":
                            try:
                                agent.cmd("QUIT 2000", timeout=30)
                            except AgentError:
                                pass
                    else:
                        res["status"] = "crash"
                        res["error"] = str(e)
                res["seconds"] = round(time.time() - t0, 2)
                op = os.path.join(exch, outf)
                res["stdout"] = open(op, encoding="latin-1").read() if os.path.exists(op) else ""
                results[p["name"]] = res
                todo.pop(0)
                if res["status"] != "exit":
                    break          # reboot: the machine state is no longer clean
        except (AgentError, OSError) as e:
            if todo:
                p = todo.pop(0)
                results[p["name"]] = {"name": p["name"], "status": "crash", "error": str(e), "stdout": ""}
        finally:
            if agent:
                try:
                    agent.close()
                except OSError:
                    pass
            subprocess.run([REFCTL, "stop", "--id", str(inst)], stdout=subprocess.DEVNULL)
            lock.close()
    return results


def run_ref(progs, jobs, timeout_ms, use_cache=True, chunk=12):
    subprocess.run([os.path.join(ROOT, "tools", "refsys", "build_refsys.sh"), "--profile", "aga"],
                   check=True, stdout=subprocess.DEVNULL)
    fp = refsys_fingerprint("aga")
    os.makedirs(CACHE, exist_ok=True)
    out, todo = {}, []
    for p in progs:
        cp = ref_cache_path(p, fp)
        if use_cache and os.path.exists(cp):
            with open(cp) as f:
                out[p["name"]] = json.load(f)
        else:
            todo.append(p)
    chunks = [todo[i:i + chunk] for i in range(0, len(todo), chunk)]
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        for res in ex.map(lambda c: run_ref_chunk(c, timeout_ms), chunks):
            for name, r in res.items():
                out[name] = r
                p = next(q for q in progs if q["name"] == name)
                if r["status"] in ("exit", "hang") or r.get("error", "").startswith(("task held", "system hang")):
                    with open(ref_cache_path(p, fp), "w") as f:
                        json.dump(r, f)
    return out


# -- lxa --------------------------------------------------------------------------

def run_lxa_one(name, build, timeout_ms):
    from rdd.pylxa import Lxa
    lxa = Lxa(build=build)
    try:
        lxa.run("SYS:" + name, "")
        # like the reference agent's WAIT_EXIT: the launched program has
        # returned, even if tasks it started (an app window...) still run
        ok = lxa.wait_program_exit(timeout_ms)
        res = {"name": name, "status": "exit" if ok else "hang",
               "rc": lxa.exit_code() if ok else None, "stdout": lxa.output()}
    finally:
        lxa.close()
    return res


def run_lxa(progs, build, jobs, timeout_ms):
    env = dict(os.environ, PYTHONPATH=os.path.join(ROOT, "tools"))

    def one(p):
        fd_, tmp = tempfile.mkstemp(prefix="suite-ref-", suffix=".json")
        os.close(fd_)
        try:
            r = subprocess.run([sys.executable, "-m", "rdd.suite_ref", "--lxa-one", p["name"], "--build", build,
                                "--timeout", str(timeout_ms), "--result", tmp], capture_output=True,
                               encoding="latin-1", env=env, cwd=ROOT, timeout=timeout_ms / 1000 * 10 + 60)
        except subprocess.TimeoutExpired:
            os.unlink(tmp)
            return {"name": p["name"], "status": "hang", "stdout": "", "error": "wall-clock timeout"}
        try:
            with open(tmp) as f:
                return json.load(f)
        except (OSError, ValueError):
            return {"name": p["name"], "status": "crash", "stdout": "", "error": r.stderr[-500:]}
        finally:
            if os.path.exists(tmp):
                os.unlink(tmp)

    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        return {r["name"]: r for r in ex.map(one, progs)}


# -- classification ---------------------------------------------------------------

def load_config():
    """tests/ref_suite.yaml: {program: {lxa_only|interactive: reason, phase: N}}"""
    if not os.path.exists(CONFIG):
        return {}
    import yaml
    with open(CONFIG) as f:
        return yaml.safe_load(f) or {}


def lint_config(cfg, names, roadmap=os.path.join(ROOT, "roadmap.md")):
    with open(roadmap) as f:
        phases = set(re.findall(r"^### Phase (\d+)", f.read(), re.M))
    probs = []
    for name, e in cfg.items():
        if name not in names:
            probs.append("%s: not a test program" % name)
        kinds = [k for k in ("lxa_only", "interactive") if e.get(k)]
        if len(kinds) != 1:
            probs.append("%s: needs exactly one of lxa_only / interactive (with a reason)" % name)
        if e.get("interactive") and str(e.get("phase")) not in phases:
            probs.append("%s: interactive entry needs a scheduled owning phase" % name)
    return probs


def classify(ref, lxa, cfg_entry=None):
    if cfg_entry and cfg_entry.get("interactive"):
        return "interactive"
    if cfg_entry and cfg_entry.get("lxa_only"):
        return "lxa-only" if lxa["status"] == "exit" and not lxa.get("rc") else "lxa-fail"
    if ref["status"] == "crash":
        return "ref-crash"
    if ref["status"] == "hang":
        return "ref-hang"
    if ref.get("rc"):
        return "ref-fail"
    if lxa["status"] != "exit" or lxa.get("rc"):
        return "lxa-fail"
    probe = ref.get("name", "").startswith("Tests/Probes/")
    return "pass" if normalise(ref["stdout"], probe) == normalise(lxa["stdout"], probe) else "output"


def expected_path(p):
    return p["expected"]


def lxa_check(a):
    """Every program with an expected.ref.out must exit 0 on lxa with the
    same normalised stdout (Phase 220)."""
    progs = [p for p in programs(a.build) if os.path.exists(expected_path(p))]
    if a.filter:
        progs = [p for p in progs if re.search(a.filter, p["name"])]
    if a.shard:
        i, n = (int(v) for v in a.shard.split("/"))
        progs = progs[i::n]
    res = run_lxa(progs, a.build, a.jobs, a.timeout)
    bad = 0
    for p in progs:
        x = res[p["name"]]
        with open(expected_path(p), encoding="latin-1") as f:
            want = normalise(f.read(), p.get("probe"))
        got = normalise(x["stdout"], p.get("probe"))
        if x["status"] != "exit" or x.get("rc") or got != want:
            bad += 1
            print("FAIL %s: status=%s rc=%s" % (p["name"], x["status"], x.get("rc")))
            import difflib
            for ln in list(difflib.unified_diff(want.split("\n"), got.split("\n"), "expected.ref.out",
                                                "lxa", lineterm="", n=1))[:30]:
                print("    " + ln)
    print("%d programs checked against expected.ref.out, %d failed" % (len(progs), bad))
    return 1 if bad else 0


def main(argv=None):
    ap = argparse.ArgumentParser(prog="rdd suite-ref")
    ap.add_argument("--filter")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "rdd", "suite"))
    ap.add_argument("--build", default=os.path.join(ROOT, "build"))
    ap.add_argument("-j", "--jobs", type=int, default=8)
    ap.add_argument("--timeout", type=int, default=20000)
    ap.add_argument("--no-cache", action="store_true")
    ap.add_argument("--capture", action="store_true")
    ap.add_argument("--capture-ref", action="store_true",
                    help="probes: write <name>.ref.out from the reference whenever it exits 0 "
                         "(lxa must then be fixed to match)")
    ap.add_argument("--lxa-one")
    ap.add_argument("--result", help="--lxa-one: write the result JSON here")
    ap.add_argument("--lint", action="store_true", help="check tests/ref_suite.yaml")
    ap.add_argument("--ref-only", action="store_true", help="skip the lxa runs")
    ap.add_argument("--lxa-check", action="store_true",
                    help="CTest mode: lxa output must equal every expected.ref.out (no reference needed)")
    ap.add_argument("--shard", help="i/n: only every n-th program, starting at i (with --lxa-check)")
    a = ap.parse_args(argv)
    a.build = os.path.abspath(a.build)
    if a.lint:
        progs = programs(a.build)
        probs = lint_config(load_config(), {p["name"] for p in progs})
        for pr in probs:
            print(pr)
        return 1 if probs else 0
    if a.lxa_check:
        return lxa_check(a)
    if a.lxa_one:
        res = run_lxa_one(a.lxa_one, a.build, a.timeout)
        if a.result:                 # not stdout: the program writes there too
            with open(a.result, "w") as f:
                json.dump(res, f)
        else:
            sys.stdout.write(json.dumps(res, ensure_ascii=True) + "\n")
        return 0
    progs = programs(a.build)
    if a.filter:
        progs = [p for p in progs if re.search(a.filter, p["name"])]
    os.makedirs(a.out, exist_ok=True)
    t0 = time.time()
    ref = run_ref(progs, a.jobs, a.timeout, not a.no_cache)
    lxa = ({p["name"]: {"name": p["name"], "status": "skipped", "stdout": ""} for p in progs}
           if a.ref_only else run_lxa(progs, a.build, a.jobs, a.timeout))
    cfg = load_config()
    rows = []
    for p in progs:
        r, x = ref[p["name"]], lxa[p["name"]]
        cls = classify(r, x, cfg.get(p["name"]))
        row = {"name": p["name"], "class": cls, "ref_status": r["status"], "ref_rc": r.get("rc"),
               "lxa_status": x["status"], "lxa_rc": x.get("rc"), "ref_error": r.get("error"),
               "ref_last": (r.get("stdout") or "").strip().split("\n")[-1][:120]}
        exp = expected_path(p)
        if os.path.exists(exp):
            with open(exp, encoding="latin-1") as f:
                row["expected_matches_lxa"] = normalise(f.read(), p.get("probe")) == normalise(x["stdout"], p.get("probe"))
        rows.append(row)
        with open(os.path.join(a.out, _flat(p["name"]) + ".json"), "w") as f:
            json.dump({"ref": r, "lxa": x, "class": cls}, f, indent=1)
        capture = (a.capture and cls == "pass") or \
            (a.capture_ref and p.get("probe") and r["status"] == "exit" and not r.get("rc"))
        if capture and r["stdout"].strip():
            with open(exp, "w", encoding="latin-1") as f:
                f.write(normalise(r["stdout"], p.get("probe")))
    with open(os.path.join(a.out, "summary.json"), "w") as f:
        json.dump(rows, f, indent=1)
    counts = {}
    for row in rows:
        counts[row["class"]] = counts.get(row["class"], 0) + 1
    for row in rows:
        if row["class"] not in ("pass", "interactive", "lxa-only"):
            print("%-10s %-40s ref=%s/%s lxa=%s/%s  %s" % (row["class"], row["name"], row["ref_status"],
                                                          row["ref_rc"], row["lxa_status"], row["lxa_rc"],
                                                          row["ref_error"] or row["ref_last"]))
    print("%d programs in %.0f s: %s" % (len(rows), time.time() - t0,
                                         ", ".join("%s %d" % kv for kv in sorted(counts.items()))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
