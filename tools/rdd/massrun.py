"""Fred Fish / PD mass run (Phase 232) - crash hunting at scale.

    python3 -m rdd massrun crawl  [--root DIR] [--out DIR]
    python3 -m rdd massrun lxa    [--out DIR] [-j N] [--frames N] [--filter RX] [--limit N]
    python3 -m rdd massrun ref    [--out DIR] [-j N] [--seconds N] [--filter RX] [--limit N]
                                  [--ids-file F]   (also for lxa)
    python3 -m rdd massrun report [--out DIR] [--md FILE]

`crawl` finds every AmigaOS executable (hunk header 0x000003F3) under the
root (default: the unpacked Fish disks, LXA_FISH_DIR).  `lxa` runs each one
headless in its own process for a fixed amount of emulated time (no input,
stdin at EOF, --strict-unimplemented off) and records exit code, windows,
CPU exceptions, unimplemented-function hits and output; `ref` does the same
on the AmigaOS 3.1 reference (the program's directory is copied to LXAREF:).
`report` classifies every program on both sides:

    crash    CPU exception (lxa) / Software Failure or frozen machine (ref)
    hang     still running without a window when the time is up
    window   still running with a window (a GUI program waiting for input)
    unimpl   (lxa) called a function lxa marks as stub/partial
    exit     exited (rc recorded)

and ranks lxa-only crashes (P0), lxa-only hangs, missing windows, output
differences, and the most-hit unimplemented functions over the corpus.
Results go to <out> (default build/rdd/massrun); --md writes the report.
"""

import argparse
import collections
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
import time

from rdd.scenario import ROOT

FISH = os.environ.get("LXA_FISH_DIR", os.path.expanduser("~/media/sys/amiga/pd/unpacked/fish"))
OUT = os.path.join(ROOT, "build", "rdd", "massrun")
HUNK_HEADER = b"\x00\x00\x03\xf3"
MAX_SIZE = 2 * 1024 * 1024


def _key(rel):
    return re.sub(r"[^A-Za-z0-9._-]", "_", rel)


# -- crawl -------------------------------------------------------------------------

def crawl(root):
    progs = []
    for dirpath, dirs, files in os.walk(root):
        dirs.sort()
        for fn in sorted(files):
            if fn.endswith((".info", ".uaem")):
                continue
            p = os.path.join(dirpath, fn)
            try:
                if os.path.getsize(p) > MAX_SIZE or os.path.islink(p):
                    continue
                with open(p, "rb") as f:
                    if f.read(4) != HUNK_HEADER:
                        continue
            except OSError:
                continue
            rel = os.path.relpath(p, root)
            progs.append({"id": _key(rel), "rel": rel, "size": os.path.getsize(p),
                          "disk": rel.split(os.sep)[0]})
    return progs


def load_programs(out, filt=None, limit=None, ids_file=None):
    with open(os.path.join(out, "programs.json")) as f:
        progs = json.load(f)
    if ids_file:
        with open(ids_file) as f:
            ids = {ln.strip() for ln in f if ln.strip()}
        progs = [p for p in progs if p["id"] in ids]
    if filt:
        progs = [p for p in progs if re.search(filt, p["rel"])]
    return progs[:limit] if limit else progs


# -- lxa ---------------------------------------------------------------------------

def run_lxa_one(rel, root, frames, build=None):
    from rdd.pylxa import Lxa
    # the reference starts programs with RUN >file (stdin NIL:)
    os.environ["LXA_STDIO_FILE"] = "1"
    lxa = Lxa(build=build)
    res = {"backend": "lxa"}
    try:
        lxa.assign("FISH", root)
        lxa.run("FISH:" + rel.replace(os.sep, "/"), "")
        step = 10
        for _ in range(0, frames, step):
            lxa.frames(step)
            if not lxa.running():
                break
        res["running"] = bool(lxa.running())
        res["rc"] = None if res["running"] else lxa.exit_code()
        res["windows"] = lxa.lib.lxa_get_window_count()
        res["exceptions"] = lxa.exceptions()
        res["unimplemented"] = lxa.unimplemented()
        res["stdout"] = lxa.output()[:4000]
    except Exception as e:  # noqa: BLE001 - a load failure is a result too
        res["error"] = str(e)
    finally:
        lxa.close()
    return res


