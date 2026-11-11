#!/usr/bin/env python3
"""Run actual main/utility MessagePort transfer without creating a BrowserWindow."""

import _support

import argparse
from pathlib import Path
import shutil
import tempfile
from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly-utility-ports-") as directory:
        root = Path(directory)
        for name in ("main.mjs", "child.mjs"):
            shutil.copyfile(ROOT / "firefly/tests/utility-ports" / name, root / name)
        (root / "config").mkdir()
        result = run(args.firefly.resolve(), root / "main.mjs")
        assert result.returncode == 0 and "PASS actual utility MessagePorts:" in result.stdout, (
            result
        )
        assert "VERIFICATION FAILED" not in result.stderr, result
        print(result.stdout.strip())


if __name__ == "__main__":
    main()
