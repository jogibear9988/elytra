#!/usr/bin/env python3
"""Compare native request/reply IPC and sender ownership with real Electron."""

import _support  # Standalone test CLI import paths.

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
from test_firefly import execute, success, crash_probe

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "firefly/tests/ipc"


def stage(directory, module):
    """Stage readable ESM fixtures, bundling only the sandbox preload payloads."""
    shutil.copytree(FIXTURES, directory, dirs_exist_ok=True)
    for file in directory.glob("*.mjs"):
        file.write_text(file.read_text().replace("from 'firefly'", f"from '{module}'"))
    for source, output in [
        ("preload", "preload"),
        ("foreign-preload", "foreign"),
        ("security-preload", "security"),
        ("budget-preload", "budget"),
        ("timeout-preload", "timeout"),
    ]:
        subprocess.run(
            [
                "node",
                str(ROOT / "tooling/bundle_preload.mjs"),
                str(directory / f"{source}.mjs"),
                str(directory / f"{output}.cjs"),
            ],
            check=True,
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", required=True, type=Path)
    parser.add_argument("--electron", required=True, type=Path)
    args = parser.parse_args()
    results = []
    with tempfile.TemporaryDirectory(prefix="firefly-public-ipc-") as directory:
        root = Path(directory).resolve()
        for module, binary in [("firefly", args.firefly), ("electron", args.electron)]:
            stage(root, module)
            results.append(success(execute(binary.resolve(), root / "main.mjs", module)))
        assert results[0] == results[1] and all(results[0].values()), results
        stage(root, "firefly")
        security = success(execute(args.firefly.resolve(), root / "security-main.mjs"))
        assert security == {
            "serialization": True,
            "traps": 0,
            "rawCapability": False,
            "pendingLimit": 16,
            "navigationCycles": 3,
            "destroy": True,
        }, security
        budget = success(execute(args.firefly.resolve(), root / "budget-main.mjs"))
        assert budget == {
            "hostLimit": 64,
            "windows": 5,
            "promptRejection": True,
            "recovery": True,
        }, budget
        crash_probe(args.firefly.resolve(), root / "crash-main.mjs")
        timeout = success(execute(args.firefly.resolve(), root / "timeout-main.mjs"))
        assert timeout == {"deadlineSeconds": 30, "recovery": True}, timeout
    print(
        "PASS Firefly IPC: no getter/proxy execution or raw capability publication; "
        "pending limits, navigation budgets, stale replies, window teardown, crash/replacement, 64-slot host recovery and actual 30-second deadline"
    )
    print(
        "PASS actual Electron/Firefly IPC: early preload, mutable clones/cycles/UTF-16, "
        "sync/async/once/missing/error handlers, concurrent replies and native sender ownership"
    )


if __name__ == "__main__":
    main()
