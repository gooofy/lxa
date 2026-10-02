"""Phase 215: golden promotion and the ratchet check."""
import json
import os
import shutil
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from rdd import compare, golden  # noqa: E402
from test_compare import make_bundle  # noqa: E402

PENS_REF = bytes([0] * 512)


def pens(n_diff):
    p = bytearray(PENS_REF)
    for i in range(n_diff):
        p[i] = 1
    return bytes(p)


class Golden(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        self.scn = os.path.join(self.d, "s.yaml")
        with open(self.scn, "w") as f:
            f.write("name: s\napp:\n  sample: Tests/Dos/HelloWorld\nsteps:\n  - launch\n  - snapshot: startup\n")
        self.ref = make_bundle(self.d, "ref", PENS_REF)
        self.lxa = make_bundle(self.d, "lxa", pens(5), wbor_top=11)
        with open(os.path.join(self.d, "run", "s", "ref", "result.json"), "w") as f:
            json.dump({"ok": True, "snapshots": ["startup"]}, f)
        self.root = os.path.join(self.d, "golden")
        self.gdir = golden.promote(self.scn, os.path.join(self.d, "run"), 223, root=self.root)
        with open(os.path.join(self.gdir, "golden.json")) as f:
            self.g = json.load(f)

    def tearDown(self):
        shutil.rmtree(self.d)

    def eval_lxa(self, lpath):
        res, _ = compare.compare_bundles(lpath, os.path.join(self.gdir, "ref", "startup"))
        return golden.evaluate(self.g, "startup", res)

    def test_promote_records_ratchet(self):
        e = self.g["snapshots"]["startup"]
        self.assertEqual(self.g["app"], "HelloWorld")
        self.assertEqual(e["pixel_budget"], 5)
        self.assertEqual(e["pixel_phase"], 223)
        self.assertEqual([(k["field"], k["lxa"], k["phase"]) for k in e["known_tree_diffs"]],
                         [("wbor", [4, 11, 4, 2], 223)])
        self.assertTrue(os.path.exists(os.path.join(self.gdir, "ref", "startup", "screen.png")))

    def test_same_lxa_passes(self):
        self.assertEqual(self.eval_lxa(self.lxa), ([], []))

    def test_injected_regressions_fail(self):
        d2 = os.path.join(self.d, "r1")
        os.makedirs(d2)
        worse_px = make_bundle(d2, "lxa", pens(9), wbor_top=11)
        fails, _ = self.eval_lxa(worse_px)
        self.assertEqual(len(fails), 1)
        self.assertIn("9 pixels differ, budget 5", fails[0])
        d3 = os.path.join(self.d, "r2")
        os.makedirs(d3)
        moved = make_bundle(d3, "lxa", pens(5), wbor_top=11, gadget_top=21)
        fails, _ = self.eval_lxa(moved)
        self.assertTrue(any("new tree diff" in f and ".top" in f for f in fails), fails)
        d4 = os.path.join(self.d, "r3")
        os.makedirs(d4)
        other = make_bundle(d4, "lxa", pens(5), wbor_top=12)   # still wrong, differently
        fails, _ = self.eval_lxa(other)
        self.assertTrue(any("wbor" in f for f in fails), fails)

    def test_improvement_asks_to_tighten(self):
        d2 = os.path.join(self.d, "i")
        os.makedirs(d2)
        better = make_bundle(d2, "lxa", pens(0))
        fails, tighten = self.eval_lxa(better)
        self.assertEqual(fails, [])
        self.assertEqual(len(tighten), 2)

    def test_lint(self):
        rm = os.path.join(self.d, "roadmap.md")
        with open(rm, "w") as f:
            f.write("### Phase 223 — x\n")
        self.assertEqual(golden.lint(self.root, rm), [])
        self.g["disabled"] = {"phase": 999, "reason": "x"}
        with open(os.path.join(self.gdir, "golden.json"), "w") as f:
            json.dump(self.g, f)
        self.assertEqual(len(golden.lint(self.root, rm)), 1)

    def test_repo_goldens_lint_clean(self):
        self.assertEqual(golden.lint(), [])
        self.assertGreaterEqual(len(golden.all_goldens()), 4)


if __name__ == "__main__":
    unittest.main()
