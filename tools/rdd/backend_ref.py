"""Run a scenario on the real AmigaOS 3.1 reference (Phase 213).

    python3 -m rdd.backend_ref <scenario.yaml> <out_dir> [--build DIR] [--no-cache]

Takes a free reference instance from the pool (ids 20-39, flock), boots it
(fresh system copy), drives the lxaprobe agent and turns the artefacts in
the instance's LXAREF: exchange directory into bundles.  Results are cached
by (scenario, app binaries, reference-system checksum, agent source) in
~/.cache/lxa/rdd/ref/<key>/ - the reference is deterministic enough for that
and booting it is the expensive part.
"""

import argparse
import fcntl
import hashlib
import json
import os
import shutil
import socket
import subprocess
import sys
import time

from rdd import bundle, fd, geometry
from rdd.scenario import Scenario

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REFSYS_TOOLS = os.path.join(ROOT, "tools", "refsys")
REFCTL = os.path.join(REFSYS_TOOLS, "refctl")
REFSYS = os.environ.get("LXA_REFSYS_DIR", os.path.expanduser("~/.cache/lxa/refsys"))
CACHE = os.environ.get("LXA_RDD_CACHE", os.path.expanduser("~/.cache/lxa/rdd/ref"))
def _pool():
    """Reference instances to use: the twin-runner pool 20-39, or
    LXA_REF_POOL="a-b" (e.g. while other agents hold the pool)."""
    spec = os.environ.get("LXA_REF_POOL")
    if spec:
        lo, _, hi = spec.partition("-")
        return range(int(lo), int(hi or lo) + 1)
    return range(20, 40)


POOL = _pool()
EPOCH = 1704067200   # lxa's deterministic boot time (2024-01-01 00:00)


class AgentError(Exception):
    pass


class Agent:
    def __init__(self, inst):
        self.inst = inst
        self.sock = socket.create_connection(("127.0.0.1", 47100 + inst), timeout=5)
        self.sock.settimeout(1.0)
        self.buf = b""

    def cmd(self, line, timeout=60):
        self.sock.sendall(line.encode("latin-1") + b"\n")
        lines = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            while b"\n" in self.buf:
                ln, self.buf = self.buf.split(b"\n", 1)
                text = ln.decode("latin-1").rstrip("\r")
                if text == "OK":
                    return lines
                if text.startswith("ERR"):
                    raise AgentError("%s -> %s" % (line, text))
                lines.append(text)
            try:
                chunk = self.sock.recv(65536)
                if not chunk:
                    raise AgentError("connection closed")
                self.buf += chunk
            except socket.timeout:
                continue
        raise AgentError("%s -> timeout" % line)

    def close(self):
        self.sock.close()


def take_instance():
    os.makedirs(os.path.join(REFSYS, "locks"), exist_ok=True)
    while True:
        for n in POOL:
            f = open(os.path.join(REFSYS, "locks", "%d.lock" % n), "w")
            try:
                fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
                return n, f
            except OSError:
                f.close()
        time.sleep(0.5)


def refsys_fingerprint(profile):
    parts = []
    try:
        with open(os.path.join(REFSYS, "SYS-%s" % profile, ".refsys-checksum")) as f:
            parts.append(f.read().strip())
    except OSError:
        parts.append("unbuilt")
    with open(os.path.join(REFSYS_TOOLS, "agent", "lxaprobe.c"), "rb") as f:
        parts.append(hashlib.sha256(f.read()).hexdigest()[:12])
    return "-".join(parts)


