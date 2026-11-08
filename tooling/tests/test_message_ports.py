#!/usr/bin/env python3
"""Run actual main-process port acceptance, without creating desktop windows."""

import _support

import argparse
from pathlib import Path
import tempfile
from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--electron", type=Path)
    args = parser.parse_args()
    if not args.firefly and not args.electron:
        parser.error("Provide --firefly and/or --electron")
    with tempfile.TemporaryDirectory(prefix="elytra-message-ports-") as directory:
        root = Path(directory)
        source = (ROOT / "firefly/tests/message-ports/main.mjs").read_text()
        for name, binary in [("Firefly", args.firefly), ("Electron reference", args.electron)]:
            if binary is None:
                continue
            file = root / ("electron.mjs" if args.electron == binary else "firefly.mjs")
            file.write_text(
                source.replace("from 'firefly'", "from 'electron'")
                if name == "Electron reference"
                else source
            )
            result = run(binary.resolve(), file)
            assert (
                result.returncode == 0 and "PASS public main MessageChannelMain:" in result.stdout
            ), result
            assert "VERIFICATION FAILED" not in result.stderr, result
            print(name + ": " + result.stdout.strip())


if __name__ == "__main__":
    main()
