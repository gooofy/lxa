"""Function descriptions from the NDK .fd files (Phase 233).

    from rdd.fd import functions, lvo_of, name_of
    functions("graphics.library")  -> {lvo: ("Text", ["a1", "a0", "d0"])}

LVO = -bias, -bias-6, ... in file order (##private entries count too).
"""

import functools
import os
import re

FD_DIRS = [os.environ.get("LXA_FD_DIR", ""), "/opt/amiga/m68k-amigaos/ndk/lib/fd",
           "/opt/amiga/m68k-amigaos/lib/fd"]


def _fd_path(lib):
    base = lib.replace(".library", "").replace(".device", "")
    for d in FD_DIRS:
        for name in ("%s_lib.fd" % base, "%s.fd" % base):
            p = os.path.join(d, name) if d else ""
            if p and os.path.exists(p):
                return p
    return None


@functools.lru_cache(maxsize=None)
def functions(lib):
    p = _fd_path(lib)
    out = {}
    if not p:
        return out
    bias = 30
    with open(p, encoding="latin-1") as f:
        for line in f:
            line = line.strip()
            if line.startswith("##bias"):
                bias = int(line.split()[1])
                continue
            if not line or line.startswith("*") or line.startswith("##"):
                continue
            m = re.match(r"(\w+)\(([^)]*)\)\(([^)]*)\)", line)
            if not m:
                continue
            regs = [r.strip().lower() for r in re.split(r"[,/]", m.group(3)) if r.strip()]
            out[-bias] = (m.group(1), regs)
            bias += 6
    return out


PROTO_DIRS = ["/opt/amiga/m68k-amigaos/ndk-include/clib", "/opt/amiga/m68k-amigaos/ndk13-include/clib"]


POINTERISH = re.compile(r"\*|\b(?:CONST_)?(?:BPTR|APTR|STRPTR|BSTR)\b|\bPLANEPTR\b|\bCONST_APTR\b")


@functools.lru_cache(maxsize=None)
def prototypes(lib):
    """{name: (return_kind, [param_kind...])} from clib/<lib>_protos.h;
    kind is "void", "ptr" (pointers, BPTR, APTR, strings) or "int"."""
    base = lib.replace(".library", "").replace(".device", "")
    for d in PROTO_DIRS:
        p = os.path.join(d, "%s_protos.h" % base)
        if not os.path.exists(p):
            continue
        with open(p, encoding="latin-1") as f:
            text = re.sub(r"/\*.*?\*/", " ", f.read(), flags=re.S)
        out = {}
        for m in re.finditer(r"^\s*([A-Za-z_][\w\s\*]*?)\s*\b(\w+)\s*\(([^;{]*?)\)\s*;", text, re.M):
            ret = re.sub(r"\b__\w+\b", "", m.group(1)).strip()
            params = [x.strip() for x in m.group(3).split(",")]
            kinds = [] if params in ([""], ["VOID"], ["void"]) else \
                ["ptr" if POINTERISH.search(x) else "int" for x in params]
            rkind = "void" if ret in ("VOID", "void") else "ptr" if POINTERISH.search(ret) else "int"
            out[m.group(2)] = (rkind, kinds)
        return out
    return {}


def void_functions(lib):
    """Names of functions declared VOID (d0 is garbage after them)."""
    return frozenset(n for n, (r, _) in prototypes(lib).items() if r == "void")


def name_of(lib, lvo):
    f = functions(lib).get(lvo)
    return f[0] if f else "LVO%d" % lvo


def lvo_of(lib, name):
    if re.match(r"^-?\d+$", str(name)):
        v = int(name)
        return -abs(v)
    for lvo, (n, _) in functions(lib).items():
        if n == name:
            return lvo
    raise KeyError("%s: no function %s" % (lib, name))


def spec_to_lvos(spec):
    """"graphics.library:Text,Move;dos.library:*" -> [(lib, [lvo...] or None)]"""
    out = []
    for part in spec.split(";"):
        part = part.strip()
        if not part:
            continue
        lib, _, fns = part.partition(":")
        if not lib.endswith((".library", ".device")):
            lib += ".library"
        if not fns or fns == "*":
            out.append((lib, None))
        else:
            out.append((lib, [lvo_of(lib, f.strip()) for f in fns.split(",") if f.strip()]))
    return out


def lxa_spec(spec):
    return ";".join("%s:%s" % (lib, "*" if lvos is None else ",".join(str(v) for v in lvos))
                    for lib, lvos in spec_to_lvos(spec))
