"""Phase 230: app corpus manifests, compat-DB rating derivation, rating-drop
detection and the dashboard (synthetic data; no emulator, no reference)."""
import json
import os
import shutil
import sys
import tempfile
import unittest

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from rdd import compat, corpus  # noqa: E402
from rdd.scenario import normalise_step  # noqa: E402

HUNK = b"\x00\x00\x03\xf3" + b"\x00" * 28


def manifest(**kw):
    m = {"dir": "Foo", "name": "Foo", "version": "1.0", "description": "test app", "executable": "bin/Foo",
         "assigns": {"LIBS": "Libs"}, "libraries": ["foo.library"], "env": {"WORKDIR": "bin"},
         "requirements": {"graphics": True, "intuition": True, "kickstart_version": 37, "profile": "aga"},
         "test": {"timeout": 10, "expected_returncode": 0, "args": ""}, "notes": []}
    m.update(kw)
    return m


class ManifestSchema(unittest.TestCase):
    def test_valid(self):
        self.assertEqual(corpus.validate(manifest(), "Foo"), [])

    def test_missing_and_unknown_keys(self):
        m = manifest(bogus=1)
        del m["executable"]
        errs = corpus.validate(m, "Foo")
        self.assertTrue(any("executable" in e for e in errs))
        self.assertTrue(any("bogus" in e for e in errs))

    def test_bad_values(self):
        for bad in (dict(executable="/abs/Foo"), dict(executable="APPS:Foo"), dict(assigns={"LIBS:": "Libs"}),
                    dict(assigns={"LIBS": "../x"}), dict(libraries=["foo.lib"]), dict(kind="tui"),
                    dict(requirements={"kickstart_version": "37"}), dict(requirements={"profile": "ocs"}),
                    dict(test={"timeout": True}), dict(dir="Foo Bar", assigns={"X": ""})):
            self.assertNotEqual(corpus.validate(manifest(**bad), "Foo"), [], bad)

    def test_alias(self):
        ms = {"Foo": manifest(), "FOO_alias": {"dir": "FOO", "alias_of": "Foo"}, "Bad": {"dir": "B", "alias_of": "Nope"}}
        errs = corpus.check_schema(ms)
        self.assertEqual([e for e in errs if e.startswith("FOO")], [])
        self.assertTrue(any(e.startswith("Bad") for e in errs))

    def test_case_colliding_manifest_names(self):
        errs = corpus.check_schema({"Foo": manifest(), "FOO": {"dir": "FOO", "alias_of": "Foo"}})
        self.assertTrue(any("differs only in case" in e for e in errs))

    def test_duplicate_dir(self):
        errs = corpus.check_schema({"A": manifest(), "B": manifest()})
        self.assertTrue(any("already claimed" in e for e in errs))

    def test_checked_in_manifests_are_valid(self):
        ms = corpus.load()
        self.assertEqual(corpus.check_schema(ms), [])
        self.assertGreaterEqual(len(corpus.apps(ms)), 35)


