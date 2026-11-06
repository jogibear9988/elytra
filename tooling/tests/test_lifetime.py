#!/usr/bin/env python3
"""Compare forced-GC task lifetimes against Node using the non-GUI embedded host."""

import _support  # Standalone regression helpers.

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for GC differential tests")
    expected = {
        "http-release": "HTTP released true\n",
        "nested-composites": "nested true 32\n",
        "nested-source-lifetime": "intermediate collected true\nleaf true true 1\n",
        "finalizer-tokens": "group true false\ntokens true true true\n",
        "weak-validation": "symbol true true [object WeakRef]\n" + "TypeError\n" * 7,
        "finalizer-nested": "nested cleanup true true\n",
        "finalizer-stop": "entry completed\n",
        "weak-targets": "creation true\nmicrotask true\nnext true\n",
        "finalizers": "unregister true false\ncleanup true true\n",
        "finalizer-error": "entry completed\n",
        "composite-listeners": "listeners 32 32 32\n",
        "composite-release": "release true true\n",
        "timeout-listeners": "deadlines true\n",
        "timeout-release": "unused true\n",
        "guard-semantics": "bookkeeping true\n",
    }
    for file in sorted((ROOT / "mite/tests/lifetime").glob("*.mjs")):
        results = [
            subprocess.run(command, capture_output=True, text=True, timeout=20)
            for command in [[args.node, "--expose-gc", str(file)], [str(args.host), str(file)]]
        ]
        status = 1 if file.stem in {"finalizer-error", "finalizer-stop"} else 0
        assert all(result.returncode == status for result in results), (file.name, results)
        if file.stem == "nested-listener-retention":
            # Node 24.15.0 unregisters the flattened source when the intermediate
            # dies. Preserve the reachable leaf's contract and assert this actual
            # reference defect separately rather than fake differential equality.
            assert results[0].stdout == "nested listeners 0\n", results
            assert results[1].stdout == "nested listeners 32\n", results
        else:
            assert results[0].stdout == results[1].stdout == expected[file.stem], (
                file.name,
                results,
            )
        assert results[1].stderr.count("HOST_SHUTDOWN\n") == 1, results
        if status:
            marker = (
                "stopped before cleanup fixture"
                if file.stem == "finalizer-stop"
                else "finalizer failure fixture"
            )
            assert all(marker in result.stderr for result in results), results
        else:
            assert not results[0].stderr and results[1].stderr == "HOST_SHUTDOWN\n", (
                file.name,
                results,
            )
        print("PASS non-GUI lifetime:", file.name, flush=True)
    print(
        "PASS actual Node/Mite weak targets, cleanup jobs, listener-only retention and resource release"
    )


if __name__ == "__main__":
    main()
