#!/usr/bin/env python3
"""Reference-system self test (Phase 211 gate).

Boots a reference instance, runs SYS:Utilities/Clock through the lxaprobe
agent, dumps the tree, takes a pen-index snapshot, quits, and validates the
results.  Exit code 77 (ctest SKIP) when the user's AmigaOS media or
FS-UAE are not available.

  tools/refsys/selftest.py [--id N] [--profile aga|rtg]
"""

import argparse
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REFCTL = os.path.join(HERE, "refctl")
REFSYS = os.environ.get("LXA_REFSYS_DIR", os.path.expanduser("~/.cache/lxa/refsys"))
OS_DIR = os.environ.get("LXA_REF_OS_DIR", os.path.expanduser("~/media/sys/amiga/os/3.1"))


def media_available():
    if not shutil.which("Xvfb") or not (shutil.which("fs-uae") or os.path.exists("/opt/FS-UAE/usr/bin/fs-uae")):
        return False
    if not os.path.isdir(OS_DIR):
        return False
    names = os.listdir(OS_DIR)
    return any("Kickstart v3.1 r40.70" in n and "(A4000).rom" in n for n in names) and \
        sum(1 for n in names if n.startswith("Workbench v3.1") and n.endswith(".adf")) >= 6


def refctl(*args, check=True):
    r = subprocess.run([REFCTL] + list(args), capture_output=True, text=True)
    if check and r.returncode != 0:
        raise RuntimeError("refctl %s failed: %s%s" % (" ".join(args), r.stdout, r.stderr))
    return r.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--id", default="9")
    ap.add_argument("--profile", default="aga")
    a = ap.parse_args()
    if not media_available():
        print("selftest: AmigaOS 3.1 media / FS-UAE not available - skipped")
        return 77

    subprocess.run([os.path.join(HERE, "build_refsys.sh"), "--profile", a.profile], check=True,
                   stdout=subprocess.DEVNULL)
    exch = os.path.join(REFSYS, "inst", a.id, "exchange")
    try:
        refctl("stop", "--id", a.id, check=False)
        print(refctl("boot", "--id", a.id, "--profile", a.profile).strip())
        print(refctl("ping", "--id", a.id).strip())
        for f in ("st-tree.json", "st-screen.snap"):
            if os.path.exists(os.path.join(exch, f)):
                os.remove(os.path.join(exch, f))
        out = refctl("cmd", "--id", a.id, "SETCLOCK 1704067200", "RUN SYS:Utilities/Clock",
                     "WAIT_WINDOW Clock 10000", "WAIT_IDLE 5000", "DUMP_TREE st-tree.json",
                     "SNAP st-screen.snap", "QUIT 5000")
        print(out.strip())
        assert "SURVIVOR no" in out, "Clock did not quit"

        tree = json.load(open(os.path.join(exch, "st-tree.json")))
        assert tree["schema"] == "lxa-tree/1"
        wins = [w for s in tree["screens"] for w in s["windows"] if w["title"] == "Clock"]
        assert wins, "no Clock window in the tree"
        clock = wins[0]
        assert clock["app"] is True
        assert len(clock["gadgets"]) >= 4, "Clock window should have its system gadgets"
        assert clock["menus"] and clock["menus"][0]["title"] == "Project"
        assert clock["border"][1] > 0

        snap = open(os.path.join(exch, "st-screen.snap"), "rb").read()
        nl = snap.index(b"\n")
        magic, w, h, depth, ncol = snap[:nl].split()
        assert magic == b"LXASNAP1"
        w, h, ncol = int(w), int(h), int(ncol)
        assert len(snap) == nl + 1 + 3 * ncol + w * h, "snapshot size mismatch"
        print("selftest: OK (Clock %dx%d, %d gadgets, snapshot %dx%d)" % (
            clock["width"], clock["height"], len(clock["gadgets"]), w, h))
        return 0
    except (AssertionError, RuntimeError, KeyError, ValueError, OSError) as e:
        print("selftest: FAILED: %s" % e)
        return 1
    finally:
        refctl("stop", "--id", a.id, check=False)


if __name__ == "__main__":
    sys.exit(main())
