#!/usr/bin/env python3
"""Merged host + ROM coverage report for lxa (Phase 202).

  tools/coverage_report.py [--build build] [--run-tests] [--out build/coverage]

--run-tests  clears old data, runs the full ctest suite with
             LXA_ROM_COVERAGE=<out> (ROM bitmap) and, when the host was built
             with -DLXA_COVERAGE=ON, collects gcov data as well.

Outputs (in --out):
  rom.info / host.info / lxa.info   lcov tracefiles (lxa.info = merged)
  index.html                        per-file summary table
  summary.txt                       totals (also printed)
and the per-LVO table doc/coverage/lvo-coverage.md (+ .json).

ROM coverage: the emulator records every executed ROM word address
(src/lxa/lxa_coverage.c).  Addresses are mapped to source lines through the
linker map (object .text bases) and `objdump -dl` of the -g ROM objects.
"""

import argparse
import glob
import gzip
import html
import json
import os
import re
import subprocess
import sys
from collections import defaultdict

OBJDUMP = os.environ.get("M68K_OBJDUMP", "/opt/amiga/bin/m68k-amigaos-objdump")
NM = os.environ.get("M68K_NM", "/opt/amiga/bin/m68k-amigaos-nm")
PRAGMA_DIR = os.environ.get("NDK_PRAGMAS", "/opt/amiga/m68k-amigaos/ndk-include/pragmas")
ROM_START = 0xF80000
ROM_SIZE = 512 * 1024

# AmigaOS 3.x system libraries/devices/resources tracked in the LVO table
# (ReAction/ClassAct classes are 3.5+ and not part of the 3.1 target).
SYSTEM_LIBS = """amigaguide asl battclock battmem bullet cardres cia colorwheel
commodities console datatypes disk diskfont dos exec expansion gadtools graphics
icon iffparse input intuition keymap layers locale lowlevel mathffp
mathieeedoubbas mathieeedoubtrans mathieeesingbas mathieeesingtrans mathtrans
misc nonvolatile potgo ramdrive realtime rexxsyslib timer translator utility
wb""".split()

# lib name -> ROM object basenames that implement it
LIB_OBJECTS = {"exec": ["exec.o", "exceptions.o"], "wb": ["lxa_workbench.o"]}


def lib_objects(lib):
    return LIB_OBJECTS.get(lib, ["lxa_%s.o" % lib, "lxa_dev_%s.o" % lib])


# --------------------------------------------------------------------------
# ROM
# --------------------------------------------------------------------------

def rom_objects(build):
    """[(base_address, object_path)] from the ROM linker map."""
    mp = os.path.join(build, "target", "rom", "lxa.rom.map")
    res = []
    rx = re.compile(r"^ \.text\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+\.o)\s*$")
    with open(mp) as f:
        for line in f:
            m = rx.match(line)
            if m and int(m.group(2), 16) > 0:
                res.append((int(m.group(1), 16), m.group(3)))
    return res


