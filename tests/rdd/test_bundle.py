"""Phase 212 gate: pylxa produces a snapshot bundle that validates.

Run: python3 -m unittest discover -s tests/rdd   (ctest: rdd_python)
"""
import json
import os
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from rdd import bundle  # noqa: E402
from rdd.pylxa import Lxa  # noqa: E402


class SnapFormat(unittest.TestCase):
    def test_png_roundtrip(self):
        pens = bytes([0, 1, 2, 3] * 6)
        snap = b"LXASNAP1 4 6 2 4\n" + bytes(range(12)) + pens
        w, h, depth, pal, p = bundle.read_snap(snap)
        self.assertEqual((w, h, depth, len(pal)), (4, 6, 2, 4))
        with tempfile.TemporaryDirectory() as d:
            bundle.write_palette_png(os.path.join(d, "s.png"), w, h, pal, p)
            self.assertEqual(bundle.read_palette_png(os.path.join(d, "s.png")), (4, 6, pens))

    def test_bad_snapshot_rejected(self):
        with self.assertRaises(ValueError):
            bundle.read_snap(b"LXASNAP1 4 4 2 4\n" + bytes(12) + bytes(3))


class LxaBundle(unittest.TestCase):
    def test_lxa_backend_bundle_validates(self):
        with tempfile.TemporaryDirectory() as d, Lxa() as lxa:
            lxa.text_start()
            lxa.run("SYS:SimpleGad")
            self.assertTrue(lxa.wait_windows(1, 10000))
            lxa.frames(25)
            lxa.wait_idle()
            lxa.dump_tree(os.path.join(d, "tree.raw.json"))
            lxa.snap(os.path.join(d, "screen.snap"))
            lxa.text_dump(os.path.join(d, "text.raw.jsonl"))
            out = os.path.join(d, "bundle")
            meta = bundle.write_bundle(out, "lxa", "selftest/simplegad", "startup",
                                       open(os.path.join(d, "screen.snap"), "rb").read(),
                                       os.path.join(d, "tree.raw.json"),
                                       text_path=os.path.join(d, "text.raw.jsonl"),
                                       stdout=lxa.output(), unimplemented=lxa.unimplemented(),
                                       extra_meta={"profile": "aga"})
            self.assertEqual(bundle.validate_bundle(out), [])
            tree = json.load(open(os.path.join(out, "tree.json")))
            wins = [w for s in tree["screens"] for w in s["windows"]]
            self.assertTrue(wins, "the SimpleGad window must be in the tree")
            self.assertGreater(len(wins[0]["gadgets"]), 0)
            self.assertEqual(meta["screen"]["depth"], tree["screens"][0]["depth"])

    def test_window_snapshot(self):
        with tempfile.TemporaryDirectory() as d, Lxa() as lxa:
            lxa.run("SYS:SimpleGad")
            self.assertTrue(lxa.wait_windows(1, 10000))
            lxa.frames(10)
            lxa.dump_tree(os.path.join(d, "t.json"))
            lxa.snap(os.path.join(d, "w.snap"), window=0)
            w, h, _, _, _ = bundle.read_snap(open(os.path.join(d, "w.snap"), "rb").read())
            win = json.load(open(os.path.join(d, "t.json")))["screens"][0]["windows"][0]
            self.assertEqual((w, h), (win["width"], win["height"]))


class ReferenceSchema(unittest.TestCase):
    def test_reference_tree_validates(self):
        """The reference agent's tree (from refsys_selftest) uses the same schema."""
        ref = os.path.expanduser("~/.cache/lxa/refsys/inst/9/exchange/st-tree.json")
        if not os.path.exists(ref):
            self.skipTest("no reference tree (run tools/refsys/selftest.py)")
        import jsonschema
        schema = json.load(open(os.path.join(bundle.SCHEMA_DIR, "tree.schema.json")))
        jsonschema.validate(json.load(open(ref)), schema)


if __name__ == "__main__":
    unittest.main()
