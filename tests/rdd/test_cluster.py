"""Phase 216: divergence clustering and phase stubs."""
import json
import os
import shutil
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from rdd import cluster  # noqa: E402


def diff(kind, where, field, lxa, ref):
    return {"kind": kind, "where": where, "field": field, "lxa": lxa, "ref": ref}


class Cluster(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        rows = []
        for scn, extra in (("a", 0x10), ("b", 0x10), ("c", 0)):
            res = {"tree": [diff("screen", "screen[W]", "menu_vborder", 0, 2),
                            diff("window", "screen[W]/window[x]", "flags", 0x1000, 0x08001000 | extra),
                            diff("gadget", "screen[W]/window[x]/gadget[0 id=1]", "top", 20, 11),
                            diff("gadget", "screen[W]/window[x]/gadget[1 id=2]", "top", 30, 21)],
                   "palette": [], "text": {"missing_in_lxa": [], "extra_in_lxa": []}, "stdout_equal": True}
            os.makedirs(os.path.join(self.d, scn))
            with open(os.path.join(self.d, scn, "compare.json"), "w") as f:
                json.dump(res, f)
            rows.append({"scenario": scn, "snapshot": "s", "compare": os.path.join(scn, "compare.json")})
        with open(os.path.join(self.d, "compare-summary.json"), "w") as f:
            json.dump(rows, f)
        self.rm = os.path.join(self.d, "roadmap.md")
        with open(self.rm, "w") as f:
            f.write("### Phase 223 — x\n- [ ] `MenuVBorder` fix: menu_vborder 0\n")

    def tearDown(self):
        shutil.rmtree(self.d)

    def test_clusters(self):
        cl = {c["signature"]: c for c in cluster.cluster_run(self.d, roadmap=self.rm)}
        mv = cl["screen.menu_vborder: lxa 0, reference 2"]
        self.assertEqual(mv["apps"], ["a", "b", "c"])
        self.assertEqual(mv["phases"], [223])            # owned via roadmap text
        # bit-level signatures: c lacks only the VISITOR bit, a/b lack two bits
        self.assertEqual(cl["window.flags: lxa lacks 0x8000000"]["apps"], ["c"])
        self.assertEqual(cl["window.flags: lxa lacks 0x8000010"]["apps"], ["a", "b"])
        # per-gadget geometry folds into one cluster per scenario set
        self.assertEqual(len(cl["gadget geometry (top) differs"]["evidence"]), 3)

    def test_stubs_only_for_unowned_multi_app(self):
        cl = cluster.cluster_run(self.d, roadmap=self.rm)
        md = cluster.phase_stubs(cl, 290)
        self.assertIn("### Phase 290 — ", md)
        self.assertNotIn("menu_vborder", md)
        self.assertNotIn("lacks 0x8000000\n", md)        # single-app cluster
        self.assertIn("lxa lacks 0x8000010", md)


if __name__ == "__main__":
    unittest.main()
