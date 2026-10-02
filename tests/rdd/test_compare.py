"""Phase 214: comparator, report and findings validator."""
import json
import os
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from rdd import bundle, compare, findings, report  # noqa: E402


def tree(backend, wbor_top=2, gadget_top=20, title="Win"):
    return {"schema": "lxa-tree/1", "backend": backend, "screens": [{
        "title": "Workbench Screen", "default_title": "Workbench Screen", "left": 0, "top": 0,
        "width": 32, "height": 16, "depth": 2, "display_id": 0x8000, "flags": 1, "bar_height": 10,
        "bar_vborder": 1, "bar_hborder": 5, "menu_vborder": 2, "menu_hborder": 4,
        "wbor": [4, wbor_top, 4, 2], "font": {"name": "topaz.font", "ysize": 8, "xsize": 8, "baseline": 6},
        "windows": [{"title": title, "screen_title": None, "app": True, "left": 0, "top": 0, "width": 30,
                     "height": 14, "min_width": 0, "min_height": 0, "max_width": 30, "max_height": 14,
                     "flags": 0, "idcmp": 0, "border": [4, 11, 4, 2], "detail_pen": 0, "block_pen": 1,
                     "font": None, "menus": [],
                     "gadgets": [{"id": 1, "type": 1, "flags": 0, "activation": 1, "left": 4,
                                  "top": gadget_top, "width": 10, "height": 4,
                                  "text": [{"text": "OK", "left": 0, "top": 0, "fpen": 1, "bpen": 0,
                                            "drawmode": 1}]}]}]}]}


def make_bundle(d, backend, pens, **kw):
    tp = os.path.join(d, "t-%s.json" % backend)
    json.dump(tree(backend, **kw), open(tp, "w"))
    snap = b"LXASNAP1 32 16 2 4\n" + bytes([0, 0, 0, 255, 255, 255, 0, 0, 0, 102, 136, 187]) + pens
    out = os.path.join(d, "run", "s", backend, "startup")
    bundle.write_bundle(out, backend, "s", "startup", snap, tp, stdout="x")
    return out


class Compare(unittest.TestCase):
    def test_identical(self):
        with tempfile.TemporaryDirectory() as d:
            pens = bytes(32 * 16)
            a, b = make_bundle(d, "lxa", pens), make_bundle(d, "ref", pens)
            res, _ = compare.compare_bundles(a, b)
            self.assertEqual(res["verdict"], "identical")

    def test_tree_and_pixel_diffs(self):
        with tempfile.TemporaryDirectory() as d:
            pa = bytearray(32 * 16)
            pa[5 * 32 + 5] = 1
            pa[15 * 32 + 31] = 2
            a = make_bundle(d, "lxa", bytes(pa), wbor_top=11, gadget_top=29)
            b = make_bundle(d, "ref", bytes(32 * 16))
            res, mask = compare.compare_bundles(a, b)
            fields = {(t["kind"], t["field"]) for t in res["tree"]}
            self.assertIn(("screen", "wbor"), fields)
            self.assertIn(("gadget", "top"), fields)
            self.assertEqual(res["pixels"]["diff_pixels"], 2)
            regions = res["pixels"]["regions"]
            self.assertEqual(len(regions), 1, "adjacent 16 px cells merge into one region")
            self.assertEqual((regions[0]["w"], regions[0]["h"]), (32, 16))
            self.assertEqual(sum(mask), 2)
            self.assertEqual(res["verdict"], "tree")

    def test_report_renders(self):
        with tempfile.TemporaryDirectory() as d:
            make_bundle(d, "lxa", bytes([1]) + bytes(32 * 16 - 1))
            make_bundle(d, "ref", bytes(32 * 16))
            rows = report.build(os.path.join(d, "run"))
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]["diff_pixels"], 1)
            self.assertTrue(os.path.exists(os.path.join(d, "run", "report.html")))
            if rows[0]["composite"]:
                self.assertTrue(os.path.exists(os.path.join(d, "run", rows[0]["composite"])))


class Findings(unittest.TestCase):
    def test_checked_in_findings_validate(self):
        import glob
        import yaml
        for p in glob.glob(os.path.join(ROOT, "doc", "findings", "*.yaml")):
            self.assertEqual(findings.validate(yaml.safe_load(open(p))), [], p)

    def test_finding_without_action_rejected(self):
        doc = {"scenario": "x", "findings": [{"id": "a", "category": "layout", "description": "d",
                                               "evidence": ["e"], "action": {}}]}
        self.assertTrue(findings.validate(doc))


if __name__ == "__main__":
    unittest.main()
