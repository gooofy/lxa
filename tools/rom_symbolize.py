#!/usr/bin/env python3
"""Map lxa ROM addresses to function symbols (including static functions).

The ROM is linked to an Amiga hunk file, so there is no ELF with debug info.
This tool combines the linker map (`lxa.rom.map`: which object's .text lands
where) with `m68k-amigaos-nm` of each object file (all symbols, including
`static` ones) to resolve an absolute ROM address to `object:function+off`.

Usage:
    tools/rom_symbolize.py [--build build] 0x00f95368 00fd4878 ...
    echo "pc=00f95368" | tools/rom_symbolize.py --stdin   # rewrite text

Also importable: RomSymbolizer(build_dir).lookup(addr) -> (obj, sym, off).
"""

import argparse
import bisect
import os
import re
import subprocess
import sys

NM = os.environ.get("M68K_NM", "/opt/amiga/bin/m68k-amigaos-nm")


class RomSymbolizer:
    def __init__(self, build_dir="build"):
        self.map_path = os.path.join(build_dir, "target", "rom", "lxa.rom.map")
        self.addrs = []
        self.entries = []   # (addr, obj, sym)
        self._load()

    def _load(self):
        text_re = re.compile(r"^ \.text\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+\.o)\s*$")
        syms = []
        with open(self.map_path) as f:
            for line in f:
                m = text_re.match(line)
                if not m:
                    continue
                base = int(m.group(1), 16)
                obj = m.group(3)
                if not os.path.exists(obj):
                    continue
                out = subprocess.run([NM, obj], capture_output=True, text=True).stdout
                for nl in out.splitlines():
                    parts = nl.split()
                    if len(parts) != 3 or parts[1] not in ("t", "T"):
                        continue
                    name = parts[2]
                    if name.startswith("_"):
                        name = name[1:]
                    syms.append((base + int(parts[0], 16), os.path.basename(obj), name))
        syms.sort()
        self.entries = syms
        self.addrs = [s[0] for s in syms]

    def lookup(self, addr):
        i = bisect.bisect_right(self.addrs, addr) - 1
        if i < 0:
            return None
        a, obj, sym = self.entries[i]
        return obj, sym, addr - a

    def format(self, addr):
        r = self.lookup(addr)
        if not r:
            return "0x%08x" % addr
        obj, sym, off = r
        return "%s:%s+0x%x" % (obj, sym, off)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--build", default="build")
    ap.add_argument("--stdin", action="store_true",
                    help="annotate every 00fxxxxx hex word read from stdin")
    ap.add_argument("addrs", nargs="*")
    a = ap.parse_args()
    s = RomSymbolizer(a.build)
    if a.stdin:
        pat = re.compile(r"\b(?:0x)?(00f[89a-f][0-9a-f]{4})\b", re.I)
        for line in sys.stdin:
            sys.stdout.write(pat.sub(lambda m: "%s<%s>" % (m.group(0), s.format(int(m.group(1), 16))), line))
        return
    for x in a.addrs:
        print("%s %s" % (x, s.format(int(x, 16))))


if __name__ == "__main__":
    main()
