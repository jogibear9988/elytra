#!/usr/bin/env python3
"""Compare native process measurements and error policy with Node; all tests run without GUI."""

import _support  # Standalone regression imports.

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--host", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Install the pinned Node reference first")
    for source in sorted((ROOT / "mite/tests/process-timing").glob("*")):
        if source.suffix not in {".mjs", ".cjs"}:
            continue
        results = [
            subprocess.run([binary, str(source)], capture_output=True, text=True, timeout=15)
            for binary in [args.node, args.mite]
        ]
        assert all(result.returncode == 0 and not result.stderr for result in results), results
        assert results[0].stdout == results[1].stdout, results
        lines = results[0].stdout.splitlines()
        assert lines and all(line.startswith(("PASS ", "DONE ")) for line in lines), results
        if source.suffix == ".mjs":
            assert lines[-1] == f"DONE process timing {source.stem}", results
        print(results[1].stdout, end="", flush=True)
    source = str(ROOT / "mite/tests/process-timing/runtime.mjs")
    results = [
        subprocess.run(command, capture_output=True, text=True, timeout=15)
        for command in [[args.node, "--expose-gc", source], [args.host, source]]
    ]
    assert all(result.returncode == 0 for result in results), results
    assert results[0].stdout == results[1].stdout, results
    assert "PASS process timing samples survive actual GC\n" in results[0].stdout, results
    assert results[0].stderr == "" and results[1].stderr == "HOST_SHUTDOWN\n", results
    print("PASS actual Node/non-GUI embedded-host process clocks and CPU across collection")
    example = ROOT / "examples/mite-process-metrics/main.mjs"
    for binary in [args.node, args.mite]:
        result = subprocess.run([binary, str(example)], capture_output=True, text=True, timeout=15)
        assert result.returncode == 0 and not result.stderr, result
        report = json.loads(result.stdout)
        assert report["digest"] == hashlib.sha256(bytes(65536)).hexdigest(), report
        assert int(report["elapsedNanoseconds"]) > 0 and report["uptimeSeconds"] > 0, report
        assert all(value >= 0 for value in report["cpuMicroseconds"].values()), report
    print("PASS modern metrics example with independent SHA-256 result in actual Node/Mite")


if __name__ == "__main__":
    main()
