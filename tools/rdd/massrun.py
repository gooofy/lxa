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
import tempfile
import time

from rdd.scenario import ROOT

FISH = os.environ.get("LXA_FISH_DIR", os.path.expanduser("~/media/sys/amiga/pd/unpacked/fish"))
OUT = os.path.join(ROOT, "build", "rdd", "massrun")
HUNK_HEADER = b"\x00\x00\x03\xf3"
MAX_SIZE = 2 * 1024 * 1024
# the reference agent's WAIT_EXIT command arrives a serial round trip after
# RUN returned: the program has a little more than --seconds there
LXA_MARGIN_FRAMES = int(os.environ.get("LXA_MASSRUN_MARGIN", "10"))


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

def run_lxa_one(rel, root, frames, build=None, drive="FISH"):
    from rdd.pylxa import Lxa
    # the reference starts programs with RUN >file (stdin NIL:)
    os.environ["LXA_STDIO_FILE"] = "1"
    # ... from LXAREF:p/<id>/ with nothing assigned to the program's own
    # S/Libs/C directories (Phase 237)
    os.environ["LXA_PROGDIR_ASSIGNS"] = "0"
    # ... and with LoadWB's backdrop window on the Workbench screen (Fish
    # ISAM opens its window through GetScreenData()->FirstWindow, Phase 238)
    os.environ.setdefault("LXA_WB_WINDOW", "1")
    lxa = Lxa(build=build)
    res = {"backend": "lxa"}
    try:
        # the reference has the Workbench 3.1 fonts (Fish Evo needs ruby.font)
        wb31_fonts = os.path.join(os.environ.get("LXA_REFSYS_DIR", os.path.expanduser("~/.cache/lxa/refsys")),
                                  "SYS-aga", "Fonts")
        if os.path.isdir(wb31_fonts):
            lxa.assign("FONTS", wb31_fonts)
        if drive == "FISH":
            lxa.assign(drive, root)
        else:
            # a volume, as the reference's exchange drive: its root has no
            # parent (Fish Mkid builds its current directory's path)
            lxa.drive(drive, root)
        lxa.run(drive + ":" + rel.replace(os.sep, "/"), "")
        # the reference's run time starts when RUN returns, i.e. after
        # LoadSeg(); on lxa booting and loading take emulated time too, so
        # the budget starts once the program is loaded (Phase 237b: Fish
        # DumpWB sleeps Delay(300), exactly the run time)
        boot = 0
        while not lxa.program_loaded() and lxa.running() and boot < 500:
            lxa.frames(1)
            boot += 1
        res["load_frames"] = boot
        step = 10
        seen_windows = 0
        for _ in range(0, frames + LXA_MARGIN_FRAMES, step):
            lxa.frames(step)
            seen_windows = max(seen_windows, lxa.lib.lxa_get_window_count())
            if lxa.program_exited():
                break
        # as the reference's WAIT_EXIT: the program itself has returned
        # (tasks it started may live on)
        res["running"] = not lxa.program_exited()
        res["rc"] = None if res["running"] else lxa.exit_code()
        res["windows"] = lxa.lib.lxa_get_window_count()
        # windows the program had open at some point of its run (Phase 237d)
        res["seen_windows"] = max(seen_windows, res["windows"])
        res["exceptions"] = lxa.exceptions()
        if not res["running"] and res["exceptions"]:
            # the program returned: exceptions are other tasks' (a shell it
            # started, a detached part); the reference's WAIT_EXIT reports
            # the exit code then too (Phase 237b: Fish NewAUSH)
            res["other_exceptions"] = res["exceptions"]
            res["exceptions"] = []
        res["unimplemented"] = lxa.unimplemented()
        res["stdout"] = lxa.output()[:4000]
    except Exception as e:  # noqa: BLE001 - a load failure is a result too
        res["error"] = str(e)
    finally:
        lxa.close()
    return res


REF_BATCH = 25      # programs per reference boot (run_ref's chunk)


