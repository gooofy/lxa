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
        system = os.path.join(ROOT, "share", "lxa", "System")
        self.assign("SYS", samples)
        self.assign_add("SYS", system)
        self.assign("LIBS", os.path.join(system, "Libs"))
        disklibs = os.path.join(self.build, "target", "sys", "Libs")   # lxa's disk libraries
        if os.path.isdir(disklibs):
            self.assign_add("LIBS", disklibs)
        self.assign("C", os.path.join(self.build, "target", "sys", "C"))
        apps = apps or os.path.normpath(os.path.join(ROOT, "..", "lxa-apps"))
        if os.path.isdir(apps):
            self.assign("APPS", apps)
        gadgets = os.path.join(system, "Libs", "gadgets")
        if os.path.isdir(gadgets):
            self.assign("GADGETS", gadgets)
        sysbin = os.path.join(self.build, "target", "sys", "System")
        if os.path.isdir(sysbin):
            self.assign("System", sysbin)
        # RAM:, T:, ENV:/ENVARC: on fresh temp dirs, as in the GTest fixture
        self._tmp = tempfile.mkdtemp(prefix="pylxa-")
        for d in ("RAM", "T", "ENV"):
            os.makedirs(os.path.join(self._tmp, d))
        self.drive("RAM", os.path.join(self._tmp, "RAM"))
        self.assign("T", os.path.join(self._tmp, "T"))
        self.assign("ENV", os.path.join(self._tmp, "ENV"))
        self.assign("ENVARC", os.path.join(self._tmp, "ENV"))
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
            "lxa_add_assign": (ctypes.c_bool, [ctypes.c_char_p, ctypes.c_char_p]),
            "lxa_add_assign_path": (ctypes.c_bool, [ctypes.c_char_p, ctypes.c_char_p]),
            "lxa_add_drive": (ctypes.c_bool, [ctypes.c_char_p, ctypes.c_char_p]),
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

    def drive(self, name, path):
        return self.lib.lxa_add_drive(name.encode(), path.encode())

    # -- execution -------------------------------------------------------------
    def run(self, program, args=""):
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
        while self.lib.lxa_get_time_us() - start < timeout_ms * 1000:
            for i in range(self.lib.lxa_get_window_count()):
                info = LxaWindowInfo()
                if self.lib.lxa_get_window_info(i, ctypes.byref(info)) and substr.encode() in info.title:
                    return i
            if not self.running():
                break
            self.frames(2)
        return -1

    def wait_idle(self, iterations=50):
        self.lib.lxa_run_until_idle(iterations, 100000)

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
        self.lib.lxa_get_output(buf, len(buf))
        return buf.value.decode("latin-1")

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

    def unimplemented(self):
        n = self.lib.lxa_get_unimplemented_log(None, 0)
        arr = (LxaUnimplemented * max(n, 1))()
        n = self.lib.lxa_get_unimplemented_log(arr, n)
        return [{"lib": e.lib.decode(), "function": e.function.decode(), "detail": e.detail.decode(),
                 "first_task": e.first_task.decode(), "count": e.count} for e in arr[:n]]
