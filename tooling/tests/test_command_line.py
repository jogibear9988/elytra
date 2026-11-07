#!/usr/bin/env python3
"""Accept real native switches and one batched hidden renderer's language/CPU policy."""

import _support

import argparse
import json
import os
from pathlib import Path
import selectors
import subprocess
import tempfile
import time
from test_renderer import ROOT, environment, run, stop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    parser.add_argument("--electron", type=Path)
    args = parser.parse_args()
    flags = [
        "--elytra-one=first",
        "position",
        "--elytra-one=last=value",
        "-elytra-flag",
        "--elytra-empty=",
        "--",
        "--elytra-after=ignored",
    ]
    observe = ROOT / "firefly/tests/command-line/observe.mjs"
    invalid = run(args.firefly.resolve(), observe, "--force-cpu-painting=invalid")
    assert invalid.returncode == 1 and "ERR_INVALID_ARG_VALUE" in invalid.stdout + invalid.stderr, (
        invalid
    )

    actual = run(args.firefly.resolve(), "--electron-compat", observe, *flags)
    assert actual.returncode == 0, actual
    observed = json.loads(actual.stdout.strip())
    expected = [
        ["elytra-one", True, "last=value"],
        ["elytra-flag", True, ""],
        ["elytra-empty", True, ""],
        ["elytra-after", False, ""],
        ["elytra-missing", False, ""],
    ]
    assert observed == expected, actual
    if args.electron:
        reference = run(args.electron.resolve(), observe, *flags)
        assert reference.returncode == 0, reference
        assert json.loads(reference.stdout.strip()) == observed, (actual, reference)
        print("PASS same native switch observations as actual Electron")
    with tempfile.TemporaryDirectory(prefix="firefly-command-line-") as directory:
        root = Path(directory)
        (root / "index.html").write_text(
            '<!doctype html><meta charset="utf-8"><title>PENDING</title><script>document.title = "LANGUAGES:" + JSON.stringify(navigator.languages)</script>'
        )
        command = [
            str(args.firefly.resolve()),
            "--electron-compat",
            str(ROOT / "firefly/tests/command-line/main.mjs"),
            directory,
            "--renderer",
            "--force-cpu-painting",
            *flags,
        ]
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=environment(str(root / "profile")),
            start_new_session=True,
        )
        prefix = ""
        try:
            helper = None
            deadline = time.monotonic() + 25
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ)
                while helper is None:
                    assert time.monotonic() < deadline, (
                        "No helper acceptance marker",
                        prefix,
                        process.poll(),
                    )
                    if not selector.select(timeout=1):
                        continue
                    chunk = os.read(process.stdout.fileno(), 8192)
                    assert chunk, (process.poll(), prefix, process.stderr.read())
                    prefix += chunk.decode("utf-8")
                    for line in prefix.splitlines():
                        if line.startswith('{"helper":'):
                            helper = json.loads(line)["helper"]
            assert helper and helper != process.pid, helper
            parent = int(
                subprocess.check_output(["ps", "-o", "ppid=", "-p", str(helper)], text=True).strip()
            )
            assert parent == process.pid, (helper, parent, process.pid)
            native_args = subprocess.check_output(
                ["ps", "-ww", "-o", "command=", "-p", str(helper)], text=True
            )
            assert "--disable-sandbox" not in native_args, native_args
            rows = subprocess.check_output(
                ["ps", "-axo", "pid=,ppid=,comm="], text=True
            ).splitlines()
            compositors = [
                int(parts[0])
                for row in rows
                if len(parts := row.strip().split(None, 2)) == 3
                and int(parts[1]) == process.pid
                and Path(parts[2]).name == "Compositor"
            ]
            assert len(compositors) == 1, compositors
            compositor_args = subprocess.check_output(
                ["ps", "-ww", "-o", "command=", "-p", str(compositors[0])], text=True
            )
            assert (
                "--force-cpu-painting" in compositor_args
                and "--disable-sandbox" not in compositor_args
            ), compositor_args
            stdout, stderr = process.communicate(timeout=15)
            assert process.returncode == 0, (prefix + stdout, stderr)
            assert "PASS actual public command-line" in prefix + stdout, (prefix + stdout, stderr)
            assert "VERIFICATION FAILED" not in stderr, stderr
            print((prefix + stdout).strip())
            print(
                "PASS actual LibWeb navigator.languages and owned compositor CPU-painting argv with sandbox retained"
            )
        finally:
            stop(process)
            process.communicate()


if __name__ == "__main__":
    main()