def load_bitmap(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except FileNotFoundError:
        return b""


def bit_set(bitmap, addr):
    if not bitmap or addr < ROM_START or addr >= ROM_START + ROM_SIZE:
        return False
    w = (addr - ROM_START) >> 1
    return (bitmap[w >> 3] >> (w & 7)) & 1 == 1


def objdump_lines(obj):
    """[(offset, file, line)] for every instruction of a -g object."""
    out = subprocess.run([OBJDUMP, "-dl", obj], capture_output=True, text=True).stdout
    cur = None
    res = []
    loc = re.compile(r"^(/\S+\.[ch]):(\d+)")
    ins = re.compile(r"^\s+([0-9a-f]+):\t")
    for line in out.splitlines():
        m = loc.match(line)
        if m:
            cur = (m.group(1), int(m.group(2)))
            continue
        m = ins.match(line)
        if m and cur:
            res.append((int(m.group(1), 16), cur[0], cur[1]))
    return res


def object_functions(obj):
    """{name: offset} of text symbols (incl. static) in an object."""
    out = subprocess.run([NM, obj], capture_output=True, text=True).stdout
    res = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "tT":
            name = p[2][1:] if p[2].startswith("_") else p[2]
            res[name] = int(p[0], 16)
    return res


def rom_coverage(build, bitmap):
    """Returns (lines: {file: {line: hits}}, funcs: {file: {name: (line, hit)}},
    symbols: {objbase: {name: hit}})."""
    lines = defaultdict(dict)
    funcs = defaultdict(dict)
    symbols = {}
    for base, obj in rom_objects(build):
        if not os.path.exists(obj):
            continue
        first_line = {}
        for off, src, ln in objdump_lines(obj):
            hit = 1 if bit_set(bitmap, base + off) else 0
            lines[src][ln] = max(lines[src].get(ln, 0), hit)
            first_line.setdefault(off, (src, ln))
        syms = {}
        for name, off in object_functions(obj).items():
            hit = bit_set(bitmap, base + off)
            syms[name] = hit
            if off in first_line:
                src, ln = first_line[off]
                funcs[src][name] = (ln, 1 if hit else 0)
        symbols[os.path.basename(obj)] = syms
    return lines, funcs, symbols


# --------------------------------------------------------------------------
# Host (gcov)
# --------------------------------------------------------------------------

def host_coverage(build):
    lines = defaultdict(dict)
    funcs = defaultdict(dict)
    gcdas = glob.glob(os.path.join(build, "host", "**", "*.gcda"), recursive=True)
    for gcda in gcdas:
        objdir = os.path.dirname(gcda)
        r = subprocess.run(["gcov", "--json-format", "--stdout", gcda],
                           capture_output=True, text=True, cwd=objdir)
        for chunk in r.stdout.splitlines():
            chunk = chunk.strip()
            if not chunk.startswith("{"):
                continue
            data = json.loads(chunk)
            cwd = data.get("current_working_directory", objdir)
            for f in data.get("files", []):
                src = f["file"]
                if not os.path.isabs(src):
                    src = os.path.normpath(os.path.join(cwd, src))
                if "/src/lxa/" not in src or "/m68k" in os.path.basename(src):
                    continue    # skip system headers and the Musashi core
                for l in f.get("lines", []):
                    n = l["line_number"]
                    lines[src][n] = lines[src].get(n, 0) + l["count"]
                for fn in f.get("functions", []):
                    name = fn["name"]
                    prev = funcs[src].get(name, (fn["start_line"], 0))
                    funcs[src][name] = (fn["start_line"], prev[1] + fn["execution_count"])
    return lines, funcs, len(gcdas)


# --------------------------------------------------------------------------
# Output
# --------------------------------------------------------------------------

def write_lcov(path, lines, funcs, test_name):
    with open(path, "w") as f:
        for src in sorted(lines):
            f.write("TN:%s\nSF:%s\n" % (test_name, src))
            for name, (ln, hits) in sorted(funcs.get(src, {}).items(), key=lambda x: x[1][0]):
                f.write("FN:%d,%s\n" % (ln, name))
            for name, (ln, hits) in sorted(funcs.get(src, {}).items(), key=lambda x: x[1][0]):
                f.write("FNDA:%d,%s\n" % (hits, name))
            fl = funcs.get(src, {})
            f.write("FNF:%d\nFNH:%d\n" % (len(fl), sum(1 for v in fl.values() if v[1])))
            for ln in sorted(lines[src]):
                f.write("DA:%d,%d\n" % (ln, lines[src][ln]))
            hit = sum(1 for v in lines[src].values() if v)
            f.write("LF:%d\nLH:%d\nend_of_record\n" % (len(lines[src]), hit))


def merge(a, b):
    out = defaultdict(dict)
    for src_map in (a, b):
        for src, ls in src_map.items():
            for ln, h in ls.items():
                out[src][ln] = out[src].get(ln, 0) + h
    return out


def totals(lines):
    lf = sum(len(v) for v in lines.values())
    lh = sum(1 for v in lines.values() for h in v.values() if h)
    return lf, lh


def write_html(path, groups):
    rows = []
    for title, lines in groups:
        for src in sorted(lines):
            lf = len(lines[src])
            lh = sum(1 for h in lines[src].values() if h)
            pct = 100.0 * lh / lf if lf else 0.0
            rows.append("<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%.1f%%</td></tr>" % (
                title, html.escape(os.path.relpath(src)), lh, lf, pct))
    with open(path, "w") as f:
        f.write("<!doctype html><meta charset=utf-8><title>lxa coverage</title>"
                "<style>body{font-family:sans-serif}td,th{padding:2px 8px}"
                "tr:nth-child(even){background:#eee}</style><h1>lxa coverage</h1>"
                "<table><tr><th>Part</th><th>File</th><th>Hit</th><th>Lines</th><th>%</th></tr>")
        f.write("\n".join(rows))
        f.write("</table>")


def parse_pragmas():
    """{lib: [(lvo, func)]} for the tracked system libraries."""
    res = {}
    rx = re.compile(r"^#pragma\s+(?:libcall|syscall)\s+(?:(\w+)\s+)?(\w+)\s+([0-9a-fA-F]+)\s")
    for lib in SYSTEM_LIBS:
        p = os.path.join(PRAGMA_DIR, "%s_pragmas.h" % lib)
        if not os.path.exists(p):
            continue
        entries = []
        with open(p, errors="replace") as f:
            for line in f:
                m = rx.match(line)
                if not m:
                    continue
                if line.split()[1] == "syscall":
                    func, off = line.split()[2], line.split()[3]
                else:
                    func, off = m.group(2), m.group(3)
                entries.append((-int(off, 16), func))
        res[lib] = sorted(set(entries), key=lambda e: -e[0])
    return res


def disk_libraries(root):
    try:
        with open(os.path.join(root, "sys", "CMakeLists.txt")) as f:
            txt = f.read()
    except FileNotFoundError:
        return set()
    return set(re.findall(r"add_disk_library\(\w+\s+(\w+)\.(?:library|device)", txt))


def load_disklibs(out):
    """{(libname_without_suffix, lvo): hit} from disklibs.cov records."""
    res = {}
    try:
        with open(os.path.join(out, "disklibs.cov")) as f:
            for line in f:
                p = line.split()
                if len(p) != 3:
                    continue
                name = p[0].rsplit(".", 1)[0]
                key = (name, int(p[1]))
                res[key] = res.get(key, 0) or int(p[2])
    except FileNotFoundError:
        pass
    return res


def lvo_table(symbols, root, measured, disk_hits=None):
    """One row per (library, LVO).  Pragma aliases that share an offset
    (DoPkt0..4, System/SystemTagList, *TagList/*A pairs) are one LVO."""
    pragmas = parse_pragmas()
    disk = disk_libraries(root)
    table = []
    prefixes_for = lambda lib: (lib + "_", "g_lxa_" + lib + "_", "exec_" if lib == "exec" else lib + "_",
                                "workbench_" if lib == "wb" else lib + "_")
    for lib, entries in sorted(pragmas.items()):
        syms = {}
        for o in lib_objects(lib):
            syms.update(symbols.get(o, {}))
        norm = {s.lstrip("_"): s for s in syms}
        by_lvo = defaultdict(list)
        for lvo, func in entries:
            by_lvo[lvo].append(func)
        for lvo in sorted(by_lvo, reverse=True):
            names = by_lvo[lvo]
            impl = []
            for func in names:
                for n, s in norm.items():
                    if n.endswith("_" + func) and n.startswith(prefixes_for(lib)):
                        impl.append(s)
            if lib in disk:
                hit = (disk_hits or {}).get((lib, lvo))
                if hit is None:
                    status = "disk library (not loaded)"
                elif hit:
                    status = "tested"
                else:
                    status = "untested"
            elif not impl:
                status = "no ROM symbol"
            elif not measured:
                status = "not measured"
            elif any(syms[s] for s in impl):
                status = "tested"
            else:
                status = "untested"
            table.append({"lib": lib, "lvo": lvo, "function": "/".join(names),
                          "symbol": impl[0].lstrip("_") if impl else "", "status": status})
    return table


def write_lvo(table, root):
    d = os.path.join(root, "doc", "coverage")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "lvo-coverage.json"), "w") as f:
        json.dump(table, f, indent=1)
    per_lib = defaultdict(lambda: defaultdict(int))
    for e in table:
        per_lib[e["lib"]][e["status"]] += 1
    statuses = ["tested", "untested", "no ROM symbol", "disk library (not loaded)"]
    with open(os.path.join(d, "lvo-coverage.md"), "w") as f:
        f.write("# Per-LVO test coverage\n\n")
        f.write("Generated by `make coverage` (tools/coverage_report.py). A public LVO is\n"
                "*tested* when the full test suite executes the entry of its ROM\n"
                "implementation. LVO list: NDK 3.2 pragmas, so V44+ (OS 3.5-3.2) functions\n"
                "such as exec MinList helpers or utility Strncpy appear as well; lxa targets\n"
                "the V40 (OS 3.1) API first. Disk libraries (rexxsyslib, amigaguide,\n"
                "datatypes) are measured through their RAM jump tables; *not loaded*\n"
                "means no test ever opened the library.\n\n")
        tot = defaultdict(int)
        f.write("| Library | " + " | ".join(statuses) + " |\n|---|" + "---|" * len(statuses) + "\n")
        for lib in sorted(per_lib):
            f.write("| %s | %s |\n" % (lib, " | ".join(str(per_lib[lib][s]) for s in statuses)))
            for s in statuses:
                tot[s] += per_lib[lib][s]
        f.write("| **total** | %s |\n\n" % " | ".join("**%d**" % tot[s] for s in statuses))
        f.write("## Untested and unimplemented LVOs\n\n")
        for lib in sorted(per_lib):
            miss = [e for e in table if e["lib"] == lib and e["status"] in ("untested", "no ROM symbol")]
            if not miss:
                continue
            f.write("- **%s**: %s\n" % (lib, ", ".join(
                "%s%s" % (e["function"], "" if e["status"] == "untested" else "*") for e in miss)))
        f.write("\n`*` = no implementing symbol found in the ROM object.\n")
    return tot