def run_lxa(progs, out, root, jobs, frames, build):
    rdir = os.path.join(out, "lxa")
    os.makedirs(rdir, exist_ok=True)
    # programs (installers!) write into LIBS:/SYS: - give them a private
    # copy of lxa's system directory, never the checked-in one
    sysdir = os.path.join(out, "system")
    shutil.rmtree(sysdir, ignore_errors=True)
    shutil.copytree(os.path.join(ROOT, "share", "lxa", "System"), sysdir)
    env = dict(os.environ, PYTHONPATH=os.path.join(ROOT, "tools"), LXA_SYSTEM_DIR=sysdir)

    def one(p):
        dst = os.path.join(rdir, p["id"] + ".json")
        tmp = dst + ".tmp"
        cmd = [sys.executable, "-m", "rdd.massrun", "lxa-one", p["rel"], "--root", root,
               "--frames", str(frames), "--result", tmp] + (["--build", build] if build else [])
        try:
            r = subprocess.run(cmd, capture_output=True, encoding="latin-1", env=env, cwd=ROOT,
                               timeout=120)
            with open(tmp) as f:
                res = json.load(f)
            os.unlink(tmp)
        except subprocess.TimeoutExpired:
            res = {"backend": "lxa", "error": "wall-clock timeout (emulator hang)", "host_hang": True}
        except (OSError, ValueError):
            res = {"backend": "lxa", "error": "emulator died (rc %s): %s" % (r.returncode, r.stderr[-300:]),
                   "host_crash": True}
        with open(dst, "w") as f:
            json.dump(res, f)
        return p["id"], res

    t0 = time.time()
    n = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        for _ in ex.map(one, progs):
            n += 1
            if n % 200 == 0:
                print("lxa: %d/%d (%.0f s)" % (n, len(progs), time.time() - t0), flush=True)


# -- reference ---------------------------------------------------------------------