def run(scn, out_dir, build=None, use_cache=True):
    build = build or os.environ.get("LXA_BUILD", os.path.join(ROOT, "build"))
    subprocess.run([os.path.join(REFSYS_TOOLS, "build_refsys.sh"), "--profile", scn.profile],
                   check=True, stdout=subprocess.DEVNULL)
    key = scn.cache_key(build, refsys_fingerprint(scn.profile))
    cdir = os.path.join(CACHE, "%s-%s" % (scn.name, key))
    if use_cache and os.path.exists(os.path.join(cdir, "result.json")):
        with open(os.path.join(cdir, "result.json")) as f:
            cached = json.load(f)
        if cached.get("ok"):
            if os.path.abspath(cdir) != os.path.abspath(out_dir):
                shutil.rmtree(out_dir, ignore_errors=True)
                shutil.copytree(cdir, out_dir)
            cached["cached"] = True
            return cached

    os.makedirs(out_dir, exist_ok=True)
    result = {"backend": "ref", "scenario": scn.name, "ok": True, "steps": [], "snapshots": [],
              "cache_key": key}
    t0 = time.time()
    inst, lock = take_instance()
    exch = os.path.join(REFSYS, "inst", str(inst), "exchange")
    agent = None
    tracing = False
    try:
        subprocess.run([REFCTL, "stop", "--id", str(inst)], stdout=subprocess.DEVNULL)
        shutil.rmtree(exch, ignore_errors=True)
        os.makedirs(os.path.join(exch, "bin"), exist_ok=True)
        if scn.sample:
            shutil.copy2(scn.sample_host_path(build), os.path.join(exch, "bin"))
        if scn.writable:
            # APPS: is read-only: the program runs from a copy in LXAREF:
            shutil.copytree(scn.app_host_dir(), os.path.join(exch, "rddapp", scn.manifest["dir"]),
                            symlinks=True)
        boot = [REFCTL, "boot", "--id", str(inst), "--profile", scn.profile]
        if scn.prefs:
            # the reference's Startup-Sequence copies ENVARC: to ENV: and runs
            # its own C:IPrefs before LoadWB
            pdir = os.path.join(exch, "prefs-overlay")
            os.makedirs(pdir, exist_ok=True)
            for f in scn.prefs_files():
                shutil.copy2(os.path.join(scn.prefs_dir(), f), pdir)
            boot += ["--overlay", "%s=Prefs/Env-Archive/Sys" % pdir]
        if scn.keymaps:
            # KEYMAPS: is DEVS:Keymaps on the reference
            boot += ["--overlay", "%s=Devs/Keymaps" % scn.keymaps_dir()]
        r = subprocess.run(boot, capture_output=True, text=True)
        if r.returncode:
            raise AgentError("boot failed: %s%s" % (r.stdout, r.stderr))
        agent = Agent(inst)
        agent.cmd("SETCLOCK %d" % EPOCH)

        def tree():
            agent.cmd("DUMP_TREE rdd-tree.json")
            with open(os.path.join(exch, "rdd-tree.json")) as f:
                return json.load(f)

        gesture_tree = [None]

        def settle():
            """after input: wait until idle, but a program that polls never
            looks idle - then a short delay is enough"""
            try:
                agent.cmd("WAIT_IDLE 5000", timeout=40)
            except AgentError as e:
                if "not idle" not in str(e) and "timeout" not in str(e):
                    raise
                agent.cmd("DELAY 10")

        def snapshot(snap_name, args, t):
            window = None
            cmd = "SNAP rdd-snap.bin"
            if "window" in args:
                found = geometry.find_window(t, args["window"])
                if not found:
                    raise AgentError("no window %r" % args["window"])
                window = found[1]
                cmd += " WINDOW %d" % window
            snap = None
            if args.get("screen", True):
                agent.cmd(cmd)
                with open(os.path.join(exch, "rdd-snap.bin"), "rb") as f:
                    snap = f.read()
            agent.cmd("TEXT_DUMP rdd-text.jsonl")
            stdout_path = os.path.join(exch, "rdd-stdout.txt")
            stdout = open(stdout_path, encoding="latin-1").read() if os.path.exists(stdout_path) else ""
            bundle.write_bundle(os.path.join(out_dir, snap_name), "ref", scn.name, snap_name,
                                snap, os.path.join(exch, "rdd-tree.json"),
                                text_path=os.path.join(exch, "rdd-text.jsonl"),
                                stdout=stdout, window=window,
                                extra_meta={"profile": scn.profile,
                                            "refsys_checksum": refsys_fingerprint(scn.profile)})
            result["snapshots"].append(snap_name)

        for kind, args in scn.steps:
            step = {"step": kind, "args": args, "ok": True}
            try:
                if kind == "trace":
                    for lib, lvos in fd.spec_to_lvos(args["spec"]):
                        if lvos is None:
                            lvos = sorted(fd.functions(lib), reverse=True)[:64]
                        agent.cmd("TRACE %s %s" % (lib, ",".join(str(-v) for v in lvos)))
                    tracing = True
                elif kind == "launch":
                    if scn.writable:
                        agent.cmd("ASSIGN RDDAPP LXAREF:rddapp")
                    for name, rel, add in scn.assigns():
                        path = scn.app_amiga_dir() + ("/" + rel if rel not in ("", ".") else "")
                        agent.cmd("ASSIGN %s %s%s" % (name, path, " ADD" if add else ""))
                    agent.cmd("TEXT_START")
                    prog = scn.ref_program()
                    if " " in prog:
                        prog = '"%s"' % prog
                    stack = (" STACK %d" % scn.stack) if scn.stack else ""
                    agent.cmd(("RUN >rdd-stdout.txt%s %s %s" % (stack, prog, scn.args)).rstrip())
                    agent.cmd("DELAY 1")
                elif kind == "wait_window":
                    agent.cmd("WAIT_WINDOW %s %d" % (args.get("title", ""), args.get("timeout", 10000)),
                              timeout=args.get("timeout", 10000) / 1000 + 30)
                elif kind == "wait_idle":
                    agent.cmd("WAIT_IDLE %d" % args.get("timeout", 10000))
                elif kind == "frames":
                    agent.cmd("DELAY %d" % args["count"])
                elif kind == "click":
                    x, y = geometry.click_point(tree(), args)
                    agent.cmd("CLICK %d %d L" % (x, y))
                    settle()
                elif kind in ("move", "press", "release"):
                    # a gesture stays anchored where it started: while a
                    # verify/menu state is active the window may be missing
                    # from the tree, so fall back to the last tree that had it
                    try:
                        t = tree()
                        x, y = geometry.click_point(t, args)
                        gesture_tree[0] = t
                    except LookupError:
                        if gesture_tree[0] is None:
                            raise
                        x, y = geometry.click_point(gesture_tree[0], args)
                    b = "R" if args.get("button", "L") == "R" else "L"
                    agent.cmd({"move": "MOVE %d %d", "press": "PRESS %d %d " + b,
                               "release": "RELEASE %d %d " + b}[kind] % (x, y))
                    agent.cmd("DELAY 2")
                elif kind == "wait_output":
                    stdout_path = os.path.join(exch, "rdd-stdout.txt")
                    waited = 0
                    while waited < args.get("timeout", 10000):
                        if os.path.exists(stdout_path) and args["text"] in open(stdout_path, encoding="latin-1").read():
                            break
                        agent.cmd("DELAY 5")
                        waited += 100
                    else:
                        tail = open(stdout_path, encoding="latin-1").read()[-300:] if os.path.exists(stdout_path) else ""
                        raise AgentError("output never contained %r (output ends: %r)" % (args["text"], tail))
                elif kind == "menu":
                    agent.cmd("MENU %s" % args["path"])
                    settle()
                elif kind == "type":
                    # TYPE maps characters through the keymap; a newline is
                    # the Return key (rawkey 0x44), which has no character
                    parts = args["text"].split("\n")
                    for i, part in enumerate(parts):
                        if part:
                            agent.cmd("TYPE %s" % part)
                        if i < len(parts) - 1:
                            agent.cmd("KEY 44")
                    settle()
                elif kind == "key":
                    agent.cmd("KEY %x %x" % (args["rawkey"], args.get("qualifier", 0)))
                    settle()
                elif kind == "snapshot":
                    snapshot(args["name"], args, tree())
                elif kind == "menus":
                    t = tree()      # dumped before the menu opens (no LockIBase while it is held)
                    pts = geometry.menu_title_points(t, args.get("window"))
                    step["menus"] = [p[0] for p in pts]
                    for i, (_, x, y) in enumerate(pts):
                        agent.cmd("PRESS %d %d R" % (x, y))
                        agent.cmd("DELAY 10")
                        snapshot("menu%d" % i, {"screen": True}, t)
                        agent.cmd("RELEASE %d %d R" % (x, y))
                        agent.cmd("DELAY 10")     # MENUPICK(MENUNULL) handling; not WAIT_IDLE
                                                  # (apps polling input never look idle)
                elif kind == "quit":
                    lines = agent.cmd("QUIT %d" % args.get("timeout", 5000), timeout=60)
                    step["survivor"] = "SURVIVOR yes" in lines
                    for ln in lines:
                        if ln.startswith("MEMDELTA"):
                            step["memdelta"] = int(ln.split()[1])
            except (AgentError, LookupError, OSError, ValueError) as e:
                step["ok"] = False
                step["error"] = str(e)
                result["ok"] = False
            result["steps"].append(step)
            if not step["ok"]:
                break
        if tracing:
            agent.cmd("TRACE_DUMP rdd-trace.jsonl")
            agent.cmd("TRACE_STOP")
            shutil.copyfile(os.path.join(exch, "rdd-trace.jsonl"), os.path.join(out_dir, "trace.jsonl"))
    except AgentError as e:
        result["ok"] = False
        result["error"] = str(e)
    finally:
        if agent:
            agent.close()
        subprocess.run([REFCTL, "stop", "--id", str(inst)], stdout=subprocess.DEVNULL)
        lock.close()
    result["seconds"] = round(time.time() - t0, 2)
    with open(os.path.join(out_dir, "result.json"), "w") as f:
        json.dump(result, f, indent=1)
    if result["ok"] and use_cache:
        shutil.rmtree(cdir, ignore_errors=True)
        shutil.copytree(out_dir, cdir)
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scenario")
    ap.add_argument("out")
    ap.add_argument("--build")
    ap.add_argument("--no-cache", action="store_true")
    a = ap.parse_args()
    r = run(Scenario(a.scenario), a.out, a.build, use_cache=not a.no_cache)
    return 0 if r["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
