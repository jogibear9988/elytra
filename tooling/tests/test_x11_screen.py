#!/usr/bin/env python3
"""Compare real native X11 snapshots against independent RandR CLI, EWMH and Xlib reads."""

import _support

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

from _x11 import isolated_display, managed_window_manager
from test_renderer import environment
from _x11_screen import native_layout, compare_snapshot


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
            native = native_layout(env)
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
                compare_snapshot(value, native, scale)

    print(
        "PASS actual RandR output ID, X11 bounds/work area, Xlib cursor and Qt scale-1/2 DIP snapshots"
    )
    print(
        "Component proof only; public screen wiring, physical hotplug and mixed-DPI acceptance remain required"
    )


if __name__ == "__main__":
    main()
