#!/usr/bin/env python3
"""Stage one actual native helper image and run its Core/AppContainer broker component.

This is deliberately not a LibWeb, desktop, rendering or release acceptance test.
A failed/timeout run retains its sole owned stage instead of deleting resources
whose native process cleanup has not been independently accepted by the probe.
"""

import _support

import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument(
        "--targets",
        type=Path,
        required=True,
        help="Actual target basename|source and runtime DLL closure",
    )
    parser.add_argument("--resources", type=Path, required=True)
    parser.add_argument("--revision", required=True)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("Actual native Windows execution is required; no emulated success")
    probe = args.probe.resolve(strict=True)
    spec = importlib.util.spec_from_file_location(
        "owned_windows_stage", ROOT / "tooling/windows-sandbox/stage.py"
    )
    staging = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(staging)
    temporary = Path(tempfile.mkdtemp(prefix="firefly-broker-")).resolve()
    stage = temporary / "firefly-helper"
    try:
        staging.stage(
            stage,
            args.targets.resolve(strict=True),
            args.resources.resolve(strict=True),
            args.revision,
        )
        result = subprocess.run(
            [str(probe), str(stage / "bin/WebContent.exe")],
            capture_output=True,
            text=True,
            timeout=120,
            env={**os.environ, "FIREFLY_BROKER_SECRET": "must-not-enter-child"},
        )
        expected = (
            "PASS real AppContainer ImageDecoder PNG, exact role and natural exit\n"
            "PASS real Core AppContainer identity, pseudo/truncation rejection, partial-copy rollback and token/handle refunds\n"
            "PASS actual multi-child remote Park bound, Browser-origin independence and disconnect recovery\n"
        )
        if result.returncode != 0 or result.stdout != expected or result.stderr:
            raise RuntimeError(
                f"Native broker component failed: exit={result.returncode}, stdout={result.stdout!r}, stderr={result.stderr!r}"
            )
    except Exception as error:
        raise RuntimeError(
            f"Windows broker component failed; retained owned stage {stage}"
        ) from error
    else:
        shutil.rmtree(temporary)
    print(expected, end="")
    print(
        "Component only: Browser-origin retention/backpressure and full Windows desktop remain unaccepted"
    )


if __name__ == "__main__":
    main()
