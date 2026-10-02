#!/usr/bin/env python3
"""Install Picasso96 into a reference SYS: tree for the `rtg` profile.

  install_p96.py <unpacked-p96-disk> <SYS>

Copies the Picasso96 libraries, classes and monitor driver from an unpacked
Picasso96 install disk and creates DEVS:Monitors/uaegfx with the tooltype
BOARDTYPE=uaegfx (FS-UAE's built-in RTG board).

Picasso96 1.33 does not support FS-UAE's uaegfx board (FindCard is never
called); Picasso96 2.0 (rtg.library 40.3945, ships uaegfx.card) does.  The
default source is the Picasso96 2.0 install tree on the user's AmigaOS 3.9
media (LXA_REF_P96_DIR in build_refsys.sh).
"""

import os
import shutil
import struct
import sys


def find(root, name):
    for d, _, files in os.walk(root):
        for f in files:
            if f.lower() == name.lower():
                return os.path.join(d, f)
    return None


def copy_tree(src, dst):
    if not os.path.isdir(src):
        return
    for d, _, files in os.walk(src):
        rel = os.path.relpath(d, src)
        os.makedirs(os.path.join(dst, rel), exist_ok=True)
        for f in files:
            if f.endswith((".xdfmeta",)):
                continue
            shutil.copy2(os.path.join(d, f), os.path.join(dst, rel, f))


def set_tooltypes(info_bytes, tooltypes):
    """Replace the tooltype array of a WB 2.x+ DiskObject (.info) file."""
    data = bytearray(info_bytes)
    magic, version = struct.unpack(">HH", data[0:4])
    if magic != 0xE310:
        raise ValueError("not a DiskObject")
    # struct DiskObject: Gadget at 4 (44 bytes), do_Type @48, pad, DefaultTool
    # @50, ToolTypes @54, CurrentX/Y @58/62, DrawerData @66, ToolWindow @70,
    # StackSize @74; header = 78 bytes.
    gadget_render, select_render = struct.unpack(">II", data[4 + 18:4 + 26])
    flags = struct.unpack(">H", data[4 + 12:4 + 14])[0]
    has_def_tool, has_tt, has_drawer = (struct.unpack(">I", data[50:54])[0],
                                         struct.unpack(">I", data[54:58])[0],
                                         struct.unpack(">I", data[66:70])[0])
    pos = 78
    if has_drawer:
        pos += 56

    def skip_image(p):
        w, h, depth = struct.unpack(">hhh", data[p + 4:p + 10])
        has_data = struct.unpack(">I", data[p + 10:p + 14])[0]
        p += 20
        if has_data:
            p += ((w + 15) // 16) * 2 * h * depth
        return p

    if gadget_render:
        pos = skip_image(pos)
    if select_render and (flags & 0x0002):   # GFLG_GADGHIMAGE
        pos = skip_image(pos)
    if has_def_tool:
        n = struct.unpack(">I", data[pos:pos + 4])[0]
        pos += 4 + n
    tt_start = pos
    if has_tt:
        cnt = struct.unpack(">I", data[pos:pos + 4])[0] // 4 - 1
        pos += 4
        for _ in range(cnt):
            n = struct.unpack(">I", data[pos:pos + 4])[0]
            pos += 4 + n
    new = struct.pack(">I", (len(tooltypes) + 1) * 4)
    for t in tooltypes:
        b = t.encode("latin-1") + b"\0"
        new += struct.pack(">I", len(b)) + b
    struct.pack_into(">I", data, 54, 1)      # ToolTypes present
    return bytes(data[:tt_start]) + new + bytes(data[pos:])


# Workbench mode of the `rtg` profile: UAE 800x600, 8 bit (uaegfx mode list
# captured with `lxaprobe MODES` on this configuration, Phase 211).
RTG_WB_MODE = (0x50041000, 800, 600, 8)


def write_screenmode_prefs(sysdir, mode):
    """ENVARC:Sys/screenmode.prefs: IFF FORM PREF with PRHD + SCRM."""
    display_id, width, height, depth = mode
    prhd = struct.pack(">BBI", 0, 0, 0)
    scrm = struct.pack(">4IIHHHH", 0, 0, 0, 0, display_id, width, height, depth, 1)  # SMF_AUTOSCROLL
    body = b"PREF" + b"PRHD" + struct.pack(">I", len(prhd)) + prhd + b"\0" * (len(prhd) & 1) \
        + b"SCRM" + struct.pack(">I", len(scrm)) + scrm
    data = b"FORM" + struct.pack(">I", len(body)) + body
    d = os.path.join(sysdir, "Prefs", "Env-Archive", "Sys")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "screenmode.prefs"), "wb") as f:
        f.write(data)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    disk, sysdir = sys.argv[1], sys.argv[2]
    if os.path.isdir(os.path.join(disk, "Libs")):
        vol = disk                      # already an install-tree layout
    else:
        vols = [os.path.join(disk, d) for d in os.listdir(disk) if os.path.isdir(os.path.join(disk, d))]
        if not vols:
            print("install_p96: no unpacked volume in", disk)
            return 1
        vol = vols[0]
    copy_tree(os.path.join(vol, "Libs"), os.path.join(sysdir, "Libs"))
    copy_tree(os.path.join(vol, "Classes"), os.path.join(sysdir, "Classes"))
    copy_tree(os.path.join(vol, "Devs"), os.path.join(sysdir, "Devs"))
    for tool in ("PicassoModeTNG",):
        p = os.path.join(vol, tool)
        if os.path.exists(p):
            os.makedirs(os.path.join(sysdir, "Prefs"), exist_ok=True)
            shutil.copy2(p, os.path.join(sysdir, "Prefs", tool))

    mon_dir = os.path.join(sysdir, "Devs", "Monitors")
    os.makedirs(mon_dir, exist_ok=True)
    # the generic Picasso96 monitor driver becomes DEVS:Monitors/uaegfx
    driver = os.path.join(vol, "Devs", "Monitors", "Picasso96")
    if not os.path.isfile(driver):
        driver = find(os.path.join(vol, "Devs"), "Picasso96")
    for stale in ("Picasso96", "Picasso96.info"):
        p = os.path.join(mon_dir, stale)
        if os.path.exists(p):
            os.remove(p)
    # the install tree ships settings variants (Picasso96Settings.NN); the
    # uaegfx board builds its default mode list without one
    for f in os.listdir(os.path.join(sysdir, "Devs")):
        if f.startswith("Picasso96Settings."):
            os.remove(os.path.join(sysdir, "Devs", f))
    if driver and os.path.isfile(driver):
        shutil.copy2(driver, os.path.join(mon_dir, "uaegfx"))
    template = os.path.join(vol, "Devs", "Monitors", "Picasso96.info")
    if not os.path.exists(template):
        template = os.path.join(mon_dir, "PAL.info")
    if os.path.exists(template):
        icon = set_tooltypes(open(template, "rb").read(), ["BOARDTYPE=uaegfx"])
        open(os.path.join(mon_dir, "uaegfx.info"), "wb").write(icon)
    write_screenmode_prefs(sysdir, RTG_WB_MODE)
    print("install_p96: Picasso96 installed (driver %s), Workbench mode 0x%08x" % (
        "found" if driver else "MISSING", RTG_WB_MODE[0]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