def run_ref_chunk(chunk, root, rdir, seconds):
    from rdd.backend_ref import EPOCH, REFCTL, REFSYS, Agent, AgentError, take_instance
    todo = list(chunk)
    while todo:
        inst, lock = take_instance()
        exch = os.path.join(REFSYS, "inst", str(inst), "exchange")
        agent = None
        try:
            subprocess.run([REFCTL, "stop", "--id", str(inst)], stdout=subprocess.DEVNULL)
            shutil.rmtree(exch, ignore_errors=True)
            os.makedirs(os.path.join(exch, "out"))
            for p in todo:     # each program with its own directory (data files)
                src = os.path.dirname(os.path.join(root, p["rel"]))
                dst = os.path.join(exch, "p", p["id"])
                shutil.copytree(src, dst, symlinks=False, ignore=shutil.ignore_patterns("*.uaem"))
            r = subprocess.run([REFCTL, "boot", "--id", str(inst)], capture_output=True, text=True)
            if r.returncode:
                raise AgentError("boot failed")
            agent = Agent(inst)
            agent.cmd("PING")
            agent.cmd("SETCLOCK %d" % EPOCH)
            while todo:
                p = todo[0]
                res = {"backend": "ref"}
                prog = "LXAREF:p/%s/%s" % (p["id"], os.path.basename(p["rel"]))
                try:
                    try:
                        agent.cmd('RUN >out/%s.txt "%s"' % (p["id"], prog))
                    except AgentError as e:
                        if "unknown command" not in str(e):
                            raise
                        agent.cmd('RUN >out/%s.txt "%s"' % (p["id"], prog))
                    lines = agent.cmd("WAIT_EXIT %d" % (seconds * 1000), timeout=seconds + 30)
                    res["rc"] = int([ln for ln in lines if ln.startswith("RC ")][0].split()[1])
                    res["running"] = False
                except AgentError as e:
                    msg = str(e)
                    if "held" in msg:
                        res["crash"] = "task held (Software Failure)"
                    elif "WAIT_EXIT" in msg and "timeout" in msg:
                        try:
                            agent.cmd("PING", timeout=5)
                            agent.cmd("DUMP_TREE t.json")
                            with open(os.path.join(exch, "t.json")) as f:
                                tree = json.load(f)
                            wins = [w for s in tree.get("screens", []) for w in s.get("windows", [])
                                    if w.get("app", True)]
                            res["windows"] = len(wins)
                            res["window_titles"] = [w.get("title") or "" for w in wins]
                            # a crash on AmigaOS 3.1 shows the "Software Failure"
                            # requester (Suspend/Reboot): that is not a window of
                            # the program (Phase 237)
                            if any(t == "Software Failure" for t in res["window_titles"]):
                                res["crash"] = "Software Failure requester"
                            res["running"] = True
                            agent.cmd("QUIT 2000", timeout=30)
                        except AgentError:
                            res["crash"] = "system hang"
                    elif "cannot load" in msg:
                        res["load_error"] = msg.split("->")[-1].strip()
                    else:
                        res["crash"] = msg
                op = os.path.join(exch, "out", p["id"] + ".txt")
                res["stdout"] = open(op, encoding="latin-1").read()[:4000] if os.path.exists(op) else ""
                with open(os.path.join(rdir, p["id"] + ".json"), "w") as f:
                    json.dump(res, f)
                todo.pop(0)
                if res.get("crash") or res.get("running"):
                    break      # reboot for a clean machine
        except (AgentError, OSError) as e:
            if todo:
                p = todo.pop(0)
                with open(os.path.join(rdir, p["id"] + ".json"), "w") as f:
                    json.dump({"backend": "ref", "crash": "harness: %s" % e}, f)
        finally:
            if agent:
                try:
                    agent.close()
                except OSError:
                    pass
            subprocess.run([REFCTL, "stop", "--id", str(inst)], stdout=subprocess.DEVNULL)
            lock.close()


def run_ref(progs, out, root, jobs, seconds, chunk=25):
    rdir = os.path.join(out, "ref")
    os.makedirs(rdir, exist_ok=True)
    progs = [p for p in progs if not os.path.exists(os.path.join(rdir, p["id"] + ".json"))]
    chunks = [progs[i:i + chunk] for i in range(0, len(progs), chunk)]
    t0 = time.time()
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        for k, _ in enumerate(ex.map(lambda c: run_ref_chunk(c, root, rdir, seconds), chunks)):
            if (k + 1) % 10 == 0:
                print("ref: %d/%d chunks (%.0f s)" % (k + 1, len(chunks), time.time() - t0), flush=True)


# -- report ------------------------------------------------------------------------

def classify(r):
    if r is None:
        return "missing"
    if r.get("host_crash") or r.get("host_hang"):
        return "emulator-crash" if r.get("host_crash") else "emulator-hang"
    if r.get("load_error") or r.get("error", "").startswith("cannot load"):
        return "noload"
    if r.get("crash") or r.get("exceptions"):
        return "crash"
    if r.get("running"):
        return "window" if r.get("windows") else "hang"
    return "exit"


NONPROGRAM_RX = re.compile(r"(\.(library|device|resource|datatype|gadget|image|class|font|keymap|module|mod|pck|dark|ilbm)$)|"
                           r"(^|/)(libs?|devs?|keymaps|modules|fonts|l|classes|datatypes)/", re.I)


def program_kind(rel):
    """'non-program' for libraries, devices, keymaps, modules ... executed as
    commands: their outcome depends on memory contents, not on lxa's
    compatibility (Phase 237)."""
    return "non-program" if NONPROGRAM_RX.search(rel) else "program"


def crash_class(e):
    """exception class: vector, plus the LVO for calls through a NULL base"""
    pc = e.get("pc", 0)
    if e.get("vector") == 2 and pc >= 0xfffff000:
        return "%s through a NULL library base (LVO %d)" % (VECTORS[2], pc - 0x100000000)
    return VECTORS.get(e.get("vector"), "vector %s" % e.get("vector"))


