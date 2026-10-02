#!/usr/bin/env python3
"""App corpus manifests (Phase 230).

    python3 tools/rdd/corpus.py --check      # validate apps/*.json against ../lxa-apps
    python3 tools/rdd/corpus.py [--list]     # list the catalogued apps
    python3 -m rdd corpus --check            # same, via the rdd CLI

The corpus binaries live outside the repository in ``../lxa-apps`` (env
``LXA_APPS``); the manifests ``apps/<App>.json`` are checked in (format:
apps/README.md).  ``--check`` verifies

  * every manifest against the schema below (always, also without the apps);
  * every directory in LXA_APPS has a manifest (matched by its ``dir``);
  * every manifest's directory, executable (an AmigaOS hunk file) and assign
    targets exist;
  * every alias manifest (``"alias_of": "<App>"``, a duplicate directory such
    as a case-variant symlink) resolves into its target's directory.

Exit codes: 0 ok, 1 problems found, 77 the manifests are valid but LXA_APPS is
not installed (CTest skip).
"""

import argparse
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
APPS_META = os.path.join(ROOT, "apps")


def apps_dir():
    return os.environ.get("LXA_APPS") or os.path.normpath(os.path.join(ROOT, "..", "lxa-apps"))


HUNK_HEADER = b"\x00\x00\x03\xf3"
KINDS = ("gui", "cli")
PROFILES = ("aga", "rtg")
TOP_KEYS = {"dir", "name", "version", "description", "executable", "kind", "assigns", "libraries",
            "env", "requirements", "test", "source", "notes"}
REQUIRED = ("dir", "name", "version", "description", "executable")
REQ_KEYS = {"graphics": bool, "intuition": bool, "kickstart_version": int, "profile": str, "cpu": str}
TEST_KEYS = {"timeout": (int, float), "expected_returncode": int, "args": str, "description": str}
ALIAS_KEYS = {"dir", "alias_of", "notes"}


def _is_str_list(v):
    return isinstance(v, list) and all(isinstance(x, str) for x in v)


def validate(m, name="?"):
    """Schema check of one manifest -> [error strings]."""
    errs = []
    if not isinstance(m, dict):
        return ["%s: manifest is not a JSON object" % name]
    if "alias_of" in m:
        for k in sorted(set(m) - ALIAS_KEYS):
            errs.append("%s: unknown key %r in alias manifest" % (name, k))
        for k in ("dir", "alias_of"):
            if not isinstance(m.get(k), str) or not m.get(k):
                errs.append("%s: %s must be a non-empty string" % (name, k))
        if "notes" in m and not _is_str_list(m["notes"]):
            errs.append("%s: notes must be a list of strings" % name)
        return errs
    for k in REQUIRED:
        if not isinstance(m.get(k), str) or not m.get(k):
            errs.append("%s: %s must be a non-empty string" % (name, k))
    for k in sorted(set(m) - TOP_KEYS):
        errs.append("%s: unknown key %r" % (name, k))
    if m.get("kind", "gui") not in KINDS:
        errs.append("%s: kind must be one of %s" % (name, "/".join(KINDS)))
    ex = m.get("executable")
    if isinstance(ex, str) and (ex.startswith("/") or ":" in ex or ".." in ex.split("/")):
        errs.append("%s: executable must be relative to the app directory" % name)
    assigns = m.get("assigns", {})
    if not isinstance(assigns, dict) or not all(isinstance(k, str) and isinstance(v, str)
                                                for k, v in assigns.items()):
        errs.append("%s: assigns must map names to relative paths" % name)
    else:
        for k, v in assigns.items():
            if k.endswith(":") or not k:
                errs.append("%s: assign name %r must not end in ':'" % (name, k))
            if v.startswith("/") or ":" in v or ".." in v.split("/"):
                errs.append("%s: assign %s must be relative to the app directory" % (name, k))
            if " " in m.get("dir", "") or " " in v:
                errs.append("%s: assign %s: paths with spaces cannot be assigned on the reference" % (name, k))
    if "libraries" in m and not (_is_str_list(m["libraries"]) and
                                 all(x.lower().endswith(".library") for x in m["libraries"])):
        errs.append("%s: libraries must be a list of *.library names" % name)
    env = m.get("env", {})
    if not isinstance(env, dict) or not all(isinstance(v, str) for v in env.values()):
        errs.append("%s: env must map names to strings" % name)
    req = m.get("requirements", {})
    if not isinstance(req, dict):
        errs.append("%s: requirements must be an object" % name)
    else:
        for k, v in req.items():
            if k not in REQ_KEYS:
                errs.append("%s: unknown requirement %r" % (name, k))
            elif not isinstance(v, REQ_KEYS[k]) or (REQ_KEYS[k] is int and isinstance(v, bool)):
                errs.append("%s: requirements.%s has the wrong type" % (name, k))
        if req.get("profile", "aga") not in PROFILES:
            errs.append("%s: requirements.profile must be one of %s" % (name, "/".join(PROFILES)))
    test = m.get("test", {})
    if not isinstance(test, dict):
        errs.append("%s: test must be an object" % name)
    else:
        for k, v in test.items():
            if k not in TEST_KEYS:
                errs.append("%s: unknown test key %r" % (name, k))
            elif not isinstance(v, TEST_KEYS[k]) or isinstance(v, bool):
                errs.append("%s: test.%s has the wrong type" % (name, k))
    if "source" in m and not isinstance(m["source"], str):
        errs.append("%s: source must be a string" % name)
    if "notes" in m and not _is_str_list(m["notes"]):
        errs.append("%s: notes must be a list of strings" % name)
    return errs


