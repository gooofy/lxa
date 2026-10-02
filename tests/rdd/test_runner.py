"""Phase 213 gate: the starter scenarios produce bundles on both backends.

The reference half runs only when the user's AmigaOS media are installed
(results are cached by tools/rdd/backend_ref.py).
"""
import glob
import json
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLS = os.path.join(ROOT, "tools")
sys.path.insert(0, TOOLS)
sys.path.insert(0, os.path.join(TOOLS, "refsys"))

from rdd import bundle  # noqa: E402

SCENARIOS = sorted(glob.glob(os.path.join(ROOT, "tests", "scenarios", "*.yaml")))


def ref_available():
    import importlib.util
    spec = importlib.util.spec_from_file_location("selftest", os.path.join(TOOLS, "refsys", "selftest.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.media_available()


class StarterScenarios(unittest.TestCase):
    def run_rdd(self, backend, out):
        env = dict(os.environ, PYTHONPATH=TOOLS)
        r = subprocess.run([sys.executable, "-m", "rdd", "run", "--backend", backend, "--out", out] + SCENARIOS,
                           cwd=TOOLS, env=env, capture_output=True, text=True, timeout=900)
        summary = json.load(open(os.path.join(out, "summary.json")))
        return r, summary

    def check(self, backend):
        with tempfile.TemporaryDirectory() as out:
            r, summary = self.run_rdd(backend, out)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertEqual(len(summary), len(SCENARIOS))
            for res in summary:
                self.assertTrue(res["ok"], res)
                for snap in res["snapshots"]:
                    path = os.path.join(out, res["scenario"], backend, snap)
                    self.assertEqual(bundle.validate_bundle(path), [], path)

    def test_lxa_backend(self):
        self.check("lxa")

    def test_ref_backend(self):
        if not ref_available():
            self.skipTest("AmigaOS reference media not installed")
        self.check("ref")


if __name__ == "__main__":
    unittest.main()
