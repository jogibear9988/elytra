#!/usr/bin/env python3
"""Run one hidden public view batch per runtime; offscreen is Firefly's default test platform."""

import _support

import argparse
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
from test_renderer import ROOT, environment, stop


def batch(binary, root, reference, platform):
    config = root / "config"
    config.mkdir()
    fixture_env = environment(str(config))
    if not reference:
        # Fail if the plugin is unavailable; never silently fall back to the user's desktop.
        fixture_env["QT_QPA_PLATFORM"] = platform
        fixture_env["LIBGL_ALWAYS_SOFTWARE"] = "1"
    command = [str(binary.resolve()), str(root / "main.mjs")]
    if not reference:
        command.append("--force-cpu-painting")
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=fixture_env,
        start_new_session=os.name != "nt",
    )
    try:
        stdout, stderr = process.communicate(timeout=45)
        assert process.returncode == 0 and "PASS public WebContentsView:" in stdout, (
            command,
            process.returncode,
            stdout,
            stderr,
        )
        assert "VERIFICATION FAILED" not in stderr, stderr
        labels = {
            "offscreen": "Firefly offscreen",
            "cocoa": "Firefly Cocoa hidden",
            "xcb": "Firefly X11 hidden",
        }
        label = "Electron reference" if reference else labels[platform]
        print(label + ": " + stdout.strip(), flush=True)
    finally:
        stop(process)
        process.communicate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--electron", type=Path)
    parser.add_argument(
        "--firefly-platform",
        choices=["offscreen", "cocoa", "xcb"],
        default="offscreen",
        help="Explicit backend: Cocoa requires macOS; xcb requires Linux and DISPLAY; all windows stay hidden",
    )
    args = parser.parse_args()
    if args.firefly_platform == "cocoa" and (sys.platform != "darwin" or not args.firefly):
        parser.error("--firefly-platform cocoa requires --firefly on actual macOS")
    if args.firefly_platform == "xcb" and (
        not sys.platform.startswith("linux") or not args.firefly or not os.environ.get("DISPLAY")
    ):
        parser.error("--firefly-platform xcb requires --firefly on actual Linux with DISPLAY")
    if not args.firefly and not args.electron:
        parser.error("Provide --firefly and/or --electron")
    with tempfile.TemporaryDirectory(prefix="elytra-web-contents-view-") as directory:
        for label, binary in [("firefly", args.firefly), ("electron", args.electron)]:
            if binary is None:
                continue
            root = Path(directory) / label
            shutil.copytree(ROOT / "firefly/tests/web-contents-view", root)
            reference = label == "electron"
            if reference:
                for source in root.glob("*.mjs"):
                    source.write_text(
                        source.read_text().replace("from 'firefly'", "from 'electron'")
                    )
            # The maintained source is ESM. Both sandboxed preload hosts consume its CJS bundle.
            subprocess.run(
                [
                    "node",
                    str(ROOT / "tooling/bundle_preload.mjs"),
                    str(root / "preload.mjs"),
                    str(root / "preload.cjs"),
                ],
                check=True,
            )
            batch(binary, root, reference, args.firefly_platform)


if __name__ == "__main__":
    main()