def build_report(out, md=None):
    progs = load_programs(out)
    rows = []
    unimpl = collections.Counter()
    unimpl_progs = collections.defaultdict(set)
    crash_sig = collections.defaultdict(list)
    for p in progs:
        def load(side):
            fp = os.path.join(out, side, p["id"] + ".json")
            if not os.path.exists(fp):
                return None
            with open(fp) as f:
                return json.load(f)
        lx, rf = load("lxa"), load("ref")
        cl, cr = classify(lx), classify(rf)
        row = {"id": p["id"], "rel": p["rel"], "lxa": cl, "ref": cr, "kind": program_kind(p["rel"]),
               "lxa_rc": (lx or {}).get("rc"), "ref_rc": (rf or {}).get("rc")}
        for u in (lx or {}).get("unimplemented", []):
            k = "%s/%s" % (u["lib"], u["function"])
            unimpl[k] += 1
            unimpl_progs[k].add(p["rel"])
        if cl == "crash" and lx and lx.get("exceptions"):
            e = lx["exceptions"][0]
            sig = "vector %d in task %s" % (e["vector"], "program" if e["task"] else "?")
            crash_sig[e["vector"]].append(p["rel"])
            row["exception"] = e
        if cl == "exit" and cr == "exit":
            norm = lambda s: re.sub(r"0x[0-9a-fA-F]{6,8}", "ADDR", (s or "").replace("\r", "")).strip()
            row["output_equal"] = norm(lx.get("stdout")) == norm(rf.get("stdout"))
        rows.append(row)

    def rank(r):
        if r["lxa"] in ("emulator-crash", "emulator-hang"):
            return 0
        if r["lxa"] == "crash" and r["ref"] not in ("crash", "noload", "missing"):
            return 1 if r["kind"] == "program" else 6
        if r["lxa"] == "hang" and r["ref"] in ("exit", "window"):
            return 2
        if r["ref"] == "window" and r["lxa"] != "window":
            return 3
        if r["lxa"] == r["ref"] == "exit" and r["lxa_rc"] != r["ref_rc"]:
            return 4
        if r.get("output_equal") is False:
            return 5
        return 9

    rows.sort(key=lambda r: (rank(r), r["rel"]))
    summary = {
        "programs": len(rows),
        "lxa": dict(collections.Counter(r["lxa"] for r in rows)),
        "ref": dict(collections.Counter(r["ref"] for r in rows)),
        "pairs": dict(collections.Counter("%s/%s" % (r["lxa"], r["ref"]) for r in rows)),
        "lxa_only_crash": sum(1 for r in rows if rank(r) <= 1),
        "lxa_only_crash_classes": dict(collections.Counter(crash_class(r.get("exception") or {})
                                                           for r in rows if rank(r) == 1)),
        "unimplemented_top": [{"function": k, "programs": len(unimpl_progs[k]), "calls_records": v}
                              for k, v in sorted(unimpl.items(), key=lambda kv: -len(unimpl_progs[kv[0]]))[:40]],
        "crash_vectors": {str(k): len(v) for k, v in sorted(crash_sig.items())},
    }
    with open(os.path.join(out, "report.json"), "w") as f:
        json.dump({"summary": summary, "rows": rows}, f, indent=1)
    if md:
        _write_md(md, summary, rows, rank)
    return summary, rows


VECTORS = {2: "bus error", 3: "address error", 4: "illegal instruction", 5: "divide by zero",
           6: "CHK", 7: "TRAPV", 8: "privilege violation", 10: "line-A", 11: "line-F"}


