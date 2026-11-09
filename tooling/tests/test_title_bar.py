#!/usr/bin/env python3
"""Hidden native title-bar geometry and optional Cocoa controls, without OS input injection."""

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
        assert process.returncode == 0 and "VERIFICATION FAILED" not in stderr, (
            process.returncode,
            stdout,
            stderr,
        )
        return stdout, stderr
    finally:
        stop(process)
        process.communicate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--electron", type=Path)
    parser.add_argument("--platform", choices=["cocoa", "offscreen"], default="offscreen")
    parser.add_argument("--probe", type=Path)
    parser.add_argument(
        "--runtime-probe", type=Path, help="One brief visible Cocoa renderer/controls batch"
    )
    args = parser.parse_args()
    if not args.firefly and not args.electron and not args.probe and not args.runtime_probe:
        parser.error("Require --firefly, --electron, --probe and/or --runtime-probe")
    if (args.platform == "cocoa" or args.probe or args.runtime_probe) and sys.platform != "darwin":
        parser.error("Cocoa acceptance requires macOS; no simulated platform fallback")
    with tempfile.TemporaryDirectory(prefix="elytra-title-bar-") as temporary:
        root = Path(temporary)
        (root / "config").mkdir()
        if args.probe:
            env = environment(str(root / "config"))
            env["QT_QPA_PLATFORM"] = "cocoa"
            stdout, stderr = run([str(args.probe.resolve())], env)
            assert all(
                marker in stdout
                for marker in [
                    "PASS native title bar hidden titled full-size transparent window preserves native controls",
                    "PASS native title bar actual full content geometry with default window unchanged",
                    "PASS native title bar drag and no-drag region policy without OS input",
                    "PASS native title bar actual NSWindow recreation restores old caption and rebinds hidden policy",
                ]
            ), (stdout, stderr)
            assert (
                "PASS native title bar native traffic-light hit targets remain above full content"
                in stdout
                or "LIMIT native title bar traffic-light hit testing unavailable for " in stdout
            ), (stdout, stderr)
            print("Cocoa native title bar: " + stdout.strip(), flush=True)
        for label, binary in [("Firefly", args.firefly), ("Electron", args.electron)]:
            if binary is None:
                continue
            directory = root / label
            directory.mkdir()
            (directory / "config").mkdir()
            source = (ROOT / "firefly/tests/title-bar/public.mjs").read_text()
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
                "PASS public title bar omitted and explicit default preserve native outer content geometry"
            ]
            markers.append(
                "PASS public title bar hidden full content geometry and real constrained resizing"
                if label == "Electron" or args.platform == "cocoa"
                else "PASS Firefly title bar hidden explicitly unsupported offscreen before native admission"
            )
            if label == "Firefly":
                markers.append(
                    "PASS Firefly title bar strict option types and unknown styles reject before native admission"
                )
            assert all(marker in stdout for marker in markers), (stdout, stderr)
            print(label + " " + args.platform + " hidden: " + stdout.strip(), flush=True)
        if args.runtime_probe:
            directory = root / "runtime"
            directory.mkdir()
            (directory / "runtime-config").mkdir()
            for name in ["runtime.mjs", "page.html"]:
                (directory / name).write_bytes(
                    (ROOT / "firefly/tests/title-bar" / name).read_bytes()
                )
            env = environment(str(directory / "runtime-config"))
            env["QT_QPA_PLATFORM"] = "cocoa"
            env["LIBGL_ALWAYS_SOFTWARE"] = "1"
            stdout, stderr = run(
                [
                    str(args.runtime_probe.resolve()),
                    str(directory / "runtime.mjs"),
                    "--force-cpu-painting",
                ],
                env,
            )
            assert (
                "PASS public title bar actual renderer toolbar and native controls runtime"
                in stdout
            ), (stdout, stderr)
            assert (
                "PASS native title bar runtime real toolbar pixels full viewport native button hit targets and HTML drag regions"
                in stdout
            ), (stdout, stderr)
            print("Cocoa actual renderer: " + stdout.strip(), flush=True)


if __name__ == "__main__":
    main()
