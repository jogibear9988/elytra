#!/usr/bin/env python3
"""Compare native runtime clock behavior in Node, Mite and an actual collecting host."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(binary, source, *options, host=False):
    result = subprocess.run(
        [str(binary), *options, str(source)], capture_output=True, text=True, timeout=20
    )
    assert result.returncode == 0, result
    assert result.stderr == ("HOST_SHUTDOWN\n" if host else ""), result
    assert result.stdout and all(line.startswith("PASS ") for line in result.stdout.splitlines()), (
        result
    )
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for differential tests")
    fixtures = ROOT / "mite/tests/perf-hooks"
    for name in ["clock.mjs", "receivers.mjs"]:
        source = fixtures / name
        reference = run(args.node, source)
        assert reference == run(args.mite.resolve(), source)
        print(reference, end="", flush=True)
    reference = run(args.node, fixtures / "clock.mjs", "--expose-gc")
    assert "PASS performance clock survives actual GC\n" in reference
    assert reference == run(args.host.resolve(), fixtures / "clock.mjs", host=True)
    print("PASS actual native clock after Node/Mite embedded-host collection")
    pinned = ROOT / "vendor/code-oss-tests"
    manifest = json.loads((pinned / "manifest.json").read_text())
    for name, record in manifest["files"].items():
        assert Path(name).name == name
        assert hashlib.sha256((pinned / name).read_bytes()).hexdigest() == record["sha256"], name
    # The Node reference is a build/test tool here, never a runtime dependency.
    # Disable only the known experimental type-stripper warning, not parse errors.
    stripped = subprocess.run(
        [
            args.node,
            "--disable-warning=ExperimentalWarning",
            "--input-type=module",
            "-e",
            'import {stripTypeScriptTypes} from "node:module"; import fs from "node:fs"; '
            'process.stdout.write(stripTypeScriptTypes(fs.readFileSync(process.argv[1], "utf8"), {mode:"strip"}));',
            str(pinned / "performance.ts"),
        ],
        capture_output=True,
        timeout=20,
    )
    assert stripped.returncode == 0 and not stripped.stderr and stripped.stdout, stripped
    with tempfile.TemporaryDirectory(prefix="mite-code-oss-performance-") as directory:
        source = Path(directory) / "performance.mjs"
        source.write_bytes(stripped.stdout)
        shutil.copyfile(pinned / "LICENSE.txt", Path(directory) / "LICENSE.txt")
        results = [
            subprocess.run(
                [str(binary), str(fixtures / "code-oss.mjs"), str(source)],
                capture_output=True,
                text=True,
                timeout=20,
            )
            for binary in [args.node, args.mite.resolve()]
        ]
        assert all(result.returncode == 0 and not result.stderr for result in results), results
        assert (
            results[0].stdout
            == results[1].stdout
            == "PASS pinned actual Code-OSS performance module origin/startup marks/clearing\n"
        ), results
        print(results[1].stdout, end="", flush=True)


if __name__ == "__main__":
    main()
