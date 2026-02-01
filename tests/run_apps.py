#!/usr/bin/env python3
"""
lxa Application Test Runner

This script executes Amiga applications from the lxa-apps repository using lxa.
It reads app.json manifests to configure assigns, libraries, and environment
before launching the application.

Usage:
    ./run_apps.py [app_name]       # Run a specific app
    ./run_apps.py --list           # List available apps
    ./run_apps.py --test [app]     # Run app tests (if defined)

The script expects:
    - lxa binary at: build/host/bin/lxa
    - ROM at: build/target/rom/lxa.rom
    - Apps at: ../lxa-apps/<app_name>/

App manifest format (app.json):
{
    "name": "KickPascal 2",
    "version": "2.0",
    "executable": "bin/KP2/KP",
    "assigns": {
        "KP2": "bin/KP2",
        "LIBS": "bin/KP2/libs"
    },
    "libraries": ["icon.library", "diskfont.library"],
    "env": {
        "WORKDIR": "bin/KP2"
    },
    "test": {
        "timeout": 10,
        "expected_output": "KICK-PASCAL",
        "args": ""
    }
}
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


class AppRunner:
    """Runs Amiga applications with configured environment."""

    def __init__(self, base_dir: Path):
        self.base_dir = base_dir
        self.apps_dir = base_dir.parent / "lxa-apps"
        self.lxa_bin = base_dir / "build" / "host" / "bin" / "lxa"
        self.lxa_rom = base_dir / "build" / "target" / "rom" / "lxa.rom"
        self.sys_dir = base_dir / "build" / "target" / "sys"

    def validate_setup(self) -> bool:
        """Verify all required files exist."""
        errors = []

        if not self.lxa_bin.exists():
            errors.append(f"lxa binary not found: {self.lxa_bin}")
        if not self.lxa_rom.exists():
            errors.append(f"ROM not found: {self.lxa_rom}")
        if not self.apps_dir.exists():
            errors.append(f"lxa-apps directory not found: {self.apps_dir}")

        if errors:
            for err in errors:
                print(f"ERROR: {err}", file=sys.stderr)
            return False
        return True

    def list_apps(self) -> list:
        """List all available apps with manifests."""
        apps = []
        if not self.apps_dir.exists():
            return apps

        for app_dir in self.apps_dir.iterdir():
            if app_dir.is_dir():
                manifest = app_dir / "app.json"
                if manifest.exists():
                    try:
                        with open(manifest) as f:
                            data = json.load(f)
                        apps.append(
                            {
                                "id": app_dir.name,
                                "name": data.get("name", app_dir.name),
                                "version": data.get("version", "unknown"),
                                "path": app_dir,
                            }
                        )
                    except json.JSONDecodeError:
                        apps.append(
                            {
                                "id": app_dir.name,
                                "name": app_dir.name,
                                "version": "unknown",
                                "path": app_dir,
                                "error": "Invalid app.json",
                            }
                        )
                else:
                    # App without manifest - still include it
                    apps.append(
                        {
                            "id": app_dir.name,
                            "name": app_dir.name,
                            "version": "unknown",
                            "path": app_dir,
                            "error": "No app.json",
                        }
                    )
        return apps

    def load_manifest(self, app_name: str) -> dict:
        """Load app manifest from app.json."""
        app_dir = self.apps_dir / app_name
        manifest = app_dir / "app.json"

        if not manifest.exists():
            return {"error": f"No app.json found for {app_name}"}

        try:
            with open(manifest) as f:
                data = json.load(f)
            data["app_dir"] = str(app_dir)
            return data
        except json.JSONDecodeError as e:
            return {"error": f"Invalid JSON in app.json: {e}"}

    def create_config(self, manifest: dict, work_dir: Path) -> Path:
        """Create a temporary config file with app-specific assigns."""
        config_path = work_dir / "config.ini"

        app_dir = Path(manifest.get("app_dir", "."))

        # Start with base config
        config = [
            "# lxa configuration for app execution",
            "[system]",
            "",
            "[drives]",
            f"SYS = {self.sys_dir}",
            "",
            "[assigns]",
        ]

        # Add assigns from manifest
        assigns = manifest.get("assigns", {})
        for assign, path in assigns.items():
            full_path = app_dir / path
            if full_path.exists():
                config.append(f"{assign} = {full_path}")
            else:
                print(
                    f"WARNING: Assign path doesn't exist: {full_path}", file=sys.stderr
                )

        # Ensure LIBS: points to app's libs if specified
        if "LIBS" not in assigns and (app_dir / "libs").exists():
            config.append(f"LIBS = {app_dir / 'libs'}")

        config.append("")
        config.append("[floppies]")
        config.append("")

        with open(config_path, "w") as f:
            f.write("\n".join(config))

        return config_path

    def run_app(
        self, app_name: str, args: list = None, timeout: int = None
    ) -> subprocess.CompletedProcess:
        """Run an application with its configured environment."""
        if args is None:
            args = []

        manifest = self.load_manifest(app_name)
        if "error" in manifest:
            print(f"ERROR: {manifest['error']}", file=sys.stderr)
            return subprocess.CompletedProcess(
                args=[], returncode=1, stdout="", stderr=manifest["error"]
            )

        app_dir = Path(manifest["app_dir"])
        executable = manifest.get("executable", "")

        if not executable:
            print(f"ERROR: No executable specified in app.json", file=sys.stderr)
            return subprocess.CompletedProcess(
                args=[], returncode=1, stdout="", stderr="No executable"
            )

        exe_path = app_dir / executable
        if not exe_path.exists():
            print(f"ERROR: Executable not found: {exe_path}", file=sys.stderr)
            return subprocess.CompletedProcess(
                args=[], returncode=1, stdout="", stderr="Executable not found"
            )

        # Create temp directory for config
        with tempfile.TemporaryDirectory() as work_dir:
            work_path = Path(work_dir)
            config_path = self.create_config(manifest, work_path)

            # Build lxa command
            cmd = [
                str(self.lxa_bin),
                "-r",
                str(self.lxa_rom),
                "-c",
                str(config_path),
                str(exe_path),
            ] + args

            print(f"Running: {' '.join(cmd)}")

            # Run with timeout
            run_timeout = timeout or manifest.get("test", {}).get("timeout", 60)

            try:
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    timeout=run_timeout,
                    cwd=str(app_dir),
                )
                return result
            except subprocess.TimeoutExpired:
                print(
                    f"TIMEOUT: App exceeded {run_timeout}s time limit", file=sys.stderr
                )
                return subprocess.CompletedProcess(
                    args=cmd, returncode=-1, stdout="", stderr="Timeout"
                )

    def test_app(self, app_name: str) -> bool:
        """Run application tests defined in manifest."""
        manifest = self.load_manifest(app_name)
        if "error" in manifest:
            print(f"ERROR: {manifest['error']}", file=sys.stderr)
            return False

        test_config = manifest.get("test", {})
        if not test_config:
            print(f"No test configuration for {app_name}")
            return True  # No tests = pass

        args = test_config.get("args", "").split() if test_config.get("args") else []
        timeout = test_config.get("timeout", 30)
        expected = test_config.get("expected_output", "")
        expected_returncode = test_config.get("expected_returncode", 0)

        result = self.run_app(app_name, args, timeout)

        success = True

        # Check return code
        if result.returncode != expected_returncode:
            print(
                f"FAIL: Expected return code {expected_returncode}, got {result.returncode}"
            )
            success = False

        # Check expected output
        if expected and expected not in result.stdout:
            print(f"FAIL: Expected output not found: '{expected}'")
            print(f"Actual output: {result.stdout[:500]}")
            success = False

        if success:
            print(f"PASS: {app_name}")
        else:
            print(f"FAIL: {app_name}")
            if result.stderr:
                print(f"STDERR: {result.stderr}")

        return success


def main():
    parser = argparse.ArgumentParser(description="lxa Application Test Runner")
    parser.add_argument("app", nargs="?", help="Application to run")
    parser.add_argument("--list", "-l", action="store_true", help="List available apps")
    parser.add_argument("--test", "-t", action="store_true", help="Run app tests")
    parser.add_argument("--timeout", type=int, help="Override timeout (seconds)")
    parser.add_argument(
        "--args", nargs="*", default=[], help="Arguments to pass to app"
    )
    parser.add_argument("--all", "-a", action="store_true", help="Test all apps")

    args = parser.parse_args()

    # Determine base directory (where this script is located)
    script_dir = Path(__file__).parent.resolve()
    base_dir = script_dir.parent  # Go up from tests/ to lxa/

    runner = AppRunner(base_dir)

    if args.list:
        apps = runner.list_apps()
        if not apps:
            print("No apps found in lxa-apps/")
            print(f"Expected location: {runner.apps_dir}")
            return 1

        print(f"Available apps in {runner.apps_dir}:")
        print()
        for app in apps:
            status = f" ({app['error']})" if "error" in app else ""
            print(f"  {app['id']:20} {app['name']} v{app['version']}{status}")
        return 0

    if args.all:
        if not runner.validate_setup():
            return 1

        apps = runner.list_apps()
        passed = 0
        failed = 0

        for app in apps:
            if "error" not in app or app["error"] != "No app.json":
                if runner.test_app(app["id"]):
                    passed += 1
                else:
                    failed += 1

        print()
        print(f"Results: {passed} passed, {failed} failed")
        return 0 if failed == 0 else 1

    if args.app:
        if not runner.validate_setup():
            return 1

        if args.test:
            return 0 if runner.test_app(args.app) else 1
        else:
            result = runner.run_app(args.app, args.args, args.timeout)
            print(result.stdout)
            if result.stderr:
                print(result.stderr, file=sys.stderr)
            return result.returncode

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