class InstallCheck(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        os.makedirs(os.path.join(self.d, "Foo", "bin"))
        os.makedirs(os.path.join(self.d, "Foo", "Libs"))
        with open(os.path.join(self.d, "Foo", "bin", "Foo"), "wb") as f:
            f.write(HUNK)
        os.symlink("Foo", os.path.join(self.d, "FOO"))

    def tearDown(self):
        shutil.rmtree(self.d)

    def test_ok(self):
        ms = {"Foo": manifest(), "FOO": {"dir": "FOO", "alias_of": "Foo"}}
        self.assertEqual(corpus.check_install(ms, self.d), [])

    def test_unmanifested_dir_and_missing_files(self):
        os.makedirs(os.path.join(self.d, "Orphan"))
        with open(os.path.join(self.d, "Foo", "bin", "Foo"), "wb") as f:
            f.write(b"not a hunk file")
        ms = {"Foo": manifest(assigns={"FONTS": "fonts"}), "Gone": manifest(dir="Gone")}
        errs = "\n".join(corpus.check_install(ms, self.d))
        self.assertIn("Orphan", errs)
        self.assertIn("FOO", errs)                 # symlink without alias manifest
        self.assertIn("not an AmigaOS hunk", errs)
        self.assertIn("FONTS", errs)
        self.assertIn("Gone", errs)

    def test_case_collision(self):
        os.makedirs(os.path.join(self.d, "Foo", "libs"))
        ms = {"Foo": manifest(), "FOO": {"dir": "FOO", "alias_of": "Foo"}}
        self.assertTrue(any("differ only in case" in e for e in corpus.check_install(ms, self.d)))

    def test_cli_exit_codes(self):
        old = os.environ.get("LXA_APPS")
        try:
            os.environ["LXA_APPS"] = os.path.join(self.d, "missing")
            self.assertEqual(corpus.main(["--check"]), 77)
        finally:
            if old is None:
                os.environ.pop("LXA_APPS", None)
            else:
                os.environ["LXA_APPS"] = old


STEPS = [normalise_step(s) for s in ["launch", {"wait_window": "Foo"}, "wait_idle", {"snapshot": "s"}, "quit"]]


def result(fail_at=None, n=len(STEPS)):
    steps = [{"step": STEPS[i][0], "ok": True} for i in range(n)]
    if fail_at is not None:
        steps = steps[:fail_at + 1]
        steps[fail_at] = {"step": STEPS[fail_at][0], "ok": False, "error": "boom"}
    return {"ok": fail_at is None, "steps": steps}


def cmp(verdict="identical", tree=(), diff_pixels=0, stdout_equal=True):
    return {"verdict": verdict, "tree": list(tree), "pixels": {"diff_pixels": diff_pixels},
            "stdout_equal": stdout_equal}


class Rating(unittest.TestCase):
    def rate(self, lres, rres, compares, golden=None):
        return compat.rate_scenario(STEPS, lres, rres, compares, golden)[0]

    def test_platinum(self):
        self.assertEqual(self.rate(result(), result(), {"s": cmp()}), "platinum")

    def test_gold_pixels_only(self):
        self.assertEqual(self.rate(result(), result(), {"s": cmp("pixels", diff_pixels=40)}), "gold")

    def test_gold_needs_golden_budget(self):
        g = {"snapshots": {"s": {"pixel_budget": 10}}}
        self.assertEqual(self.rate(result(), result(), {"s": cmp("pixels", diff_pixels=40)}, g), "silver")
        g["snapshots"]["s"]["pixel_budget"] = 50
        self.assertEqual(self.rate(result(), result(), {"s": cmp("pixels", diff_pixels=40)}, g), "gold")

    def test_silver_tree_diff(self):
        d = {"kind": "window", "where": "w", "field": "flags", "lxa": 1, "ref": 2}
        self.assertEqual(self.rate(result(), result(), {"s": cmp("tree", tree=[d])}), "silver")
        self.assertEqual(self.rate(result(), result(), {"s": cmp("output", stdout_equal=False)}), "silver")

    def test_bronze_later_step(self):
        self.assertEqual(self.rate(result(fail_at=3), result(), {}), "bronze")

    def test_garbage_no_window_or_crash(self):
        self.assertEqual(self.rate(result(fail_at=1), result(), {}), "garbage")
        self.assertEqual(self.rate(None, result(), {}), "garbage")

    def test_untested_without_oracle(self):
        self.assertEqual(self.rate(result(), result(fail_at=1), {}), compat.UNTESTED)
        self.assertEqual(self.rate(result(), None, {}), compat.UNTESTED)

    def test_ref_fails_late_caps_at_silver(self):
        self.assertEqual(self.rate(result(), result(fail_at=3), {}), "silver")

    def test_cli_window_step_is_launch(self):
        steps = [normalise_step(s) for s in ["launch", {"frames": 10}, {"snapshot": "s"}]]
        self.assertEqual(compat.window_step(steps), 0)

    def test_app_rating_is_worst_tested(self):
        self.assertEqual(compat.rate_app(["gold", "untested", "silver"]), "silver")
        self.assertEqual(compat.rate_app(["untested"]), "untested")
        self.assertEqual(compat.rate_app([]), "untested")


class Database(unittest.TestCase):
    MANIFESTS = {"Foo": {"name": "Foo", "version": "1"}, "Bar": {"name": "Bar", "version": "2"},
                 "Baz": {"name": "Baz", "version": "3"}}

    def evaluated(self, foo="gold"):
        return {"Foo": {"rating": foo, "scenarios": {"tests/scenarios/apps/foo.yaml": {"rating": foo}},
                        "divergences": [{"signature": "window.flags: lxa 1, reference 2", "phase": 222,
                                         "owned": True}]}}

    def test_merge_keeps_unrun_apps(self):
        old = compat.merge({}, {"Bar": {"rating": "silver", "scenarios": {}, "divergences": []}},
                           self.MANIFESTS, "0.1.0", "2026-01-01")
        new = compat.merge(old, self.evaluated(), self.MANIFESTS, "0.2.0", "2026-02-01")
        self.assertEqual(new["apps"]["Foo"]["rating"], "gold")
        self.assertEqual(new["apps"]["Foo"]["last_tested"], "0.2.0")
        self.assertEqual(new["apps"]["Bar"]["last_tested"], "0.1.0")      # not re-run: kept
        self.assertEqual(new["apps"]["Baz"]["rating"], "untested")
        self.assertEqual(new["ratings"]["gold"], 1)
        self.assertEqual(sum(new["ratings"].values()), 3)

    def test_drop_detection(self):
        old = compat.merge({}, self.evaluated("gold"), self.MANIFESTS, "0.1.0")
        self.assertEqual(compat.drops(old, compat.merge(old, self.evaluated("platinum"), self.MANIFESTS, "x")), [])
        self.assertEqual(compat.drops(old, compat.merge(old, self.evaluated("silver"), self.MANIFESTS, "x")),
                         [("Foo", "gold", "silver")])
        self.assertEqual(compat.drops(old, compat.merge(old, self.evaluated("untested"), self.MANIFESTS, "x")),
                         [("Foo", "gold", "untested")])
        # an app that was never rated cannot drop
        self.assertEqual(compat.drops({}, old), [])

    def test_divergences_carry_phases(self):
        clusters = [{"signature": "a", "apps": ["s1", "s2"], "phases": [223]},
                    {"signature": "b", "apps": ["s1"], "phases": []},
                    {"signature": "c", "apps": ["s3"], "phases": [224]}]
        d = compat.divergences_for("s1", clusters)
        self.assertEqual([(x["signature"], x["phase"], x["owned"]) for x in d],
                         [("a", 223, True), ("b", compat.TRIAGE_PHASE, False)])

    def test_write_and_dashboard(self):
        d = tempfile.mkdtemp()
        try:
            db = compat.merge({}, self.evaluated(), self.MANIFESTS, "0.2.0", "2026-02-01")
            path = os.path.join(d, "compat.yaml")
            compat.write_db(db, path)
            with open(path) as f:
                text = f.read()
            self.assertTrue(text.startswith("# lxa compatibility database"))
            self.assertEqual(yaml.safe_load(text)["apps"]["Foo"]["rating"], "gold")
            with open(compat.render_dashboard(compat.load_db(path), os.path.join(d, "x", "d.html"))) as f:
                html = f.read()
            self.assertIn("<title>lxa Compatibility</title>", html)
            self.assertIn("Phase 222", html)
            self.assertIn("prefers-color-scheme:dark", html)
            for name in self.MANIFESTS:
                self.assertIn(">%s<" % name, html)
        finally:
            shutil.rmtree(d)


class EvaluateRun(unittest.TestCase):
    """evaluate_run on a synthetic run directory using a real app scenario."""

    def test_run_dir(self):
        scen = compat.app_scenarios()
        self.assertGreaterEqual(len(scen), 35)
        app, paths = "DirectoryOpus", compat.app_scenarios()["DirectoryOpus"]
        from rdd.scenario import Scenario
        scn = Scenario(os.path.join(ROOT, paths[0]))
        d = tempfile.mkdtemp()
        try:
            n = len(scn.steps)
            for b in ("lxa", "ref"):
                os.makedirs(os.path.join(d, scn.name, b))
                with open(os.path.join(d, scn.name, b, "result.json"), "w") as f:
                    json.dump({"ok": True, "steps": [{"step": k, "ok": True} for k, _ in scn.steps[:n]]}, f)
            os.makedirs(os.path.join(d, scn.name, "compare", "startup"))
            with open(os.path.join(d, scn.name, "compare", "startup", "compare.json"), "w") as f:
                json.dump(cmp("pixels", diff_pixels=3), f)
            ev = compat.evaluate_run(d, {app: paths}, [])
            self.assertEqual(ev[app]["rating"], "gold")
            self.assertEqual(list(ev[app]["scenarios"]), paths)
        finally:
            shutil.rmtree(d)


if __name__ == "__main__":
    unittest.main()