def load(meta=APPS_META):
    """{manifest name (file stem) -> manifest}; unreadable files map to None."""
    out = {}
    for fn in sorted(os.listdir(meta)):
        if not fn.endswith(".json"):
            continue
        try:
            with open(os.path.join(meta, fn)) as f:
                out[fn[:-5]] = json.load(f)
        except ValueError as e:
            out[fn[:-5]] = e
    return out


def apps(manifests):
    """Only the real (non-alias, valid) app manifests."""
    return {k: v for k, v in manifests.items() if isinstance(v, dict) and "alias_of" not in v}


def check_schema(manifests):
    errs = []
    dirs = {}
    for name, m in manifests.items():
        if isinstance(m, Exception):
            errs.append("%s: invalid JSON: %s" % (name, m))
            continue
        errs += validate(m, name)
        if isinstance(m, dict) and isinstance(m.get("dir"), str):
            if m["dir"] in dirs:
                errs.append("%s: dir %r already claimed by %s" % (name, m["dir"], dirs[m["dir"]]))
            dirs[m["dir"]] = name
        if isinstance(m, dict) and "alias_of" in m:
            t = manifests.get(m["alias_of"])
            if not isinstance(t, dict) or "alias_of" in t:
                errs.append("%s: alias_of %r is not an app manifest" % (name, m["alias_of"]))
    return errs


def is_hunk(path):
    try:
        with open(path, "rb") as f:
            return f.read(4) == HUNK_HEADER
    except OSError:
        return False


