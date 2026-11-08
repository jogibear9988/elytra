#!/usr/bin/env python3
"""Exercise the exact policy native Host with controlled providers, never OS policy changes."""

import _support

import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    cases = [("main", []), ("shutdown", ["--leave-active"])]
    cases += [
        (scenario, ["--expect-failure"])
        for scenario in [
            "read-error",
            "unknown-key",
            "wrong-type",
            "invalid-utf8",
            "non-finite",
            "oversize",
            "callback-throw",
        ]
    ]
    for scenario, flags in cases:
        result = subprocess.run(
            [
                str(args.probe.resolve()),
                str(ROOT / "mite/tests/policy-watcher/host.mjs"),
                "--scenario",
                scenario,
                *flags,
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=12,
        )
        assert result.returncode == 0 and "PASS exact policy Host" in result.stdout, result
        if "--expect-failure" in flags:
            expected = (
                "Native policy snapshot failed; watcher retired"
                if scenario == "read-error"
                else "CONTROLLED_POLICY_CALLBACK_THROW"
                if scenario == "callback-throw"
                else "Native policy snapshot violated its contract; watcher retired"
            )
            assert expected in result.stderr and "runtimeStatus=1" in result.stdout, result
        else:
            assert not result.stderr and "runtimeStatus=0" in result.stdout, result
        print(scenario + ": " + result.stdout.strip(), flush=True)
    print(
        "PASS exact production Host controlled-provider subprocesses; no OS policy or managed preference writes"
    )


if __name__ == "__main__":
    main()
