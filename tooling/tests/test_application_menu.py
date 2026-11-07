#!/usr/bin/env python3
"""Accept actual public main menus without constructing a BrowserWindow."""

import _support

import argparse
from pathlib import Path
from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    result = run(
        args.firefly.resolve(),
        "--electron-compat",
        ROOT / "firefly/tests/menu/main.mjs",
    )
    assert result.returncode == 0, result
    assert (
        "PASS actual Firefly main menu exports/install/update/remove without a window"
        in result.stdout
    ), result
    assert "VERIFICATION FAILED" not in result.stderr, result
    print(result.stdout.strip())


if __name__ == "__main__":
    main()
