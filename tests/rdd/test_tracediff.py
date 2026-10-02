"""Phase 233: relay-trace differential."""
import json
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from rdd import fd, tracediff  # noqa: E402

BUILD = os.environ.get("LXA_BUILD", os.path.join(ROOT, "build"))
REF_TRACE = os.path.join(ROOT, "tests", "traces", "trace-fileio.ref.jsonl")


def call(lib, lvo, d=(0,) * 8, a=(0,) * 4, task="app", **strs):
    return {"lib": lib, "lvo": lvo, "task": task, "d": list(d), "a": list(a),
            "str": {k: strs.get(k) for k in ("d1", "d2", "a0", "a1")}}


def ret(lib, lvo, v, task="app"):
    return {"lib": lib, "lvo": lvo, "task": task, "ret": v}


class Units(unittest.TestCase):
    def test_fd_and_prototypes(self):
        self.assertEqual(fd.name_of("graphics.library", -60), "Text")
        self.assertEqual(fd.lvo_of("dos.library", "Seek"), -66)
        self.assertEqual(fd.prototypes("dos.library")["Write"], ("int", ["ptr", "ptr", "int"]))
        self.assertIn("CloseWindow", fd.void_functions("intuition.library"))
        self.assertEqual(fd.lxa_spec("dos:Open,Close"), "dos.library:-30,-36")

    def test_nested_calls_are_internal(self):
        recs = [call("gadtools.library", -30), call("graphics.library", -72), ret("graphics.library", -72, 5),
                ret("gadtools.library", -30, 0x123456)]
        calls = tracediff.reduce_calls(recs, "app")
        self.assertEqual([c["name"] for c in calls], ["CreateGadgetA"])
        self.assertEqual(calls[0]["ret"], "ptr")

    def test_return_and_order_divergence(self):
        base = [call("dos.library", -66, d=(0, 0x1000, 0, 0xffffffff, 0, 0, 0, 0)), ret("dos.library", -66, 13)]
        same = tracediff.diff(tracediff.reduce_calls(base), tracediff.reduce_calls(base))
        self.assertIsNone(same["first_divergence"])
        bad = [base[0], ret("dos.library", -66, 0)]
        d = tracediff.diff(tracediff.reduce_calls(bad), tracediff.reduce_calls(base))["first_divergence"]
        self.assertEqual((d["kind"], d["lxa"], d["ref"]), ("return", 0, 13))
        extra = base + [call("dos.library", -36), ret("dos.library", -36, 0xffffffff)]
        d = tracediff.diff(tracediff.reduce_calls(base), tracediff.reduce_calls(extra))["first_divergence"]
        self.assertEqual(d["kind"], "order")


@unittest.skipUnless(os.path.exists(os.path.join(BUILD, "host", "lib", "liblxa.so")), "liblxa not built")
class InjectedBug(unittest.TestCase):
    """Test gate: tracediff names a deliberately injected return-value bug."""

    def run_lxa(self, inject=None):
        out = tempfile.mkdtemp(prefix="tracediff-")
        env = dict(os.environ, PYTHONPATH=os.path.join(ROOT, "tools"))
        env.pop("LXA_TRACE_INJECT", None)
        if inject:
            env["LXA_TRACE_INJECT"] = inject
        subprocess.run([sys.executable, "-m", "rdd.backend_lxa",
                        os.path.join(ROOT, "tests", "scenarios", "trace-fileio.yaml"), out, "--build", BUILD],
                       env=env, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        lt = tracediff.load(os.path.join(out, "trace.jsonl"))
        rt = tracediff.load(REF_TRACE)
        task = max({r["task"] for r in rt}, key=[r["task"] for r in rt].count)
        rt = [dict(r, task="<app>") if r["task"] == task else r for r in rt]
        return tracediff.diff(tracediff.reduce_calls(lt, "<app>"), tracediff.reduce_calls(rt, "<app>"))

    def test_clean_run_matches_reference(self):
        res = self.run_lxa()
        self.assertIsNone(res["first_divergence"], json.dumps(res, indent=1))
        self.assertEqual(res["calls_lxa"], res["calls_ref"])

    def test_injected_seek_bug_is_located(self):
        res = self.run_lxa("dos.library:-66:7")
        d = res["first_divergence"]
        self.assertIsNotNone(d)
        self.assertEqual(d["kind"], "return")
        self.assertTrue(d["call"].startswith("dos Seek("), d)
        self.assertEqual(d["lxa"], d["ref"] + 7)


if __name__ == "__main__":
    unittest.main()
