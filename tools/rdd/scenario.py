"""Scenario DSL (Phase 213).

A scenario is one YAML file, executed unchanged on both backends:

    name: simplegtgadget-startup        # default: file name
    profile: aga                        # aga | rtg
    app:
      sample: SimpleGTGadget            # lxa sample/test binary (SYS:<path>)
      # manifest: DPaintV               # or an app from apps/<App>.json
      args: ""
    steps:
      - launch
      - wait_window: "GadTools"         # or {title: ..., timeout: ms}
      - wait_idle                       # or {timeout: ms}
      - frames: 25                      # let 25 frames (0.5 s) pass
      - click: {gadget_id: 3}           # {label: "OK"} | {xy: [x, y]}
                                        # + optional window: "title"
      - menu: "Project/Quit..."
      - type: "hello"
      - key: {rawkey: 0x44, qualifier: 0}
      - snapshot: startup               # or {name: x, window: "title"}
      - menus                           # open every menu of the window (RMB held on
                                        # its title), snapshot "menu<N>", cancel
                                        # (or {window: "title"}) - Phase 231
      - quit                            # or {timeout: ms}
      # - trace: "graphics.library:Text,Move;dos.library:*"
      #   relay trace from here on (may precede launch) -> <bundle dir>/../trace.jsonl,
      #   compared with `python3 -m rdd tracediff` (Phase 233)

Steps run in order; a failing step fails the backend run (the bundles taken
so far are kept). See doc/rdd-snapshot.md for the bundle produced by
`snapshot`.
"""

import hashlib
import json
import os

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
APPS_META = os.path.join(ROOT, "apps")
APPS_DIR = os.environ.get("LXA_APPS", os.path.normpath(os.path.join(ROOT, "..", "lxa-apps")))

STEP_KINDS = {"launch", "wait_window", "wait_idle", "frames", "click", "menu", "type",
              "key", "snapshot", "quit", "trace", "menus"}

# assigns that exist on a stock system and are extended, not replaced
SYSTEM_ASSIGNS = {"LIBS", "FONTS", "DEVS", "S", "L", "C", "KEYMAPS", "LOCALE", "HELP",
                  "PRINTERS", "REXX", "ENVARC"}


class ScenarioError(Exception):
    pass


def normalise_step(step):
    """-> (kind, args dict)"""
    if isinstance(step, str):
        kind, arg = step, None
    elif isinstance(step, dict) and len(step) == 1:
        kind, arg = next(iter(step.items()))
    else:
        raise ScenarioError("bad step %r" % (step,))
    if kind not in STEP_KINDS:
        raise ScenarioError("unknown step %r" % kind)
    if arg is None:
        args = {}
    elif isinstance(arg, dict):
        args = dict(arg)
    elif kind == "wait_window":
        args = {"title": arg}
    elif kind == "frames":
        args = {"count": int(arg)}
    elif kind == "menu":
        args = {"path": arg}
    elif kind == "type":
        args = {"text": str(arg)}
    elif kind == "snapshot":
        args = {"name": str(arg)}
    elif kind == "key":
        args = {"rawkey": int(arg)}
    elif kind == "trace":
        args = {"spec": str(arg)}
    else:
        raise ScenarioError("step %s takes a mapping, got %r" % (kind, arg))
    return kind, args


class Scenario:
    def __init__(self, path):
        self.path = os.path.abspath(path)
        with open(path) as f:
            self.raw = f.read()
        doc = yaml.safe_load(self.raw) or {}
        self.name = doc.get("name") or os.path.splitext(os.path.basename(path))[0]
        self.profile = doc.get("profile", "aga")
        app = doc.get("app") or {}
        self.args = app.get("args", "")
        self.sample = app.get("sample")
        self.manifest_name = app.get("manifest")
        if bool(self.sample) == bool(self.manifest_name):
            raise ScenarioError("%s: app needs exactly one of sample/manifest" % path)
        self.manifest = None
        if self.manifest_name:
            mpath = os.path.join(APPS_META, self.manifest_name + ".json")
            if not os.path.exists(mpath):
                raise ScenarioError("no manifest %s" % mpath)
            with open(mpath) as f:
                self.manifest = json.load(f)
        self.steps = [normalise_step(s) for s in doc.get("steps", [])]
        first = [k for k, _ in self.steps if k != "trace"]
        if not first or first[0] != "launch":
            raise ScenarioError("%s: the first step must be launch (trace may precede it)" % path)

    # -- what to run on each backend ----------------------------------------------
    def lxa_program(self):
        if self.sample:
            return "SYS:" + self.sample
        return "APPS:%s/%s" % (self.manifest["dir"], self.manifest["executable"])

    def ref_program(self):
        """Samples are copied to LXAREF:bin/ by the reference backend."""
        if self.sample:
            return "LXAREF:bin/" + os.path.basename(self.sample)
        return "APPS:%s/%s" % (self.manifest["dir"], self.manifest["executable"])

    def sample_host_path(self, build):
        return os.path.join(build, "target", "samples", "Samples", self.sample) if self.sample else None

    def assigns(self):
        """[(name, path relative to the app dir, add?)]"""
        if not self.manifest:
            return []
        return [(n, p, n.upper() in SYSTEM_ASSIGNS)
                for n, p in (self.manifest.get("assigns") or {}).items()]

    def app_host_dir(self):
        return os.path.join(APPS_DIR, self.manifest["dir"]) if self.manifest else None

    def cache_key(self, build, extra=""):
        h = hashlib.sha256()
        h.update(self.raw.encode())
        h.update(json.dumps(self.manifest, sort_keys=True).encode())
        sp = self.sample_host_path(build)
        if sp and os.path.exists(sp):
            with open(sp, "rb") as f:
                h.update(f.read())
        h.update(extra.encode())
        return h.hexdigest()[:24]
