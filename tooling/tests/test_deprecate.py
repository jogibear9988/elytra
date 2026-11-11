#!/usr/bin/env python3
"""Compare genuine asynchronous warnings and deprecation call/constructor behavior."""

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/deprecate"
EXPECTED = [
    "PASS deprecate once-per-code/wrapper, real async warnings, receiver/args/return and constructor identity",
    "PASS noDeprecation dynamic suppression and asynchronous throwDeprecation preserve wrapped calls",
]
WARNINGS = [
    "DeprecationWarning: owned first",
    "DeprecationWarning: owned uncoded a",
    "DeprecationWarning: owned uncoded b",
    "DeprecationWarning: owned constructor",
    "DeprecationWarning: owned no prototype",
    "DeprecationWarning: owned delayed",
    "OwnedErrorWarning: owned exact",
    "OwnedWarning: owned explicit",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.node or (not args.reference_only and not args.mite):
        parser.error("Require Node and --mite, or --reference-only")
    runtimes = [("Node", args.node)]
    if not args.reference_only:
        runtimes.append(("Mite", str(args.mite.resolve())))
    for name, binary in runtimes:
        for fixture in ["main.mjs", "trace.mjs"]:
            result = subprocess.run(
                [binary, str(FIXTURES / fixture)],
                cwd=ROOT,
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=10,
            )
            assert result.returncode == 0, (name, fixture, result)
            if fixture == "main.mjs":
                assert result.stdout.splitlines() == EXPECTED, (name, result)
                for warning in WARNINGS:
                    assert result.stderr.count(warning) == 1, (name, warning, result)
                assert "owned detail" in result.stderr, (name, result)
                assert "owned duplicate" not in result.stderr, (name, result)
                assert "OWNED_THROW" not in result.stderr, (name, result)
            else:
                assert result.stdout == "PASS traceDeprecation warning scheduled\n", (name, result)
                assert result.stderr.count("DeprecationWarning: owned trace warning") == 1, (
                    name,
                    result,
                )
                assert "ownedWarningCaller" in result.stderr, (name, result)
            print(f"PASS {name} {fixture}: actual warning events and stderr", flush=True)


if __name__ == "__main__":
    main()
