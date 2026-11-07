#!/usr/bin/env python3
"""Check the actual main-host protocol exports/readiness without constructing a window."""

import _support

import argparse
from pathlib import Path
from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    # Use the shipped native Qt plugin; this entry never constructs a BrowserWindow.
    result = run(
        args.firefly.resolve(),
        "--electron-compat",
        ROOT / "firefly/tests/protocol/registration-main.mjs",
    )
    assert result.returncode == 0, result
    assert (
        "PASS actual Firefly main protocol exports/registration/readiness without a window"
        in result.stdout
    ), result
    assert "VERIFICATION FAILED" not in result.stderr, result
    print(result.stdout.strip())


if __name__ == "__main__":
    main()
