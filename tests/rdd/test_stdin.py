"""Phase 237e: an emulator run does not read its stdin.  A write to an
unmapped address (exec/getmsgnull writes through ROM code bytes taken as
list pointers) used to start the interactive debugger, which read debugger
commands from the stdin the test runner shares with every other run: a
stray "s" made the emulator single-step, other input made it wait."""
import os
import subprocess
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BUILD = os.environ.get("LXA_BUILD", os.path.join(ROOT, "build"))
PROBE = "Tests/Probes/exec/getmsgnull"


class StdinTest(unittest.TestCase):
    def test_debugger_commands_on_stdin_change_nothing(self):
        if not os.path.exists(os.path.join(BUILD, "target", "samples", "Samples", PROBE)):
            self.skipTest("probe not built")
        want = open(os.path.join(ROOT, "tests", "probes", "exec", "getmsgnull.ref.out")).read()
        out = os.path.join(BUILD, "test-stdin.json")
        r = subprocess.run([sys.executable, "-m", "rdd.suite_ref", "--lxa-one", PROBE, "--build", BUILD,
                            "--timeout", "20000", "--result", out],
                           input="s\n" * 200 + "c\n", capture_output=True, text=True,
                           cwd=os.path.join(ROOT, "tools"), timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        import json
        with open(out) as f:
            res = json.load(f)
        os.unlink(out)
        self.assertEqual(res["status"], "exit")
        self.assertEqual(res["stdout"].replace("\r", ""), want)
        self.assertNotIn("LXA DEBUGGER", r.stdout)


if __name__ == "__main__":
    unittest.main()
