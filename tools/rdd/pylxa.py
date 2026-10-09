"""pylxa - Python bindings for liblxa (Phase 212).

Drives the lxa emulator in-process through ctypes, with the same vocabulary
the reference agent (lxaprobe) offers, so tools/rdd can run one scenario on
both backends.

    from rdd.pylxa import Lxa
    with Lxa() as lxa:
        lxa.run("SYS:SimpleGad")
        lxa.wait_windows(1)
        lxa.dump_tree("tree.json")

liblxa keeps global state: one Lxa instance at a time per process
(use a subprocess per scenario for parallelism).
"""

import ctypes
import os
import shutil
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


class LxaConfig(ctypes.Structure):
    _fields_ = [
        ("rom_path", ctypes.c_char_p),
        ("sys_drive", ctypes.c_char_p),
        ("config_path", ctypes.c_char_p),
        ("headless", ctypes.c_bool),
        ("rootless", ctypes.c_bool),
        ("verbose", ctypes.c_bool),
        ("realtime_clock", ctypes.c_bool),
        ("cpu_hz", ctypes.c_uint32),
        ("cycles_per_frame", ctypes.c_uint32),
        ("epoch_secs", ctypes.c_uint64),
        ("strict_unimplemented", ctypes.c_bool),
    ]


class LxaWindowInfo(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int), ("width", ctypes.c_int),
                ("height", ctypes.c_int), ("title", ctypes.c_char * 256)]


class LxaUnimplemented(ctypes.Structure):
    _fields_ = [("lib", ctypes.c_char * 32), ("function", ctypes.c_char * 48),
                ("detail", ctypes.c_char * 128), ("first_task", ctypes.c_char * 64),
                ("count", ctypes.c_int)]


TEXT_HOOK = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_void_p)

MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE = 1, 2, 4


def _find(build, *parts):
    p = os.path.join(build, *parts)
    return p if os.path.exists(p) else None


class LxaException(ctypes.Structure):
    _fields_ = [("vector", ctypes.c_int), ("pc", ctypes.c_uint32), ("task", ctypes.c_char * 64),
                ("count", ctypes.c_int)]