def run_lxa(progs, out, root, jobs, frames, build):
    """Run the programs on lxa in the environment the reference gives them
    (Phase 237d): the reference boots a fresh SYS: for every batch of
    REF_BATCH programs (in crawl order) and runs them one after the other,
    each from its own copy of its directory - so what an installer puts into
    LIBS:, FONTS: or ENV:, or a daemon it leaves, is there for the programs
    after it in the batch, and only for them.  lxa does the same: one fresh
    copy of its system directory per batch, the batch's programs in order,
    each from a fresh copy of its directory.  A program asked for alone is
    run with its whole batch."""
    rdir = os.path.join(out, "lxa")
    os.makedirs(rdir, exist_ok=True)
    allp = load_programs(out)
    index = {q["id"]: i for i, q in enumerate(allp)}
    wanted = sorted({index[q["id"]] // REF_BATCH for q in progs if q["id"] in index})
    batches = [allp[k * REF_BATCH:(k + 1) * REF_BATCH] for k in wanted]
    systemplate = os.path.join(ROOT, "share", "lxa", "System")
    scratch = os.path.join(out, "scratch")
    shutil.rmtree(scratch, ignore_errors=True)
    os.makedirs(scratch)
    total = sum(len(bt) for bt in batches)

    def one(p, sysdir, work):
        dst = os.path.join(rdir, p["id"] + ".json")
        tmp = dst + ".tmp"
        progroot = os.path.join(work, "root-" + p["id"])
        res = None
        # under the name the reference gives it: LXAREF:p/<id>/<name>
        # (Fish Mkid fid/lid copy their own name into a small buffer)
        prel = "p/%s/%s" % (p["id"], os.path.basename(p["rel"]))
        try:
            reldir = os.path.dirname(p["rel"])
            shutil.copytree(os.path.join(root, reldir), os.path.join(progroot, "p", p["id"]),
                            symlinks=True, ignore=shutil.ignore_patterns("*.uaem"))
        except OSError as e:
            res = {"backend": "lxa", "error": "harness: cannot copy the directory: %s" % e,
                   "harness": "copy failed"}
        if res is None:
            env = dict(os.environ, PYTHONPATH=os.path.join(ROOT, "tools"), LXA_SYSTEM_DIR=sysdir)
            cmd = [sys.executable, "-m", "rdd.massrun", "lxa-one", prel, "--root", progroot, "--drive", "LXAREF",
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
        shutil.rmtree(progroot, ignore_errors=True)
        with open(dst, "w") as f:
            json.dump(res, f)

    def batch(bt):
        work = tempfile.mkdtemp(prefix="batch-", dir=scratch)
        sysdir = os.path.join(work, "system")
        shutil.copytree(systemplate, sysdir)
        # LIBS: as on the reference's fresh SYS:: none of the third-party
        # libraries lxa's system directory carries for the app corpus (arp,
        # req, reqtools, powerpacker ...) - an installer in the batch adds
        # them for the programs after it, as there
        ref_libs = os.path.join(os.environ.get("LXA_REFSYS_DIR", os.path.expanduser("~/.cache/lxa/refsys")),
                                "SYS-aga", "Libs")
        if os.path.isdir(ref_libs):
            for dirpath, _dirs, files in os.walk(os.path.join(sysdir, "Libs")):
                rel = os.path.relpath(dirpath, os.path.join(sysdir, "Libs"))
                for fn in files:
                    if not os.path.exists(os.path.join(ref_libs, rel, fn)):
                        os.remove(os.path.join(dirpath, fn))
        for p in bt:
            one(p, sysdir, work)
        if not os.environ.get("LXA_MASSRUN_KEEP"):     # for a look at what the batch left
            shutil.rmtree(work, ignore_errors=True)
        return len(bt)

    t0 = time.time()
    n = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        for k in ex.map(batch, batches):
            n += k
            print("lxa: %d/%d (%.0f s)" % (n, total, time.time() - t0), flush=True)


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
                    prog.encode("latin-1")
                except UnicodeEncodeError:   # not a name AmigaOS can be given
                    with open(os.path.join(rdir, p["id"] + ".json"), "w") as f:
                        json.dump({"backend": "ref", "harness": "harness: name not Latin-1"}, f)
                    todo.pop(0)
                    continue
                try:
                    try:
                        agent.cmd('RUN >out/%s.txt "%s"' % (p["id"], prog))
                    except AgentError as e:
                        if "unknown command" not in str(e):
                            raise
                        agent.cmd('RUN >out/%s.txt "%s"' % (p["id"], prog))
                    lines = agent.cmd("WAIT_EXIT %d" % (seconds * 1000), timeout=seconds + 30)
                    rcs = [ln for ln in lines if ln.startswith("RC ")]
                    if not rcs:     # the agent's replies got out of step
                        raise AgentError("harness: WAIT_EXIT without RC: %r" % lines[-3:])
                    res["rc"] = int(rcs[0].split()[1])
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
                    elif msg.startswith("harness:"):
                        # reboot and run the program again (once)
                        p["retries"] = p.get("retries", 0) + 1
                        if p["retries"] <= 1:
                            break
                        res["harness"] = msg
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
            # a boot or agent failure is not the program's: retry it on a
            # fresh machine, then record it as a harness failure
            if todo:
                p = todo[0]
                p["retries"] = p.get("retries", 0) + 1
                if p["retries"] > 2:
                    todo.pop(0)
                    with open(os.path.join(rdir, p["id"] + ".json"), "w") as f:
                        json.dump({"backend": "ref", "harness": "harness: %s" % e}, f)
        finally:
            if agent:
                try:
                    agent.close()
                except OSError:
                    pass
            subprocess.run([REFCTL, "stop", "--id", str(inst)], stdout=subprocess.DEVNULL)
            lock.close()


def run_ref(progs, out, root, jobs, seconds, whole_batches=False):
    """Batches of REF_BATCH programs in crawl order, one fresh boot each
    (run_lxa reproduces the same batches).  whole_batches: run every
    program of the batches the given programs belong to, again - so that
    their environment is the one of a full batch (Phase 237d)."""
    rdir = os.path.join(out, "ref")
    os.makedirs(rdir, exist_ok=True)
    allp = load_programs(out)
    index = {q["id"]: i for i, q in enumerate(allp)}
    wanted = sorted({index[q["id"]] // REF_BATCH for q in progs if q["id"] in index})
    chunks = [allp[k * REF_BATCH:(k + 1) * REF_BATCH] for k in wanted]
    if not whole_batches:
        chunks = [[p for p in c if not os.path.exists(os.path.join(rdir, p["id"] + ".json"))] for c in chunks]
        chunks = [c for c in chunks if c]
    t0 = time.time()
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        for k, _ in enumerate(ex.map(lambda c: run_ref_chunk(c, root, rdir, seconds), chunks)):
            if (k + 1) % 10 == 0:
                print("ref: %d/%d chunks (%.0f s)" % (k + 1, len(chunks), time.time() - t0), flush=True)


# -- report ------------------------------------------------------------------------

def classify(r):
    if r is None or r.get("harness"):
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


# Programs whose outcome depends on the bytes of the Kickstart ROM, of low
# memory or of the registers they are started with (they read or write
# through a NULL pointer, or are modules of another program): neither lxa
# nor the reference outcome says anything about compatibility.  Each entry
# was established by disassembling the program.
MEMORY_DEPENDENT = {
    "fish-0734/PowerVisor/s/pv/GetCode": "PowerVisor function module: uses the A2 PowerVisor "
                                         "calls it with (Phase 237b)",
    "fish-0734/PowerVisor/s/pv/GetQual": "PowerVisor function module: uses the A2 PowerVisor "
                                         "calls it with (Phase 237b)",
    "fish-0363/WO/wo1.0d": "reads IntuitionBase->ActiveScreen through a NULL IntuitionBase "
                           "(Kickstart code via the vector table, Phase 237)",
    "fish-0099/A-Render/programs/LATHE": "parses its stdin buffer (AllocMem(1032) without MEMF_CLEAR) "
                                         "although Read() returned 0 at end of file: the answers are "
                                         "whatever the memory held (Phase 237d)",
    "fish-0842/AntiCicloVir/AntiCicloVir": "copies its argument string up to the next line feed after "
                                           "the first character, past the end of an empty command line "
                                           "(\\n): the outcome depends on the caller's buffer - on the "
                                           "reference a line feed follows 8 bytes later in lxaprobe's "
                                           "memory, on lxa none (Phase 237d)",
    "fish-0038/Window/window": "calls through the longword at $100 (movea.l $100,a6; jsr -28(a6), a "
                               "Kickstart 1.x convention): the outcome depends on the exception vector "
                               "there and the Kickstart code before it (Phase 240)",
}


# Programs whose reference outcome depends on the reference machine itself
# rather than on lxa (Phase 237d; what earlier programs of a batch install or
# leave behind is reproduced by run_lxa's batches).  Each entry was checked.
ENVIRONMENT_DEPENDENT = {
    "fish-0224/MoreIsBetter/V": "runs SYS:Utilities/More, a Workbench 3.1 program lxa does not ship",
    "fish-0429/V/V": "runs SYS:Utilities/More, a Workbench 3.1 program lxa does not ship",
    "fish-0763/MidiSyn/MidiSyn": "the reference's serial port is held by lxaprobe (\"Serialport not available\")",
    "fish-0101/Midi/util/mm": "midi.library cannot route from MidiIn on the reference: lxaprobe holds the "
                              "serial port",
    # what an earlier program of the batch left in memory: lxa runs every
    # program in a fresh emulator (Phase 237d)
    "fish-0398/Formatter/Formatter": "needs arp.library V39+: on the reference DClock, run before it in the "
                                     "same batch, opened the arp.library of its own drawer, which stays in "
                                     "memory",
    "fish-0896/Riff/riff": "needs reqtools.library: on the reference DA2.5, run before it in the same batch, "
                           "opened the reqtools.library of its own drawer, which stays in memory",
    "fish-0638/ShellTimer/ShellTimerDaemon": "ShellTimer, run before it in the reference batch, had already "
                                             "started the daemon",
    "fish-0422/PopUpMenu/PopUpMenu.run": "PopUpMenu, run before it in the reference batch, left its \"Reply\" "
                                         "port: PopUpMenu.run sends it the quit message and exits; alone it "
                                         "waits on the reference too",
    "fish-0955/MuroloUtil/Kickmaker": "on the reference AllocMem(MEMF_CHIP) failed after the earlier "
                                      "programs of its batch (\"Can't alloc memory\", exit); alone it waits "
                                      "for RETURN there too, as on lxa",
    "fish-0431/A68Kex/say": "the reference's Workbench 3.1 has no translator.library (say shows its \"not "
                            "found\" requester), lxa has one; neither has narrator.device",
}


# Programs that do their whole job and return within the run time on lxa
# while the reference is still busy with the window open - lxa's emulated
# rendering is faster than the reference's.  Each entry was checked by its
# trace (Phase 237d).
FINISHED_SOONER = {
    "fish-0305/PrFont/prfont": "renders every font of FONTS: into its window, prints it and quits",
    "fish-0606/PrFont/PrFont": "renders every font of FONTS: into its window, prints it and quits",
    "fish-0263/Sentinel/HD_3_Megabyte_Version": "plays its film until it needs Sentinel.2:Flash1 (Fish disk "
                                                "264) and quits with \"Can't load\"; the reference makes the "
                                                "same calls but was still loading Hall.anim",
    "fish-0263/Sentinel/HD_3.5_Megabyte_Version": "plays its film until it needs Sentinel.2:Flash1 (Fish disk "
                                                  "264) and quits with \"Can't load\"; the reference makes the "
                                                  "same calls but was still loading Hall.anim",
}


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
        kind = ("memory-dependent" if p["rel"] in MEMORY_DEPENDENT else
                "environment-dependent" if p["rel"] in ENVIRONMENT_DEPENDENT else program_kind(p["rel"]))
        row = {"id": p["id"], "rel": p["rel"], "lxa": cl, "ref": cr, "kind": kind,
               "lxa_rc": (lx or {}).get("rc"), "ref_rc": (rf or {}).get("rc"),
               "lxa_seen_windows": (lx or {}).get("seen_windows", 0)}
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
        if r["kind"] == "memory-dependent":
            return 7 if r["lxa"] != r["ref"] else 9
        if r["kind"] == "environment-dependent":
            return 8 if r["lxa"] != r["ref"] else 9
        if r["lxa"] == "crash" and r["ref"] not in ("crash", "noload", "missing"):
            return 1 if r["kind"] == "program" else 6
        if r["lxa"] == "hang" and r["ref"] in ("exit", "window"):
            return 2
        if r["rel"] in FINISHED_SOONER and r["ref"] == "window" and r["lxa"] == "exit" \
                and r.get("lxa_seen_windows"):
            return 8      # finished sooner: its window was there, see the list
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
        "lxa_only_hang_or_missing_window": sum(1 for r in rows if rank(r) in (2, 3)),
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
    L += ["", "## Memory-dependent programs", "",
          "Their outcome depends on the bytes of the Kickstart ROM or low memory (`MEMORY_DEPENDENT` in massrun.py).", ""]
    L += ["- %s (lxa %s, 3.1 %s): %s" % (r["rel"], r["lxa"], r["ref"], MEMORY_DEPENDENT[r["rel"]])
          for r in rows if rank(r) == 7]
    L += ["", "## Environment-dependent programs", "",
          "Their reference outcome depends on libraries other programs of the same reference batch "
          "installed (`ENVIRONMENT_DEPENDENT` in massrun.py).", ""]
    L += ["- %s (lxa %s, 3.1 %s): %s" % (r["rel"], r["lxa"], r["ref"], ENVIRONMENT_DEPENDENT[r["rel"]])
          for r in rows if rank(r) == 8 and r["rel"] in ENVIRONMENT_DEPENDENT]
    L += ["", "## Finished sooner", "",
          "Programs that opened their window on lxa and finished their work within the run time, while "
          "the reference was still at it (`FINISHED_SOONER` in massrun.py, each checked).", ""]
    L += ["- %s: %s" % (r["rel"], FINISHED_SOONER[r["rel"]]) for r in rows
          if rank(r) == 8 and r["rel"] in FINISHED_SOONER]
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
    # the reference's run time: lxaprobe's WAIT_EXIT 6000 polls 300 times
    # with Delay(1), and a Delay(1) takes two ticks on AmigaOS 3.1 (probe
    # dos/delayticks), so programs run 12 s there (measured: `C:Wait 12`
    # returns within WAIT_EXIT 6000, `Wait 13` does not; Phase 237d)
    ap.add_argument("--frames", type=int, default=600)
    ap.add_argument("--seconds", type=int, default=6)
    ap.add_argument("--filter")
    ap.add_argument("--limit", type=int)
    ap.add_argument("--ids-file", help="only the program ids listed (one per line)")
    ap.add_argument("--md")
    ap.add_argument("--result", help="lxa-one: write the result JSON here")
    ap.add_argument("--drive", default="FISH", help="lxa-one: the name --root is assigned to")
    a = ap.parse_args(argv)
    if a.build:
        a.build = os.path.abspath(a.build)
    # the lxa side runs its workers with cwd=ROOT
    a.out = os.path.abspath(a.out)
    if a.result:
        a.result = os.path.abspath(a.result)
    if a.cmd == "lxa-one":
        res = run_lxa_one(a.rel, a.root, a.frames, a.build, a.drive)
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
        run_ref(load_programs(a.out, a.filter, a.limit, a.ids_file), a.out, a.root, a.jobs, a.seconds,
                whole_batches=bool(a.ids_file))
    elif a.cmd == "report":
        s, _ = build_report(a.out, a.md and os.path.abspath(a.md))
        print(json.dumps({k: v for k, v in s.items() if k != "unimplemented_top"}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
