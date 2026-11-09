#!/usr/bin/env python3
"""Hidden window option acceptance and optional real Cocoa first-mouse callbacks; no input injection."""

import _support

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from test_renderer import ROOT, environment, stop


def run(command, env):
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=env,
        start_new_session=os.name != "nt",
    )
    try:
        stdout, stderr = process.communicate(timeout=22)
        assert process.returncode == 0, (process.returncode, stdout, stderr)
        assert "VERIFICATION FAILED" not in stderr, stderr
        return stdout, stderr
    finally:
        stop(process)
        process.communicate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--electron", type=Path, help="Optional hidden pinned Electron reference")
    parser.add_argument("--platform", choices=["cocoa", "offscreen"], default="offscreen")
    parser.add_argument(
        "--probe", type=Path, help="Standalone genuine Cocoa/QWidget callback probe"
    )
    args = parser.parse_args()
    if not args.firefly and not args.probe and not args.electron:
        parser.error("Require --firefly, --electron and/or --probe")
    if (args.platform == "cocoa" or args.probe) and sys.platform != "darwin":
        parser.error("Cocoa acceptance requires macOS; no simulated platform fallback")
    with tempfile.TemporaryDirectory(prefix="elytra-window-input-") as temporary:
        root = Path(temporary)
        (root / "config").mkdir()
        if args.probe:
            env = environment(str(root / "config"))
            env["QT_QPA_PLATFORM"] = "cocoa"
            stdout, stderr = run([str(args.probe.resolve())], env)
            assert all(
                marker in stdout
                for marker in [
                    "PASS native first mouse hidden Cocoa callbacks and per-view isolation",
                    "PASS native first mouse unowned Qt class and callback preserved",
                    "PASS native first mouse reinstall and teardown restore native class",
                    "PASS native first mouse actual NSView recreation automatically preserves false and true policies",
                    "PASS native first mouse preserves reported class and real AppKit Qt safe-area getters",
                ]
            ), (stdout, stderr)
            print("Cocoa native callbacks: " + stdout.strip(), flush=True)
        for label, binary in [("Firefly", args.firefly), ("Electron", args.electron)]:
            if binary is None:
                continue
            directory = root / label
            directory.mkdir()
            (directory / "config").mkdir()
            source = (ROOT / "firefly/tests/window-input/public.mjs").read_text()
            if label == "Electron":
                source = source.replace("from 'firefly'", "from 'electron'")
            entry = directory / "public.mjs"
            entry.write_text(source)
            env = environment(str(directory / "config"))
            env["QT_QPA_PLATFORM"] = args.platform
            env["LIBGL_ALWAYS_SOFTWARE"] = "1"
            command = [str(binary.resolve()), str(entry)]
            if label == "Firefly":
                command.append("--force-cpu-painting")
            stdout, stderr = run(command, env)
            markers = [
                "PASS public window input default hidden native window admission",
                "PASS public window input legacy experimentalDarkMode booleans admitted without appearance claim",
                "PASS public window input explicit first mouse "
                + (
                    "native admission"
                    if label == "Electron" or args.platform == "cocoa"
                    else "offscreen unsupported before admission"
                ),
                "PASS public window input child view attach move detach preserves owned native identity",
            ]
            if label == "Firefly":
                markers.append(
                    "PASS public window input invalid types and unknown option fail before native admission"
                )
            assert all(marker in stdout for marker in markers), (stdout, stderr)
            print(label + " " + args.platform + " hidden: " + stdout.strip(), flush=True)


if __name__ == "__main__":
    main()