class Lxa:
    """One emulator session."""

    def __init__(self, build=None, rootless=False, strict_unimplemented=False,
                 apps=None, extra_assigns=None):
        self.build = build or os.environ.get("LXA_BUILD", os.path.join(ROOT, "build"))
        libpath = _find(self.build, "host", "lib", "liblxa.so")
        if not libpath:
            raise RuntimeError("liblxa.so not found in %s (build the liblxa_shared target)" % self.build)
        self.lib = ctypes.CDLL(libpath)
        self._prototypes()
        # same prefix the GTest drivers use (LXA_TEST_PREFIX in lxa_test.h)
        os.environ.setdefault("LXA_PREFIX", os.path.normpath(os.path.join(ROOT, "..", "..", "usr")))
        self._rom = os.path.join(self.build, "target", "rom", "lxa.rom").encode()
        cfg = LxaConfig(rom_path=self._rom, headless=True, rootless=rootless,
                        strict_unimplemented=strict_unimplemented)
        if self.lib.lxa_init(ctypes.byref(cfg)) != 0:
            raise RuntimeError("lxa_init failed")
        self._open = True
        # same standard assigns as the GTest fixtures (tests/drivers/lxa_test.h)
        samples = os.path.join(self.build, "target", "samples", "Samples")
        # LXA_SYSTEM_DIR: a private copy for runs of untrusted programs (mass
        # runs) - installers write into LIBS: and must not touch the repo
        system = os.environ.get("LXA_SYSTEM_DIR") or os.path.join(ROOT, "share", "lxa", "System")
        sysbuild = os.path.join(self.build, "target", "sys")
        self._tmp = tempfile.mkdtemp(prefix="pylxa-")
        # SYS: (the boot volume "System") is laid out like the reference's
        # Workbench 3.1 partition: a private root directory whose drawers
        # link to lxa's files (C, Libs, S, System, Tests ...) or are private
        # (Prefs/Env-Archive = ENVARC:), so nothing writes into the checked-in
        # share/lxa/System.  The samples, share/lxa/System and the built sys
        # tree follow as further SYS: paths, so SYS:SimpleGad and the names
        # of their files (System:C/List) resolve as before.
        root = os.path.join(self._tmp, "SYS")
        os.makedirs(root)
        links = {"C": os.path.join(sysbuild, "C"), "Libs": os.path.join(system, "Libs"),
                 "S": os.path.join(system, "S"), "System": os.path.join(sysbuild, "System"),
                 "Fonts": os.path.join(system, "Fonts"), "Tests": os.path.join(samples, "Tests")}
        for name in ("C", "Classes", "Devs", "Expansion", "Fonts", "L", "Libs", "Locale", "Prefs",
                     "Rexxc", "S", "Storage", "System", "T", "Tests", "Tools", "Utilities", "WBStartup"):
            target = links.get(name)
            if target and os.path.isdir(target):
                os.symlink(target, os.path.join(root, name))
            elif name != "Tests":
                os.makedirs(os.path.join(root, name))
        envarc = os.path.join(root, "Prefs", "Env-Archive")
        if os.path.isdir(os.path.join(system, "Prefs", "Env-Archive")):
            shutil.copytree(os.path.join(system, "Prefs", "Env-Archive"), envarc, symlinks=True)
        else:
            os.makedirs(envarc)
        self.assign("SYS", root)
        self.assign_add("SYS", samples)
        self.assign_add("SYS", system)
        if os.path.isdir(sysbuild):
            self.assign_add("SYS", sysbuild)
        self.assign("LIBS", os.path.join(system, "Libs"))
        disklibs = os.path.join(sysbuild, "Libs")   # lxa's disk libraries
        if os.path.isdir(disklibs):
            self.assign_add("LIBS", disklibs)
        self.assign("C", os.path.join(sysbuild, "C"))
        for name, rel in (("S", "S"), ("L", "L"), ("DEVS", "Devs"), ("ENVARC", "Prefs/Env-Archive")):
            self.assign(name, os.path.join(root, rel))
        apps = apps or os.environ.get("LXA_APPS") or os.path.normpath(os.path.join(ROOT, "..", "lxa-apps"))
        if os.path.isdir(apps):
            self.assign("APPS", apps)
        gadgets = os.path.join(system, "Libs", "gadgets")
        if os.path.isdir(gadgets):
            self.assign("GADGETS", gadgets)
        # RAM: is the volume "Ram Disk"; T: and ENV: are drawers in it (3.1)
        ram = os.path.join(self._tmp, "RAM")
        for d in ("T", "ENV"):
            os.makedirs(os.path.join(ram, d))
        self.drive("RAM", ram)
        self.assign("T", os.path.join(ram, "T"))
        self.assign("ENV", os.path.join(ram, "ENV"))
        # FONTS: always exists on AmigaOS (disk fonts; topaz is in ROM)
        self.assign("FONTS", os.path.join(root, "Fonts"))
        for name, path in (extra_assigns or {}).items():
            self.assign(name, path)
        self._text_cb = None

    # -- plumbing ------------------------------------------------------------
    def _prototypes(self):
        L = self.lib
        sig = {
            "lxa_init": (ctypes.c_int, [ctypes.POINTER(LxaConfig)]),
            "lxa_shutdown": (None, []),
            "lxa_load_program": (ctypes.c_int, [ctypes.c_char_p, ctypes.c_char_p]),
            "lxa_set_program_stack": (None, [ctypes.c_uint32]),
            "lxa_add_assign": (ctypes.c_bool, [ctypes.c_char_p, ctypes.c_char_p]),
            "lxa_add_assign_path": (ctypes.c_bool, [ctypes.c_char_p, ctypes.c_char_p]),
            "lxa_add_drive": (ctypes.c_bool, [ctypes.c_char_p, ctypes.c_char_p]),
            "lxa_get_exception_log": (ctypes.c_int, [ctypes.c_void_p, ctypes.c_int]),
            "lxa_program_exited": (ctypes.c_bool, []),
            "lxa_program_loaded": (ctypes.c_bool, []),
            "lxa_wait_program_exit": (ctypes.c_bool, [ctypes.c_int]),
            "lxa_trace_start": (ctypes.c_bool, [ctypes.c_char_p, ctypes.c_char_p]),
            "lxa_trace_stop": (None, []),
            "lxa_run_cycles": (ctypes.c_int, [ctypes.c_int]),
            "lxa_run_frames": (ctypes.c_int, [ctypes.c_int]),
            "lxa_is_running": (ctypes.c_bool, []),
            "lxa_get_exit_code": (ctypes.c_int, []),
            "lxa_wait_windows": (ctypes.c_bool, [ctypes.c_int, ctypes.c_int]),
            "lxa_wait_window_drawn": (ctypes.c_bool, [ctypes.c_int, ctypes.c_int]),
            "lxa_wait_exit": (ctypes.c_bool, [ctypes.c_int]),
            "lxa_run_until_idle": (ctypes.c_int, [ctypes.c_int, ctypes.c_int]),
            "lxa_get_window_count": (ctypes.c_int, []),
            "lxa_get_window_info": (ctypes.c_bool, [ctypes.c_int, ctypes.POINTER(LxaWindowInfo)]),
            "lxa_inject_mouse": (ctypes.c_bool, [ctypes.c_int] * 4),
            "lxa_inject_mouse_click": (ctypes.c_bool, [ctypes.c_int] * 3),
            "lxa_inject_keypress": (ctypes.c_bool, [ctypes.c_int, ctypes.c_int]),
            "lxa_inject_string": (ctypes.c_bool, [ctypes.c_char_p]),
            "lxa_inject_drag_begin": (ctypes.c_bool, [ctypes.c_int] * 3),
            "lxa_inject_drag_step": (ctypes.c_bool, [ctypes.c_int] * 2),
            "lxa_inject_drag_end": (ctypes.c_bool, [ctypes.c_int] * 2),
            "lxa_select_menu_path": (ctypes.c_bool, [ctypes.c_int, ctypes.c_char_p]),
            "lxa_get_output": (ctypes.c_int, [ctypes.c_char_p, ctypes.c_int]),
            "lxa_clear_output": (None, []),
            "lxa_dump_tree_json": (ctypes.c_int, [ctypes.c_char_p]),
            "lxa_capture_screen_indexed": (ctypes.c_bool, [ctypes.c_char_p]),
            "lxa_capture_window_indexed": (ctypes.c_bool, [ctypes.c_int, ctypes.c_char_p]),
            "lxa_text_log_start": (None, []),
            "lxa_text_log_stop": (None, []),
            "lxa_text_log_dump": (ctypes.c_int, [ctypes.c_char_p]),
            "lxa_get_unimplemented_log": (ctypes.c_int, [ctypes.POINTER(LxaUnimplemented), ctypes.c_int]),
            "lxa_get_emulated_cycles": (ctypes.c_uint64, []),
            "lxa_get_time_us": (ctypes.c_uint64, []),
        }
        for name, (res, args) in sig.items():
            fn = getattr(L, name)
            fn.restype = res
            fn.argtypes = args

    def close(self):
        if self._open:
            self.lib.lxa_shutdown()
            self._open = False
            shutil.rmtree(self._tmp, ignore_errors=True)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # -- environment -----------------------------------------------------------
    def assign(self, name, path):
        return self.lib.lxa_add_assign(name.encode(), path.encode())

    def assign_add(self, name, path):
        return self.lib.lxa_add_assign_path(name.encode(), path.encode())

    def install_prefs(self, path):
        """Copy a prefs file to ENV:Sys/ (applied by C:IPrefs when the next
        program is started)."""
        d = os.path.join(self._tmp, "RAM", "ENV", "Sys")
        os.makedirs(d, exist_ok=True)
        shutil.copy2(path, d)

    def trace_start(self, spec, path):
        """Relay trace (Phase 233): spec "graphics.library:-60,-66;dos.library:*"."""
        return self.lib.lxa_trace_start(spec.encode(), path.encode())

    def trace_stop(self):
        self.lib.lxa_trace_stop()

    def drive(self, name, path):
        return self.lib.lxa_add_drive(name.encode(), path.encode())

    # -- execution -------------------------------------------------------------
    def run(self, program, args="", stack=0):
        self.lib.lxa_set_program_stack(stack)
        if self.lib.lxa_load_program(program.encode(), args.encode()) != 0:
            raise RuntimeError("cannot load %s" % program)

    def frames(self, n):
        return self.lib.lxa_run_frames(n)

    def running(self):
        return self.lib.lxa_is_running()

    def exit_code(self):
        return self.lib.lxa_get_exit_code()

    def wait_windows(self, count=1, timeout_ms=10000):
        return self.lib.lxa_wait_windows(count, timeout_ms)

    def wait_window_title(self, substr, timeout_ms=10000):
        """Like lxaprobe WAIT_WINDOW: index of the first window whose title
        contains substr, or -1."""
        start = self.lib.lxa_get_time_us()
        polls = 0
        while self.lib.lxa_get_time_us() - start < timeout_ms * 1000:
            for i in range(self.lib.lxa_get_window_count()):
                info = LxaWindowInfo()
                if self.lib.lxa_get_window_info(i, ctypes.byref(info)) and substr.encode() in info.title:
                    return i
            # also search the Intuition tree, as lxaprobe's WAIT_WINDOW does
            # (e.g. windows liblxa does not track)
            if polls % 5 == 0 and self._tree_has_title(substr):
                return 0
            polls += 1
            if not self.running():
                break
            self.frames(2)
        return 0 if self._tree_has_title(substr) else -1

    def _tree_has_title(self, substr):
        import json
        import tempfile
        fd, p = tempfile.mkstemp(suffix=".json", prefix="pylxa-tree-")
        os.close(fd)
        try:
            self.dump_tree(p)
            with open(p, encoding="utf-8", errors="replace") as f:
                tree = json.load(f)
        except (RuntimeError, OSError, ValueError):
            return False
        finally:
            os.unlink(p)
        return any(substr in (w.get("title") or "") for s in tree.get("screens", []) for w in s.get("windows", [])
                   if w.get("title") is not None)

    def wait_idle(self, iterations=50, timeout_ms=10000):
        """Run until every task waits (like lxaprobe WAIT_IDLE), at most
        timeout_ms of emulated time; a program that draws for longer than
        one batch of `iterations` slices is waited for, not cut short."""
        start = self.lib.lxa_get_time_us()
        while True:
            n = self.lib.lxa_run_until_idle(iterations, 100000)
            if n < iterations or self.lib.lxa_get_time_us() - start >= timeout_ms * 1000:
                return

    def program_exited(self):
        """the launched program returned (its other tasks may still run)"""
        return self.lib.lxa_program_exited()

    def program_loaded(self):
        """the launched program was loaded and is about to start (where the
        reference agent's RUN returns)"""
        return self.lib.lxa_program_loaded()

    def wait_program_exit(self, timeout_ms=5000):
        return self.lib.lxa_wait_program_exit(timeout_ms)

    def wait_exit(self, timeout_ms=5000):
        return self.lib.lxa_wait_exit(timeout_ms)

    # -- input -------------------------------------------------------------
    def click(self, x, y, button=MOUSE_LEFT):
        return self.lib.lxa_inject_mouse_click(x, y, button)

    def key(self, rawkey, qualifier=0):
        return self.lib.lxa_inject_keypress(rawkey, qualifier)

    def type(self, text):
        return self.lib.lxa_inject_string(text.encode("latin-1"))

    def menu(self, path, window=0):
        return self.lib.lxa_select_menu_path(window, path.encode("latin-1"))

    def drag(self, points, button=MOUSE_RIGHT):
        (x0, y0), rest = points[0], points[1:]
        self.lib.lxa_inject_drag_begin(x0, y0, button)
        for x, y in rest[:-1]:
            self.lib.lxa_inject_drag_step(x, y)
        x1, y1 = rest[-1] if rest else (x0, y0)
        return self.lib.lxa_inject_drag_end(x1, y1)

    # -- observation ---------------------------------------------------------
    def output(self):
        buf = ctypes.create_string_buffer(1 << 20)
        n = self.lib.lxa_get_output(buf, len(buf))
        # binary-safe: programs may print NUL bytes (buf.value stops there)
        return buf.raw[:max(0, n)].decode("latin-1")

    def clear_output(self):
        self.lib.lxa_clear_output()

    def dump_tree(self, path):
        if self.lib.lxa_dump_tree_json(path.encode()) != 0:
            raise RuntimeError("lxa_dump_tree_json failed")

    def snap(self, path, window=None):
        ok = (self.lib.lxa_capture_screen_indexed(path.encode()) if window is None
              else self.lib.lxa_capture_window_indexed(window, path.encode()))
        if not ok:
            raise RuntimeError("indexed capture failed")

    def text_start(self):
        self.lib.lxa_text_log_start()

    def text_stop(self):
        self.lib.lxa_text_log_stop()

    def text_dump(self, path):
        return self.lib.lxa_text_log_dump(path.encode())

    def exceptions(self):
        """CPU exceptions raised by emulated tasks (Phase 232)"""
        n = self.lib.lxa_get_exception_log(None, 0)
        arr = (LxaException * max(n, 1))()
        n = self.lib.lxa_get_exception_log(arr, n)
        return [{"vector": e.vector, "pc": e.pc, "task": e.task.decode("latin-1"), "count": e.count}
                for e in arr[:n]]

    def unimplemented(self):
        n = self.lib.lxa_get_unimplemented_log(None, 0)
        arr = (LxaUnimplemented * max(n, 1))()
        n = self.lib.lxa_get_unimplemented_log(arr, n)
        return [{"lib": e.lib.decode(), "function": e.function.decode(), "detail": e.detail.decode(),
                 "first_task": e.first_task.decode(), "count": e.count} for e in arr[:n]]
