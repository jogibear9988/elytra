#!/usr/bin/env python3
"""Compare real external file ESM and check the retained file/CORS security boundary."""

import _support  # Standalone regression CLI imports.

import argparse
from pathlib import Path
import shutil
import tempfile
from test_firefly import execute, success

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", required=True, type=Path)
    parser.add_argument("--electron", required=True, type=Path)
    args = parser.parse_args()
    results = []
    with tempfile.TemporaryDirectory(prefix="firefly-file-modules-") as directory:
        root = Path(directory).resolve()
        for module, binary in [("firefly", args.firefly), ("electron", args.electron)]:
            shutil.copytree(ROOT / "firefly/tests/loading", root, dirs_exist_ok=True)
            for file in root.glob("*.mjs"):
                file.write_text(file.read_text().replace("from 'firefly'", f"from '{module}'"))
            results.append(success(execute(binary.resolve(), root / "main.mjs", module)))
        assert (
            results[0] == results[1]
            and all(value is True for key, value in results[0].items() if key != "node")
            and results[0]["node"] is False
        ), results
        # The stricter file-fetch policy is intentional, not asserted as Electron parity.
        source = ROOT / "firefly/tests/loading/security-main.mjs"
        (root / source.name).write_text(source.read_text())
        assert success(execute(args.firefly.resolve(), root / source.name)) == {
            "fileFetchDenied": True,
            "networkFileDenied": True,
        }
    print(
        "PASS real Electron/Firefly external file ESM: static/dynamic graph, cache/live bindings, "
        "Unicode, reload, MIME, CSP and absent Node APIs"
    )
    print("PASS Firefly file modules retain fetch(file:) and HTTP-to-file script refusal")


if __name__ == "__main__":
    main()
