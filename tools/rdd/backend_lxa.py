"""Run a scenario on lxa (Phase 213).

    python3 -m rdd.backend_lxa <scenario.yaml> <out_dir> [--build DIR]

Runs in its own process because liblxa is a per-process singleton; the twin
runner starts one process per scenario.  Writes <out_dir>/<snapshot>/
bundles and <out_dir>/result.json.
"""

import argparse
import json
import os
import sys
import tempfile
import time

from rdd import bundle, geometry
from rdd.pylxa import Lxa, MOUSE_LEFT
from rdd.scenario import Scenario

RAWKEY_MAP = {}


def run(scn, out_dir, build=None):
    os.makedirs(out_dir, exist_ok=True)
    result = {"backend": "lxa", "scenario": scn.name, "ok": True, "steps": [], "snapshots": []}
    t0 = time.time()
    tmp = tempfile.mkdtemp(prefix="rdd-lxa-")
    lxa = Lxa(build=build)
    try:
        for name, rel, add in scn.assigns():
            path = os.path.join(scn.app_host_dir(), rel)
            (lxa.assign_add if add else lxa.assign)(name, path)
        lxa.text_start()

        def tree():
            p = os.path.join(tmp, "tree.json")
            lxa.dump_tree(p)
            with open(p) as f:
                return json.load(f)

        for kind, args in scn.steps:
            step = {"step": kind, "args": args, "ok": True}
            try:
                if kind == "launch":
                    lxa.run(scn.lxa_program(), scn.args)
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
                    snap_name = args["name"]
                    t = tree()
                    window = None
                    if "window" in args:
                        found = geometry.find_window(t, args["window"])
                        if not found:
                            raise RuntimeError("no window %r" % args["window"])
                        window = found[1]
                    tp = os.path.join(tmp, "tree.json")
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
