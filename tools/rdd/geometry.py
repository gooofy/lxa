"""Screen geometry from lxa-tree/1 dumps (Phase 213).

Both backends resolve "click gadget 3 of window X" through these functions
on their own tree dump, so a scenario clicks the same logical target even
when the two systems lay the window out differently.
"""

GFLG_RELBOTTOM = 0x0008
GFLG_RELRIGHT = 0x0010
GFLG_RELWIDTH = 0x0020
GFLG_RELHEIGHT = 0x0040
GTYP_SYSGADGET = 0x8000
GTYP_SYSTYPEMASK = 0x00F0
GTYP_CLOSE = 0x0080


def windows(tree):
    """[(screen_index, window_index_in_screen, window)] in tree order."""
    out = []
    for si, s in enumerate(tree.get("screens", [])):
        for wi, w in enumerate(s.get("windows", [])):
            out.append((si, wi, w))
    return out


def find_window(tree, title=None, app_only=True):
    """First (app) window whose title contains `title` (any app window if None)."""
    for si, wi, w in windows(tree):
        if app_only and not w.get("app", True):
            continue
        if title is None or (w.get("title") or "").find(title) >= 0:
            return si, wi, w
    return None


def gadget_box(win, g):
    """Absolute screen box (x, y, w, h) of a gadget, honouring GFLG_REL*."""
    left, top, width, height = g["left"], g["top"], g["width"], g["height"]
    flags = g["flags"]
    if flags & GFLG_RELRIGHT:
        left = win["width"] - 1 + left
    if flags & GFLG_RELBOTTOM:
        top = win["height"] - 1 + top
    if flags & GFLG_RELWIDTH:
        width = win["width"] + width
    if flags & GFLG_RELHEIGHT:
        height = win["height"] + height
    return win["left"] + left, win["top"] + top, width, height


def gadget_label(g):
    texts = [t.get("text") or "" for t in g.get("text", [])]
    return " ".join(t for t in texts if t)


def find_gadget(win, gadget_id=None, label=None):
    for g in win.get("gadgets", []):
        if g["type"] & GTYP_SYSGADGET:
            continue
        if gadget_id is not None and g["id"] == gadget_id:
            return g
        if label is not None and label.lower() in gadget_label(g).lower().replace("_", ""):
            return g
    return None


def center(box):
    x, y, w, h = box
    return x + w // 2, y + h // 2


def close_gadget_point(win):
    for g in win.get("gadgets", []):
        if (g["type"] & GTYP_SYSGADGET) and (g["type"] & GTYP_SYSTYPEMASK) == GTYP_CLOSE:
            return center(gadget_box(win, g))
    return None


def click_point(tree, spec):
    """Resolve a scenario click spec to screen coordinates.

    spec: {"xy": [x, y]} | {"gadget_id": n} | {"label": "text"},
          optional {"window": "title substring"} and {"offset": [dx, dy]}
          (offset relative to the window's top-left for "xy" when a window
          is given).
    """
    if "xy" in spec and "window" not in spec and "anchor" not in spec:
        return tuple(spec["xy"])
    found = find_window(tree, spec.get("window"))
    if not found:
        raise LookupError("no window %r" % spec.get("window"))
    _, _, win = found
    anchor = spec.get("anchor")
    if anchor:
        # window-relative anchors that resolve to the same logical spot on
        # both systems even if borders differ: "title" (drag bar), "inside"
        # (top-left of the inner area), "size_gadget" (bottom-right corner),
        # "center", "menubar" (screen title bar above the window)
        b = win.get("border") or [4, 11, 4, 2]
        ax, ay = {"title": (win["left"] + 20, win["top"] + max(1, b[1] // 2)),
                  "inside": (win["left"] + b[0], win["top"] + b[1]),
                  "size_gadget": (win["left"] + win["width"] - 4, win["top"] + win["height"] - 4),
                  "center": (win["left"] + win["width"] // 2, win["top"] + win["height"] // 2),
                  "menubar": (win["left"] + 30, 4)}[anchor]
        dx, dy = spec.get("xy", (0, 0))
        return ax + dx, ay + dy
    if "xy" in spec:
        return win["left"] + spec["xy"][0], win["top"] + spec["xy"][1]
    g = find_gadget(win, spec.get("gadget_id"), spec.get("label"))
    if not g:
        raise LookupError("no gadget %r in window %r" % (spec, win.get("title")))
    return center(gadget_box(win, g))


def menu_title_points(tree, title=None):
    """[(menu_title, x, y)] screen points of the menu titles of a window's
    MenuStrip (the first app window, or the one whose title contains
    `title`): centre of the title box in the screen's title bar."""
    found = find_window(tree, title)
    if not found:
        return []
    si, _, win = found
    scr = tree["screens"][si]
    y = max(1, (scr.get("bar_height") or 10) // 2)
    pts = []
    for m in win.get("menus", []):
        x = scr.get("left", 0) + m["left"] + max(1, m["width"] // 2)
        pts.append((m.get("title") or "", x, y))
    return pts
