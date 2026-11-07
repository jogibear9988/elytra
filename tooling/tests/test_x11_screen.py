#!/usr/bin/env python3
"""Compare real native X11 snapshots against independent RandR CLI, EWMH and Xlib reads."""

import _support

import argparse
import ctypes
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from _x11 import isolated_display, managed_window_manager, query
from test_renderer import environment


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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--check-guard", action="store_true")
    args = parser.parse_args()
    probe = args.probe.resolve(strict=True)
    if args.check_guard:
        result = subprocess.run([str(probe), "--no-gui"], timeout=10)
        assert result.returncode == 0, result
        print("PASS actual provider refuses display/cursor reads without a GUI application")
        return
    isolated_display(os.environ)
    with tempfile.TemporaryDirectory(prefix="firefly-x11-screen-") as directory:
        root = Path(directory)
        runtime = root / "runtime"
        runtime.mkdir(mode=0o700)
        env = {**environment(directory), "XDG_RUNTIME_DIR": str(runtime)}
        with managed_window_manager(env, root / "window-manager.log"):
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
            for scale in (1, 2):
                result = subprocess.run(
                    [str(probe)],
                    env={**env, "QT_SCALE_FACTOR": str(scale), "QT_SCREEN_SCALE_FACTORS": ""},
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
                assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
                value = json.loads(result.stdout)
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
    print(
        "PASS actual RandR output ID, X11 bounds/work area, Xlib cursor and Qt scale-1/2 DIP snapshots"
    )
    print(
        "Component proof only; public screen wiring, physical hotplug and mixed-DPI acceptance remain required"
    )


if __name__ == "__main__":
    main()
