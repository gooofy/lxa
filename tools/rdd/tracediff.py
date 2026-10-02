"""Relay-trace differential (Phase 233, the counterpart of WINE's +relay).

    python3 -m rdd tracediff <lxa-trace.jsonl> <ref-trace.jsonl> [--task NAME]
    python3 -m rdd tracediff <run>/<scenario>          # lxa/trace.jsonl vs ref/trace.jsonl

Both traces use the JSON-lines format of the reference agent's TRACE_DUMP
(lxa writes the same via LXA_TRACE / lxa_trace_start).  Calls are paired
with their returns per task, reduced to comparable values and aligned; the
report names the first divergence: a call missing on one side (call order)
or the same call returning a different value.

Comparable values: the argument registers from the NDK .fd file, typed by
the clib prototypes: pointers / BPTRs / APTRs compare as NULL or non-NULL
(strings they point to exactly), integers exactly; VOID functions have no
return value.
"""

import argparse
import difflib
import json
import os
import sys

from rdd import fd

# task names of the launcher on each backend -> "<app>"
LAUNCHERS = {"exec bootstrap"}
AGENT_TASKS = {"lxaprobe"}     # the reference agent's own calls are not the app's
SMALL = 0x10000


def load(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return [json.loads(ln) for ln in f if ln.strip()]


def value(v, kind=None):
    """kind from the prototype: "ptr" values compare as NULL / ptr only"""
    if v is None:
        return None
    v &= 0xffffffff
    if kind == "ptr":
        return 0 if v == 0 else "ptr"
    if kind == "int" or v < SMALL or v >= 0xffff0000:
        return v if v < 0x80000000 else v - 0x100000000
    return "ptr"


def reduce_calls(recs, task=None):
    """records -> [call dict] in call order (returns attached).

    Only top-level calls count: a traced call made while another traced
    call of the same task is still open is library-internal (AmigaOS ROM
    libraries call each other through their jump tables, lxa's mostly
    directly), so it is not part of the application's behaviour."""
    open_calls = {}
    depth = {}
    calls = []
    for r in recs:
        t = r.get("task") or ""
        if t in LAUNCHERS:
            t = "<app>"
        if task and t != task:
            continue
        key = (t, r["lib"], r["lvo"])
        if "ret" in r:
            stack = open_calls.get(key)
            if stack:
                c = stack.pop()
                if c is not None:
                    rkind = fd.prototypes(c["lib"]).get(c["name"], (None, []))[0]
                    if rkind != "void":
                        c["ret"] = value(r["ret"], rkind)
                depth[t] = depth.get(t, 1) - 1
            continue
        depth[t] = depth.get(t, 0) + 1
        if depth[t] > 1:
            open_calls.setdefault(key, []).append(None)
            continue
        name, regs = fd.functions(r["lib"]).get(r["lvo"], ("LVO%d" % r["lvo"], []))
        args = []
        allregs = {"d%d" % i: r["d"][i] for i in range(8)}
        allregs.update({"a%d" % i: r["a"][i] for i in range(4)})
        pkinds = fd.prototypes(r["lib"]).get(name, (None, []))[1]
        for k, reg in enumerate(regs):
            s = (r.get("str") or {}).get(reg)
            kind = pkinds[k] if k < len(pkinds) else None
            if s is not None and kind != "int":
                args.append((reg, "\"%s\"" % s))
            elif reg in allregs:
                args.append((reg, value(allregs[reg], kind)))
        call = {"task": t, "lib": r["lib"], "lvo": r["lvo"], "name": name, "args": args, "ret": None}
        calls.append(call)
        open_calls.setdefault(key, []).append(call)
    return calls


def sig(c):
    return "%s %s(%s)" % (c["lib"].replace(".library", ""), c["name"],
                          ", ".join("%s=%s" % a for a in c["args"]))


def fmt(c):
    return "%s -> %s" % (sig(c), c["ret"])


def diff(lxa_calls, ref_calls, context=3):
    """-> dict with the first divergence (or None) and alignment stats"""
    a = [sig(c) for c in lxa_calls]
    b = [sig(c) for c in ref_calls]
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    first = None
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == "equal":
            for k in range(i2 - i1):
                la, rb = lxa_calls[i1 + k], ref_calls[j1 + k]
                if la["ret"] != rb["ret"]:
                    first = {"kind": "return", "index_lxa": i1 + k, "index_ref": j1 + k,
                             "call": sig(la), "lxa": la["ret"], "ref": rb["ret"]}
                    break
            if first:
                break
            continue
        first = {"kind": "order", "op": op, "index_lxa": i1, "index_ref": j1,
                 "lxa_calls": [fmt(c) for c in lxa_calls[i1:min(i2, i1 + 5)]],
                 "ref_calls": [fmt(c) for c in ref_calls[j1:min(j2, j1 + 5)]]}
        break
    if first:
        i, j = first["index_lxa"], first["index_ref"]
        first["context_lxa"] = [fmt(c) for c in lxa_calls[max(0, i - context):i]]
        first["context_ref"] = [fmt(c) for c in ref_calls[max(0, j - context):j]]
    return {"calls_lxa": len(a), "calls_ref": len(b), "matching_ratio": round(sm.ratio(), 4),
            "first_divergence": first}


def report(res):
    out = ["calls: lxa %d, reference %d, alignment %.1f%%" % (res["calls_lxa"], res["calls_ref"],
                                                           100 * res["matching_ratio"])]
    d = res["first_divergence"]
    if not d:
        out.append("no divergence")
        return "\n".join(out)
    out.append("first divergence (%s) at lxa call #%d / reference call #%d:" % (
        d["kind"], d["index_lxa"], d["index_ref"]))
    for c in d.get("context_ref", []):
        out.append("    = " + c)
    if d["kind"] == "return":
        out.append("    ! %s" % d["call"])
        out.append("        lxa returns %s, AmigaOS 3.1 returns %s" % (d["lxa"], d["ref"]))
    else:
        for c in d["ref_calls"]:
            out.append("    ref + " + c)
        for c in d["lxa_calls"]:
            out.append("    lxa + " + c)
    return "\n".join(out)


def main(argv=None):
    ap = argparse.ArgumentParser(prog="rdd tracediff")
    ap.add_argument("a", help="lxa trace.jsonl, or a scenario run directory")
    ap.add_argument("b", nargs="?", help="reference trace.jsonl")
    ap.add_argument("--task", help="only this task (launcher tasks are called <app>)")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args(argv)
    if a.b is None:
        la, rb = os.path.join(a.a, "lxa", "trace.jsonl"), os.path.join(a.a, "ref", "trace.jsonl")
    else:
        la, rb = a.a, a.b
    lt, rt = load(la), load(rb)
    task = a.task
    if task is None:
        # the application task: the reference agent names it after the program
        names = [r.get("task") for r in rt if r.get("task") and r.get("task") not in AGENT_TASKS]
        task = None
        if names:
            ref_task = max(set(names), key=names.count)
            rt = [dict(r, task="<app>") if r.get("task") == ref_task else r for r in rt]
            task = "<app>"
    res = diff(reduce_calls(lt, task), reduce_calls(rt, task))
    print(json.dumps(res, indent=1) if a.json else report(res))
    return 1 if res["first_divergence"] else 0


if __name__ == "__main__":
    sys.exit(main())
