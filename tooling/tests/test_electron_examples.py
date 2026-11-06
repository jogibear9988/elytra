#!/usr/bin/env python3
"""Validate ESM examples against real runtimes, keeping blocked coverage explicit."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
from pathlib import Path
import subprocess
from test_firefly import execute

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", required=True, type=Path)
    parser.add_argument("--electron", required=True, type=Path)
    parser.add_argument("--firefly-apps", required=True, type=Path)
    parser.add_argument("--electron-apps", required=True, type=Path)
    args = parser.parse_args()
    catalog = json.loads((ROOT / "examples/electron/catalog.json").read_text())["examples"]
    for entry in catalog:
        for file in (ROOT / "examples/electron" / entry["id"]).glob("*.mjs"):
            subprocess.run(["node", "--check", str(file)], check=True, capture_output=True)
    for entry in catalog:
        if not entry["smoke"]:
            result = execute(
                args.firefly.resolve(),
                args.firefly_apps.resolve() / entry["id"] / "main.mjs",
                extra_args=["--smoke"],
            )
            manual = entry["firefly"] == "partial"
            expected = (
                ["ERR_EXAMPLE_MANUAL_VALIDATION:"]
                if manual
                else ["ERR_FIREFLY_UNSUPPORTED_API:", "ERR_EXAMPLE_PLATFORM:"]
            )
            assert result.returncode == 1 and any(code in result.stderr for code in expected), (
                entry["id"],
                result,
            )
            print(
                "MANUAL Firefly:" if manual else "BLOCKED Firefly:",
                entry["id"],
                "(explicit manual-validation gate; not feature acceptance)"
                if manual
                else "(explicit API/platform rejection; not feature acceptance)",
                flush=True,
            )
            continue
        observations = []
        for binary, apps, module in [
            (args.firefly, args.firefly_apps, "firefly"),
            (args.electron, args.electron_apps, "electron"),
        ]:
            result = execute(
                binary.resolve(),
                apps.resolve() / entry["id"] / "main.mjs",
                module,
                extra_args=["--smoke"],
            )
            assert result.returncode == 0, (entry["id"], module, result)
            values = [
                json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")
            ]
            assert len(values) == 1, (entry["id"], result)
            observations.append(values[0])
        assert observations[0] == observations[1], (entry["id"], observations)
        print("PASS shared native startup/assertions:", entry["id"], flush=True)
    for binary, apps, module in [
        (args.firefly, args.firefly_apps, "firefly"),
        (args.electron, args.electron_apps, "electron"),
    ]:
        result = execute(
            binary.resolve(),
            apps.resolve() / "lifecycle/main.mjs",
            module,
            extra_args=["--exit-code=7"],
        )
        assert result.returncode == 7, (module, result)
        values = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
        assert values == [{"trace": ["ready", "when-ready", "quit:7"]}], (module, result)
    print("PASS shared forced app.exit(7), without cancellable quit events", flush=True)
    print(
        "Manual/reference feature examples remain unverified; no OS permissions, purchases, registrations or notifications were triggered."
    )


if __name__ == "__main__":
    main()
