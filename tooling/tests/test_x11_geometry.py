#!/usr/bin/env python3
"""Compare owned window geometry with independent X11 queries on a dedicated Xvfb display."""

import _support

import argparse
import json
import os
from pathlib import Path
import re
import selectors
import subprocess
import sys
import tempfile
import time

from test_renderer import environment, stop


def query(*args):
    return subprocess.check_output(args, text=True, timeout=5, env={**os.environ, "LC_ALL": "C"})


def property_values(window, name):
    """Read native window-manager properties without calling the owned Qt geometry adapter."""
    output = query("xprop", "-id", str(window), name)
    if " = " not in output:
        return []
    return [int(value, 0) for value in output.split(" = ", 1)[1].strip().split(", ")]


def root_property(name):
    output = query("xprop", "-root", name)
    if "window id #" in output:
        return output.split("window id #", 1)[1].strip()
    return output.split(" = ", 1)[1].strip() if " = " in output else ""


def native_bounds(window):
    output = query("xwininfo", "-id", str(window))
    values = []
    for label in ["Absolute upper-left X", "Absolute upper-left Y", "Width", "Height"]:
        match = re.search(rf"^\s*{label}:\s*(-?\d+)\s*$", output, re.MULTILINE)
        assert match, output
        values.append(int(match[1]))
    margins = property_values(window, "_NET_FRAME_EXTENTS")
    assert len(margins) == 4, (window, margins)
    left, right, top, bottom = margins
    x, y, width, height = values
    return {
        "content": values,
        "outer": [x - left, y - top, width + left + right, height + top + bottom],
    }


def wait_for(predicate, message):
    deadline = time.monotonic() + 10
    last = None
    while time.monotonic() < deadline:
        last = predicate()
        if last:
            return last
        time.sleep(0.03)
    raise AssertionError((message, last))


def exercise(probe, mode, env):
    observations = {}
    process = subprocess.Popen(
        [str(probe), mode],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
        start_new_session=True,
    )
    try:

        def receive():
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ)
                assert selector.select(10), (mode, "native probe response timeout", process.poll())
                line = process.stdout.readline()
            assert line, (mode, "native probe exited", process.poll())
            return json.loads(line)

        def request(**value):
            process.stdin.write(json.dumps(value).encode() + b"\n")
            process.stdin.flush()
            return receive()

        window = int(receive()["window"])
        wait_for(lambda: len(property_values(window, "_NET_FRAME_EXTENTS")) == 4, "WM decorations")
        margins = property_values(window, "_NET_FRAME_EXTENTS")
        assert any(margins) if mode == "framed" else not any(margins), (mode, margins)

        def settled(expected=None):
            observed = native_bounds(window)
            owned = request(op="get")
            observations.update(native=observed, owned=owned)
            return (
                observed
                if owned == observed and (expected is None or observed == expected)
                else None
            )

        wait_for(settled, "initial native/owned geometry agreement")
        for content, bounds in [(False, [80, 90, 640, 420]), (True, [170, 180, 500, 340])]:
            assert request(op="set", bounds=bounds, content=content) == {"ok": True}
            key = "content" if content else "outer"

            def matches():
                value = settled()
                return value if value and value[key] == bounds else None

            wait_for(matches, (mode, key, bounds))

        before = wait_for(settled, "before size-only update")
        assert request(op="set", bounds=[900, 650, 560, 360], content=False, position=False) == {
            "ok": True
        }
        wait_for(
            lambda: (value := settled()) and value["outer"] == before["outer"][:2] + [560, 360],
            "size-only update must preserve frame origin",
        )
        before = wait_for(settled, "before rejected content update")
        assert request(op="set", bounds=[0, 0, 0, 0], content=True) == {"ok": False}
        assert settled(before), "rejected content size mutated the native window"

        desktop = int(root_property("_NET_CURRENT_DESKTOP"))
        areas = [int(value) for value in root_property("_NET_WORKAREA").split(", ")]
        x, y, width, height = areas[desktop * 4 : desktop * 4 + 4]
        size = before["outer"][2:]
        centered = [x + (width - size[0]) // 2, y + (height - size[1]) // 2, *size]
        assert request(op="center") == {"ok": True}
        wait_for(
            lambda: (value := settled()) and value["outer"] == centered,
            "native work-area centering",
        )
        assert process.poll() is None
    finally:
        if sys.exc_info()[0] is not None:
            print(f"Last {mode} native/owned geometry: {observations}", file=sys.stderr)
        stop(process)
        stdout, stderr = process.communicate(timeout=5)
        assert not stdout and not stderr, (mode, stdout, stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--check-guard", action="store_true")
    args = parser.parse_args()
    probe = args.probe.resolve()
    if not probe.is_file():
        parser.error("Build firefly-x11-geometry-probe first")
    if args.check_guard:
        env = {
            key: value for key, value in os.environ.items() if key != "FIREFLY_TEST_ISOLATED_X11"
        }
        for case in [
            {**env, "QT_QPA_PLATFORM": "offscreen"},
            {**env, "FIREFLY_TEST_ISOLATED_X11": "1", "QT_QPA_PLATFORM": "offscreen"},
            {**env, "FIREFLY_TEST_ISOLATED_X11": "1", "QT_QPA_PLATFORM": "xcb", "DISPLAY": ""},
        ]:
            result = subprocess.run(
                [str(probe), "framed"],
                env=case,
                capture_output=True,
                text=True,
                timeout=5,
            )
            assert result.returncode == 77 and result.stdout == "", result
            assert (
                result.stderr
                == "Refusing geometry probe without an explicitly isolated X11 server\n"
            ), result
        print("PASS native geometry isolation guard before GUI initialization")
        return
    if (
        sys.platform != "linux"
        or not os.environ.get("DISPLAY")
        or os.environ.get("QT_QPA_PLATFORM") != "xcb"
        or os.environ.get("FIREFLY_TEST_ISOLATED_X11") != "1"
    ):
        parser.error("Use the dedicated Xvfb CI display and explicit isolated-X11 environment")
    assert not root_property("_NET_SUPPORTING_WM_CHECK"), "Use a fresh dedicated X server"
    with tempfile.TemporaryDirectory(prefix="firefly-x11-geometry-") as directory:
        runtime = Path(directory) / "runtime"
        runtime.mkdir(mode=0o700)
        # The X11 peer measures physical pixels; this gate deliberately uses 1x DIP.
        env = {
            **environment(directory),
            "XDG_RUNTIME_DIR": str(runtime),
            "QT_SCALE_FACTOR": "1",
            "QT_SCREEN_SCALE_FACTORS": "1",
        }
        with (Path(directory) / "openbox.log").open("wb") as log:
            manager = subprocess.Popen(
                ["openbox", "--sm-disable"],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
            try:
                wait_for(
                    lambda: root_property("_NET_SUPPORTING_WM_CHECK"), "dedicated Openbox readiness"
                )
                for mode in ["framed", "frameless"]:
                    exercise(probe, mode, env)
                assert manager.poll() is None, "window manager exited during acceptance"
            finally:
                stop(manager)
                manager.wait(timeout=5)
                if sys.exc_info()[0] is not None:
                    log.flush()
                    print((Path(directory) / "openbox.log").read_text(), file=sys.stderr)
        print(
            "PASS actual X11 frame/content bounds, size-only updates, refusal and work-area centering"
        )


if __name__ == "__main__":
    main()
