"""Run a scenario on lxa (Phase 213).

    python3 -m rdd.backend_lxa <scenario.yaml> <out_dir> [--build DIR]

Runs in its own process because liblxa is a per-process singleton; the twin
runner starts one process per scenario.  Writes <out_dir>/<snapshot>/
bundles and <out_dir>/result.json.
"""

import argparse
import json
import os
import shutil
import sys
import tempfile
import time

from rdd import bundle, fd, geometry
from rdd.pylxa import Lxa, MOUSE_LEFT, MOUSE_RIGHT
from rdd.scenario import Scenario

RAWKEY_MAP = {}


def run(scn, out_dir, build=None):
    os.makedirs(out_dir, exist_ok=True)
    result = {"backend": "lxa", "scenario": scn.name, "ok": True, "steps": [], "snapshots": []}
    t0 = time.time()
    tmp = tempfile.mkdtemp(prefix="rdd-lxa-")
    # the reference runner starts programs as `RUN >rdd-stdout.txt prog`:
    # stdout is a file and stdin NIL: there, so IsInteractive() of both
    # must be FALSE here
    os.environ["LXA_STDIO_FILE"] = "1"
    lxa = Lxa(build=build)
    try:
        fonts = scn.lxa_fonts_dir()
        if fonts:
            if not os.path.isdir(fonts):
                raise RuntimeError("fonts: %s missing (build the reference system)" % fonts)
            lxa.assign("FONTS", fonts)
        if scn.keymaps:
            if not os.path.isdir(scn.keymaps_dir()):
                raise RuntimeError("keymaps: %s missing (build the reference system)" % scn.keymaps_dir())
            lxa.assign("KEYMAPS", scn.keymaps_dir())
        for f in scn.prefs_files():
            # ENV:Sys/*.prefs - applied by C:IPrefs when the program boots
            lxa.install_prefs(os.path.join(scn.prefs_dir(), f))
        app_dir = scn.app_host_dir()
        if scn.writable:
            # a private copy the program may write into (RDDAPP:<dir>)
            work = os.path.join(tmp, "rddapp")
            shutil.copytree(app_dir, os.path.join(work, scn.manifest["dir"]), symlinks=True)
            app_dir = os.path.join(work, scn.manifest["dir"])
            lxa.assign("RDDAPP", work)
        for name, rel, add in scn.assigns():
            path = os.path.join(app_dir, rel)
            (lxa.assign_add if add else lxa.assign)(name, path)
        lxa.text_start()

        def tree():
            p = os.path.join(tmp, "tree.json")
            lxa.dump_tree(p)
            with open(p) as f:
                return json.load(f)

        def snapshot(snap_name, args, t=None):
            t = t or tree()
            window = None
            if "window" in args:
                found = geometry.find_window(t, args["window"])
                if not found:
                    raise RuntimeError("no window %r" % args["window"])
                window = found[1]
            tp = os.path.join(tmp, "tree-snap.json")
            with open(tp, "w") as f:
                json.dump(t, f)
            sp = os.path.join(tmp, "snap.bin")
            xp = os.path.join(tmp, "text.jsonl")
            snap = None
            if args.get("screen", True):
                lxa.snap(sp, window)
                with open(sp, "rb") as f:
                    snap = f.read()
            lxa.text_dump(xp)
            bundle.write_bundle(os.path.join(out_dir, snap_name), "lxa", scn.name, snap_name,
                                snap, tp, text_path=xp, stdout=lxa.output(),
                                unimplemented=lxa.unimplemented(), window=window,
                                extra_meta={"profile": scn.profile})
            result["snapshots"].append(snap_name)

        held = [0]   # mouse buttons held by press/release steps
        gesture_tree = [None]

        for kind, args in scn.steps:
            step = {"step": kind, "args": args, "ok": True}
            try:
                if kind == "trace":
                    if not lxa.trace_start(fd.lxa_spec(args["spec"]), os.path.join(out_dir, "trace.jsonl")):
                        raise RuntimeError("cannot start the relay trace")
                elif kind == "launch":
                    lxa.run(scn.lxa_program(), scn.args, stack=scn.stack)
                    lxa.frames(1)
                elif kind == "wait_window":
                    if lxa.wait_window_title(args.get("title", ""), args.get("timeout", 10000)) < 0:
                        raise RuntimeError("no window %r" % args.get("title"))
                elif kind == "wait_idle":
                    lxa.wait_idle()
                    lxa.frames(2)
                    lxa.wait_idle()
                elif kind == "frames":
                    lxa.frames(args["count"])
                elif kind == "click":
                    x, y = geometry.click_point(tree(), args)
                    lxa.click(x, y, MOUSE_LEFT)
                    lxa.wait_idle()
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
                    bit = MOUSE_RIGHT if args.get("button", "L") == "R" else MOUSE_LEFT
                    if kind == "press":
                        held[0] |= bit
                    elif kind == "release":
                        held[0] &= ~bit
                    lxa.lib.lxa_inject_mouse(x, y, held[0], 2 if kind == "move" else 1)
                    lxa.frames(2)
                elif kind == "wait_output":
                    deadline = args.get("timeout", 10000) // 20
                    while args["text"] not in lxa.output() and deadline > 0:
                        lxa.frames(1)
                        deadline -= 1
                    if args["text"] not in lxa.output():
                        raise RuntimeError("output never contained %r (output ends: %r)" % (args["text"], lxa.output()[-300:]))
                elif kind == "menu":
                    if not lxa.menu(args["path"]):
                        raise RuntimeError("menu %r not found" % args["path"])
                    lxa.wait_idle()
                elif kind == "type":
                    lxa.type(args["text"])
                    lxa.wait_idle()
                elif kind == "key":
                    lxa.key(args["rawkey"], args.get("qualifier", 0))
                    lxa.wait_idle()
                elif kind == "snapshot":
                    snapshot(args["name"], args)
                elif kind == "menus":
                    t = tree()
                    pts = geometry.menu_title_points(t, args.get("window"))
                    step["menus"] = [p[0] for p in pts]
                    for i, (_, x, y) in enumerate(pts):
                        lxa.lib.lxa_inject_drag_begin(x, y, MOUSE_RIGHT)
                        lxa.wait_idle()
                        snapshot("menu%d" % i, {"screen": True}, t)
                        lxa.lib.lxa_inject_drag_end(x, y)
                        lxa.wait_idle()
                elif kind == "quit":
                    t = tree()
                    found = geometry.find_window(t)
                    if found:
                        pt = geometry.close_gadget_point(found[2])
                        if pt:
                            lxa.click(pt[0], pt[1], MOUSE_LEFT)
                    lxa.wait_exit(args.get("timeout", 5000))
                    step["survivor"] = bool(lxa.running())
            except Exception as e:  # noqa: BLE001 - reported per step
                step["ok"] = False
                step["error"] = str(e)
                result["ok"] = False
            result["steps"].append(step)
            if not step["ok"]:
                break
        lxa.trace_stop()
        result["exit_code"] = lxa.exit_code() if not lxa.running() else None
        result["unimplemented"] = lxa.unimplemented()
    finally:
        lxa.close()
    result["seconds"] = round(time.time() - t0, 2)
    with open(os.path.join(out_dir, "result.json"), "w") as f:
        json.dump(result, f, indent=1)
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scenario")
    ap.add_argument("out")
    ap.add_argument("--build")
    a = ap.parse_args()
    r = run(Scenario(a.scenario), a.out, a.build)
    return 0 if r["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
