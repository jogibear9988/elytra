#!/usr/bin/env python3
"""Compare source-analyzed CommonJS namespaces with actual Node linking and execution."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for differential CommonJS namespace tests")
    for name in [
        "main",
        "snapshots",
        "reexports",
        "cache",
        "reload",
        "retry",
        "missing",
        "missing-chain",
    ]:
        results = []
        with tempfile.TemporaryDirectory(prefix="mite-cjs-namespaces-") as directory:
            work = Path(directory).resolve()
            source = work / "sources"
            shutil.copytree(ROOT / "mite/tests/cjs-namespaces", source)
            for executable in [args.node, str(args.mite.resolve())]:
                result = subprocess.run(
                    [executable, str(source / (name + ".mjs"))],
                    cwd=work,
                    capture_output=True,
                    text=True,
                    timeout=20,
                )
                results.append(result)
                assert not (work / "early-executed.bin").exists(), (
                    "Linking executed the CommonJS body before missing-export validation"
                )
                if name.startswith("missing"):
                    # Native stack formatting differs; both must fail at link time, before any code runs.
                    assert (
                        result.returncode == 1
                        and not result.stdout
                        and "SyntaxError" in result.stderr
                        and "absent" in result.stderr
                    ), result
                else:
                    assert result.returncode == 0 and not result.stderr and result.stdout, result
            assert results[0].stdout == results[1].stdout, (name, results)
            print("PASS CommonJS namespace/" + name + " against Node", flush=True)


if __name__ == "__main__":
    main()
