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
POOL = range(20, 40)
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
        r = subprocess.run([REFCTL, "boot", "--id", str(inst), "--profile", scn.profile],
                           capture_output=True, text=True)
        if r.returncode:
            raise AgentError("boot failed: %s%s" % (r.stdout, r.stderr))
        agent = Agent(inst)
        agent.cmd("SETCLOCK %d" % EPOCH)

        def tree():
            agent.cmd("DUMP_TREE rdd-tree.json")
            with open(os.path.join(exch, "rdd-tree.json")) as f:
                return json.load(f)

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
                    for name, rel, add in scn.assigns():
                        agent.cmd("ASSIGN %s APPS:%s/%s%s" % (name, scn.manifest["dir"], rel,
                                                             " ADD" if add else ""))
                    agent.cmd("TEXT_START")
                    agent.cmd(("RUN >rdd-stdout.txt %s %s" % (scn.ref_program(), scn.args)).rstrip())
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
                    agent.cmd("WAIT_IDLE 5000")
                elif kind == "menu":
                    agent.cmd("MENU %s" % args["path"])
                    agent.cmd("WAIT_IDLE 5000")
                elif kind == "type":
                    agent.cmd("TYPE %s" % args["text"])
                    agent.cmd("WAIT_IDLE 5000")
                elif kind == "key":
                    agent.cmd("KEY %x %x" % (args["rawkey"], args.get("qualifier", 0)))
                    agent.cmd("WAIT_IDLE 5000")
                elif kind == "snapshot":
                    snap_name = args["name"]
                    t = tree()
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
