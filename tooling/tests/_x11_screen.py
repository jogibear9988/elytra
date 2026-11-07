"""Independent Xlib/RandR/EWMH observations for native provider and actual app acceptance."""

import ctypes
import re

from _x11 import query


def cursor():
    """Use Xlib directly, independently of the owned Qt/XCB implementation; never move it."""
    xlib = ctypes.CDLL("libX11.so.6")
    window = ctypes.c_ulong
    integer = ctypes.c_int
    xlib.XOpenDisplay.argtypes = [ctypes.c_char_p]
    xlib.XOpenDisplay.restype = ctypes.c_void_p
    xlib.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
    xlib.XDefaultRootWindow.restype = window
    xlib.XQueryPointer.argtypes = [
        ctypes.c_void_p,
        window,
        ctypes.POINTER(window),
        ctypes.POINTER(window),
        *[ctypes.POINTER(integer)] * 4,
        ctypes.POINTER(ctypes.c_uint),
    ]
    xlib.XQueryPointer.restype = integer
    xlib.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = xlib.XOpenDisplay(None)
    assert display, "Cannot independently query native X11 pointer"
    try:
        root, child = window(), window()
        x, y, wx, wy = integer(), integer(), integer(), integer()
        mask = ctypes.c_uint()
        assert xlib.XQueryPointer(
            display,
            xlib.XDefaultRootWindow(display),
            ctypes.byref(root),
            ctypes.byref(child),
            ctypes.byref(x),
            ctypes.byref(y),
            ctypes.byref(wx),
            ctypes.byref(wy),
            ctypes.byref(mask),
        )
        return x.value, y.value
    finally:
        xlib.XCloseDisplay(display)


def native_layout(env):
    native = query("xrandr", "--current", "--verbose", env=env)
    # This gate intentionally requires one native Xvfb output. Other
    # physical/mixed-DPI layouts need their own independent acceptance.
    sections = re.findall(r"(?m)^(\S+) connected (.*?)(?=^\S|\Z)", native, re.S)
    assert len(sections) == 1, native
    name, description = sections[0]
    bounds = re.search(r"\b(\d+)x(\d+)\+0\+0\b", description)
    identity = re.search(r"Identifier:\s*(0x[0-9a-fA-F]+)", description)
    assert bounds and identity, native
    width, height = map(int, bounds.groups())
    xid = int(identity[1], 16)
    work_text = query("xprop", "-root", "_NET_WORKAREA", env=env)
    work = [int(value.strip()) for value in work_text.split(" = ", 1)[1].split(",")][:4]
    assert len(work) == 4, work_text
    pointer = cursor()
    return {
        "id": xid,
        "name": name,
        "width": width,
        "height": height,
        "work": work,
        "pointer": pointer,
    }


def compare_snapshot(value, native, scale):
    xid, name = native["id"], native["name"]
    width, height = native["width"], native["height"]
    work, pointer = native["work"], native["pointer"]
    snapshot = value["screens"]
    assert snapshot["primaryId"] == xid and len(snapshot["displays"]) == 1, value
    display = snapshot["displays"][0]
    assert display["id"] == xid and display["label"] == name, value
    assert display["bounds"] == {
        "x": 0,
        "y": 0,
        "width": width // scale,
        "height": height // scale,
    }, value
    assert display["size"] == {"width": width // scale, "height": height // scale}
    assert display["workArea"] == dict(
        zip(("x", "y", "width", "height"), (coordinate // scale for coordinate in work))
    ), (value, work)
    assert display["scaleFactor"] == scale and display["rotation"] == 0, value
    assert display["detected"] is True and "internal" not in display, value
    # Qt rounds native cursor coordinates into DIP, with <= half a
    # native pixel at this scale; do not warp the pointer to simplify it.
    assert abs(value["cursor"]["x"] * scale - pointer[0]) <= scale / 2, value
    assert abs(value["cursor"]["y"] * scale - pointer[1]) <= scale / 2, value