def _write_md(path, summary, rows, rank):
    L = ["# Fred Fish mass run", "", "Generated by `python3 -m rdd massrun report` (Phase 232).", "",
         "%d executables from the Fred Fish disks, each run without input on lxa and on AmigaOS 3.1." %
         summary["programs"], "", "| outcome | lxa | AmigaOS 3.1 |", "|---|---|---|"]
    for k in ("exit", "window", "hang", "crash", "noload", "emulator-crash", "emulator-hang", "missing"):
        L.append("| %s | %d | %d |" % (k, summary["lxa"].get(k, 0), summary["ref"].get(k, 0)))
    L += ["", "## lxa-only crash classes", "",
          "Programs that crash on lxa where AmigaOS 3.1 exits, opens a window or keeps running "
          "(non-programs and programs without a reference result are listed separately).", "",
          "| class | programs |", "|---|---|"]
    L += ["| %s | %d |" % kv for kv in sorted(summary["lxa_only_crash_classes"].items(), key=lambda kv: -kv[1])]
    L += ["", "## lxa-only crashes (P0)", "", "| program | exception | reference |", "|---|---|---|"]
    for r in rows:
        if rank(r) <= 1:
            e = r.get("exception") or {}
            L.append("| %s | %s at 0x%08x | %s |" % (r["rel"], VECTORS.get(e.get("vector"), e.get("vector", r["lxa"])),
                                                    e.get("pc", 0), r["ref"]))
    L += ["", "## Non-programs that crash on lxa only", "",
          "Libraries, devices, keymaps and modules run as commands: the outcome depends on memory contents.", ""]
    L += ["- %s (%s)" % (r["rel"], crash_class(r.get("exception") or {})) for r in rows if rank(r) == 6]
    L += ["", "## lxa-only hangs and missing windows", "", "| program | lxa | AmigaOS 3.1 |", "|---|---|---|"]
    L += ["| %s | %s | %s |" % (r["rel"], r["lxa"], r["ref"]) for r in rows if rank(r) in (2, 3)]
    L += ["", "## Exit code and output differences", "",
          "%d programs exit with a different code, %d exit equally but print different output." % (
              sum(1 for r in rows if rank(r) == 4), sum(1 for r in rows if rank(r) == 5)), "",
          "## Most-hit unimplemented functions", "", "| function | programs |", "|---|---|"]
    L += ["| %s | %d |" % (u["function"], u["programs"]) for u in summary["unimplemented_top"]]
    with open(path, "w") as f:
        f.write("\n".join(L) + "\n")


def main(argv=None):
    ap = argparse.ArgumentParser(prog="rdd massrun")
    ap.add_argument("cmd", choices=["crawl", "lxa", "ref", "report", "lxa-one"])
    ap.add_argument("rel", nargs="?")
    ap.add_argument("--root", default=FISH)
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--build")
    ap.add_argument("-j", "--jobs", type=int, default=16)
    # 300 frames = 6 s, the reference's --seconds (equal run time on both)
    ap.add_argument("--frames", type=int, default=300)
    ap.add_argument("--seconds", type=int, default=6)
    ap.add_argument("--filter")
    ap.add_argument("--limit", type=int)
    ap.add_argument("--ids-file", help="only the program ids listed (one per line)")
    ap.add_argument("--md")
    ap.add_argument("--result", help="lxa-one: write the result JSON here")
    a = ap.parse_args(argv)
    if a.build:
        a.build = os.path.abspath(a.build)
    if a.cmd == "lxa-one":
        res = run_lxa_one(a.rel, a.root, a.frames, a.build)
        with open(a.result, "w") as f:      # not stdout: the program writes there too
            json.dump(res, f)
        return 0
    os.makedirs(a.out, exist_ok=True)
    if a.cmd == "crawl":
        progs = crawl(a.root)
        with open(os.path.join(a.out, "programs.json"), "w") as f:
            json.dump(progs, f, indent=0)
        print("%d executables on %d disks" % (len(progs), len({p["disk"] for p in progs})))
    elif a.cmd == "lxa":
        run_lxa(load_programs(a.out, a.filter, a.limit, a.ids_file), a.out, a.root, a.jobs, a.frames, a.build)
    elif a.cmd == "ref":
        run_ref(load_programs(a.out, a.filter, a.limit, a.ids_file), a.out, a.root, a.jobs, a.seconds)
    elif a.cmd == "report":
        s, _ = build_report(a.out, a.md and os.path.abspath(a.md))
        print(json.dumps({k: v for k, v in s.items() if k != "unimplemented_top"}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
