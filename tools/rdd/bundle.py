"""Snapshot bundles (Phase 212) - one format for both backends.

A bundle is a directory (doc/rdd-snapshot.md):

    meta.json        lxa-bundle/1: backend, scenario, snapshot name, files
    screen.png       8-bit palette PNG; pixel value = pen index
    palette.json     [[r,g,b], ...] 8-bit components per pen
    tree.json        lxa-tree/1 Intuition tree
    text.jsonl       Text() calls {"text","x","y"} (optional)
    stdout.txt       program output (optional)
    trace.jsonl      library call trace (optional)
    unimplemented.json  lxa stub telemetry (lxa backend, optional)

Both backends emit the raw LXASNAP1 pen dump; `write_bundle` converts it.
"""

import json
import os
import struct
import zlib

try:
    import jsonschema
except ImportError:  # validation degrades to structural checks
    jsonschema = None

SCHEMA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "schemas")


# --------------------------------------------------------------------------
# LXASNAP1 and palette PNG
# --------------------------------------------------------------------------

def read_snap(data):
    """LXASNAP1 bytes -> (width, height, depth, palette[[r,g,b]], pens bytes)."""
    nl = data.index(b"\n")
    magic, w, h, depth, ncol = data[:nl].split()
    if magic != b"LXASNAP1":
        raise ValueError("not an LXASNAP1 snapshot")
    w, h, depth, ncol = int(w), int(h), int(depth), int(ncol)
    pal_raw = data[nl + 1:nl + 1 + 3 * ncol]
    pens = data[nl + 1 + 3 * ncol:]
    if len(pens) != w * h:
        raise ValueError("snapshot size mismatch: %d pixels for %dx%d" % (len(pens), w, h))
    palette = [list(pal_raw[i:i + 3]) for i in range(0, 3 * ncol, 3)]
    return w, h, depth, palette, pens


def write_palette_png(path, w, h, palette, pens):
    """8-bit indexed PNG whose pixel values are the pen indices."""
    def chunk(tag, body):
        c = struct.pack(">I", len(body)) + tag + body
        return c + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)
    raw = b"".join(b"\0" + pens[y * w:(y + 1) * w] for y in range(h))
    plte = b"".join(bytes(c[:3]) for c in palette[:256]) or b"\0\0\0"
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0))
           + chunk(b"PLTE", plte) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def read_palette_png(path):
    """Inverse of write_palette_png (only for our own files): (w, h, pens)."""
    data = open(path, "rb").read()
    pos, w, h, idat = 8, 0, 0, b""
    while pos < len(data):
        n, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if tag == b"IHDR":
            w, h = struct.unpack(">II", body[:8])
        elif tag == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    pens = b"".join(raw[y * (w + 1) + 1:(y + 1) * (w + 1)] for y in range(h))
    return w, h, pens


# --------------------------------------------------------------------------
# Bundle writer / validator
# --------------------------------------------------------------------------

def write_bundle(out_dir, backend, scenario, snapshot, snap_bytes, tree_path,
                 text_path=None, stdout=None, trace_path=None, unimplemented=None,
                 window=None, extra_meta=None):
    os.makedirs(out_dir, exist_ok=True)
    files = []
    w, h, depth, palette, pens = read_snap(snap_bytes)
    write_palette_png(os.path.join(out_dir, "screen.png"), w, h, palette, pens)
    with open(os.path.join(out_dir, "palette.json"), "w") as f:
        json.dump(palette, f)
    files += ["screen.png", "palette.json"]

    with open(tree_path) as f:
        tree = json.load(f)
    with open(os.path.join(out_dir, "tree.json"), "w") as f:
        json.dump(tree, f, indent=1, sort_keys=True)
    files.append("tree.json")

    if text_path and os.path.exists(text_path):
        with open(text_path, encoding="utf-8", errors="replace") as src, \
                open(os.path.join(out_dir, "text.jsonl"), "w") as dst:
            dst.write(src.read())
        files.append("text.jsonl")
    if stdout is not None:
        with open(os.path.join(out_dir, "stdout.txt"), "w") as f:
            f.write(stdout)
        files.append("stdout.txt")
    if trace_path and os.path.exists(trace_path):
        with open(trace_path) as src, open(os.path.join(out_dir, "trace.jsonl"), "w") as dst:
            dst.write(src.read())
        files.append("trace.jsonl")
    if unimplemented is not None:
        with open(os.path.join(out_dir, "unimplemented.json"), "w") as f:
            json.dump(unimplemented, f, indent=1)
        files.append("unimplemented.json")

    meta = {"schema": "lxa-bundle/1", "backend": backend, "scenario": scenario,
            "snapshot": snapshot, "window": window,
            "screen": {"width": w, "height": h, "depth": depth}, "files": files}
    meta.update(extra_meta or {})
    with open(os.path.join(out_dir, "meta.json"), "w") as f:
        json.dump(meta, f, indent=1, sort_keys=True)
    return meta


def _schema(name):
    with open(os.path.join(SCHEMA_DIR, name)) as f:
        return json.load(f)


def validate_bundle(path):
    """Return a list of problems (empty = valid)."""
    problems = []

    def load(name):
        try:
            with open(os.path.join(path, name)) as f:
                return json.load(f)
        except (OSError, ValueError) as e:
            problems.append("%s: %s" % (name, e))
            return None

    meta = load("meta.json")
    tree = load("tree.json")
    palette = load("palette.json")
    if jsonschema:
        for doc, schema in ((meta, "meta.schema.json"), (tree, "tree.schema.json")):
            if doc is not None:
                try:
                    jsonschema.validate(doc, _schema(schema))
                except jsonschema.ValidationError as e:
                    problems.append("%s: %s at %s" % (schema, e.message, "/".join(map(str, e.path))))
    if meta:
        for fname in meta.get("files", []):
            if not os.path.exists(os.path.join(path, fname)):
                problems.append("missing %s" % fname)
        if tree and tree.get("backend") != meta.get("backend"):
            problems.append("tree backend %s != meta backend %s" % (tree.get("backend"), meta.get("backend")))
    try:
        w, h, pens = read_palette_png(os.path.join(path, "screen.png"))
        if meta and (w, h) != (meta["screen"]["width"], meta["screen"]["height"]):
            problems.append("screen.png size %dx%d != meta" % (w, h))
        if palette is not None and pens and max(pens) >= len(palette):
            problems.append("pen %d outside palette of %d" % (max(pens), len(palette)))
    except (OSError, ValueError, KeyError, zlib.error, struct.error) as e:
        problems.append("screen.png: %s" % e)
    if os.path.exists(os.path.join(path, "text.jsonl")):
        with open(os.path.join(path, "text.jsonl"), encoding="utf-8") as f:
            for i, line in enumerate(f):
                try:
                    rec = json.loads(line)
                    if not {"text", "x", "y"} <= rec.keys():
                        problems.append("text.jsonl:%d missing fields" % (i + 1))
                except ValueError as e:
                    problems.append("text.jsonl:%d %s" % (i + 1, e))
    return problems
