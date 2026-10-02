"""Phase 235: LXA_OVERRIDE loads the user's own AmigaOS 3.1 disk library."""
import os
import subprocess
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BUILD = os.environ.get("LXA_BUILD", os.path.join(ROOT, "build"))
WB_LIBS = os.environ.get("LXA_OVERRIDE_DIR", os.path.expanduser("~/.cache/lxa/refsys/SYS-aga/Libs"))

RUN = """
import sys
sys.path.insert(0, %r)
from rdd.pylxa import Lxa
l = Lxa(build=%r)
l.run(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "")
l.wait_exit(10000)
print("RC=%%s" %% l.exit_code())
print(l.output())
l.close()
""" % (os.path.join(ROOT, "tools"), BUILD)


def run(program, args="", override=None):
    env = dict(os.environ)
    env.pop("LXA_OVERRIDE", None)
    if override:
        env["LXA_OVERRIDE"] = override
    r = subprocess.run([sys.executable, "-c", RUN, program, args], capture_output=True, text=True,
                       env=env, errors="replace", timeout=120)
    return r.stdout, r.stderr


@unittest.skipUnless(os.path.exists(os.path.join(BUILD, "host", "lib", "liblxa.so")), "liblxa not built")
@unittest.skipUnless(os.path.exists(os.path.join(WB_LIBS, "iffparse.library")),
                     "no Workbench 3.1 Libs/ (build the reference system)")
class Override(unittest.TestCase):
    def test_libident_switches_to_the_users_binary(self):
        own, _ = run("SYS:Tests/Exec/LibIdent", "iffparse.library")
        wb, err = run("SYS:Tests/Exec/LibIdent", "iffparse.library", "iffparse")
        self.assertIn("LXA_OVERRIDE: iffparse.library from", err)
        self.assertIn("iffparse.library 40.1", wb)
        self.assertNotIn("iffparse.library 40.1", own)

    def test_app_runs_with_overridden_library(self):
        out, _ = run("SYS:Tests/IffParse/Basic", "", "iffparse")
        self.assertIn("RC=0", out)

    def test_rom_resident_libraries_are_refused(self):
        _, err = run("SYS:Tests/Exec/LibIdent", "graphics.library", "graphics")
        self.assertIn("not a disk library of AmigaOS 3.1", err)


if __name__ == "__main__":
    unittest.main()