def run_tests(build, out):
    for g in glob.glob(os.path.join(build, "host", "**", "*.gcda"), recursive=True):
        os.remove(g)
    for f in glob.glob(os.path.join(out, "*")):
        if os.path.isfile(f):
            os.remove(f)
    env = dict(os.environ, LXA_ROM_COVERAGE=os.path.abspath(out))
    r = subprocess.run(["ctest", "--test-dir", build, "-j16", "--timeout", "180"], env=env)
    if r.returncode != 0:
        print("coverage_report: WARNING - test suite reported failures", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--build", default="build")
    ap.add_argument("--out", default=None)
    ap.add_argument("--run-tests", action="store_true")
    a = ap.parse_args()
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    build = os.path.abspath(a.build)
    out = os.path.abspath(a.out or os.path.join(build, "coverage"))
    os.makedirs(out, exist_ok=True)

    if a.run_tests:
        run_tests(build, out)

    bitmap = load_bitmap(os.path.join(out, "rom.cov"))
    rom_lines, rom_funcs, symbols = rom_coverage(build, bitmap)
    host_lines, host_funcs, ngcda = host_coverage(build)

    write_lcov(os.path.join(out, "rom.info"), rom_lines, rom_funcs, "rom")
    write_lcov(os.path.join(out, "host.info"), host_lines, host_funcs, "host")
    allf = dict(rom_funcs)
    allf.update(host_funcs)
    write_lcov(os.path.join(out, "lxa.info"), merge(rom_lines, host_lines), allf, "lxa")
    write_html(os.path.join(out, "index.html"), [("ROM", rom_lines), ("host", host_lines)])
    tot = write_lvo(lvo_table(symbols, root, bool(bitmap), load_disklibs(out)), root)

    rf, rh = totals(rom_lines)
    hf, hh = totals(host_lines)
    lines = [
        "ROM  line coverage: %d/%d (%.1f%%)%s" % (rh, rf, 100.0 * rh / rf if rf else 0,
                                                 "" if bitmap else "  [no rom.cov - run with --run-tests]"),
        "host line coverage: %d/%d (%.1f%%)%s" % (hh, hf, 100.0 * hh / hf if hf else 0,
                                                 "" if ngcda else "  [no gcov data - configure -DLXA_COVERAGE=ON]"),
        "LVOs: %d tested, %d untested, %d without ROM symbol, %d in never-loaded disk libraries" % (
            tot["tested"], tot["untested"], tot["no ROM symbol"], tot["disk library (not loaded)"]),
        "report: %s/index.html, merged tracefile: %s/lxa.info" % (out, out),
    ]
    with open(os.path.join(out, "summary.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