def check_install(manifests, root):
    """Problems between the manifests and the installed corpus at root."""
    errs = []
    by_dir = {m["dir"]: n for n, m in manifests.items() if isinstance(m, dict) and isinstance(m.get("dir"), str)}
    for entry in sorted(os.listdir(root)):
        p = os.path.join(root, entry)
        if entry.startswith(".") or not os.path.isdir(p):
            continue
        if entry not in by_dir:
            errs.append("%s: directory in %s has no manifest apps/<App>.json" % (entry, root))
    for name, m in sorted(manifests.items()):
        if not isinstance(m, dict) or not isinstance(m.get("dir"), str):
            continue
        d = os.path.join(root, m["dir"])
        if not os.path.isdir(d):
            errs.append("%s: app directory %s missing" % (name, d))
            continue
        if "alias_of" in m:
            t = manifests.get(m["alias_of"])
            if isinstance(t, dict) and isinstance(t.get("dir"), str):
                tdir = os.path.realpath(os.path.join(root, t["dir"]))
                real = os.path.realpath(d)
                if real != tdir and not real.startswith(tdir + os.sep):
                    errs.append("%s: alias directory %s does not resolve into %s" % (name, d, tdir))
            continue
        exe = os.path.join(d, m.get("executable", ""))
        if not os.path.isfile(exe):
            errs.append("%s: executable %s missing" % (name, exe))
        elif not is_hunk(exe):
            errs.append("%s: executable %s is not an AmigaOS hunk file" % (name, exe))
        for a, rel in sorted((m.get("assigns") or {}).items()):
            if not os.path.isdir(os.path.join(d, rel)):
                errs.append("%s: assign %s: target %s missing" % (name, a, os.path.join(d, rel)))
        wd = (m.get("env") or {}).get("WORKDIR")
        if wd is not None and not os.path.isdir(os.path.join(d, wd)):
            errs.append("%s: WORKDIR %s missing" % (name, wd))
        for a, b in case_collisions(d):
            errs.append("%s: %s and %s differ only in case (one of them is invisible on AmigaOS)" % (name, a, b))
    return errs


def case_collisions(top, maxdepth=4):
    """Pairs of sibling entries whose names differ only in case: AmigaDOS
    file systems are case-insensitive, so one of them cannot be reached on
    the reference (APPS: is a directory mount)."""
    out = []
    for dp, dn, fn in os.walk(top):
        if dp[len(top):].count(os.sep) >= maxdepth:
            dn[:] = []
        seen = {}
        for e in sorted(dn + fn):
            k = e.lower()
            if k in seen:
                out.append((os.path.relpath(os.path.join(dp, seen[k]), top), os.path.relpath(os.path.join(dp, e), top)))
            else:
                seen[k] = e
    return out


def listing(manifests):
    rows = []
    for name, m in sorted(apps(manifests).items(), key=lambda kv: kv[0].lower()):
        rows.append("%-14s %-34s %-18s %-4s %s" % (name, m.get("name", "")[:34], m.get("version", "")[:18],
                                                  m.get("kind", "gui"), m["dir"] + "/" + m.get("executable", "")))
    aliases = sorted(n for n, m in manifests.items() if isinstance(m, dict) and "alias_of" in m)
    rows.append("%d apps, %d aliases (%s)" % (len(apps(manifests)), len(aliases), ", ".join(aliases)))
    return rows


def main(argv=None):
    ap = argparse.ArgumentParser(prog="corpus", description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true", help="validate manifests and the installed corpus")
    ap.add_argument("--list", action="store_true", help="list the catalogued apps")
    ap.add_argument("--apps", help="corpus directory (default: $LXA_APPS or ../lxa-apps)")
    a = ap.parse_args(argv)
    manifests = load()
    if not a.check:
        print("\n".join(listing(manifests)))
        return 0
    errs = check_schema(manifests)
    root = a.apps or apps_dir()
    installed = os.path.isdir(root)
    if installed:
        errs += check_install(manifests, root)
    for e in errs:
        print("ERROR " + e)
    if a.list:
        print("\n".join(listing(manifests)))
    if errs:
        print("corpus check: %d problems" % len(errs))
        return 1
    if not installed:
        print("SKIP corpus check: %s not installed (set LXA_APPS); %d manifests valid" % (root, len(manifests)))
        return 77
    print("corpus check: %d apps, %d manifests, all present (%s)" % (len(apps(manifests)), len(manifests), root))
    return 0


if __name__ == "__main__":
    sys.exit(main())
