#!/usr/bin/env python3
"""Run pinned unmodified Code-OSS source bootstrap after type erasure, without GUI."""

import _support

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    assert args.node, "Pinned Node test reference required"
    pinned = ROOT / "vendor/code-oss-tests"
    manifest = json.loads((pinned / "manifest.json").read_text())
    for name, record in manifest["files"].items():
        assert Path(name).name == name
        assert hashlib.sha256((pinned / name).read_bytes()).hexdigest() == record["sha256"], name
    stripped = subprocess.run(
        [
            args.node,
            "--disable-warning=ExperimentalWarning",
            "--input-type=module",
            "-e",
            'import {stripTypeScriptTypes} from "node:module"; import fs from "node:fs"; '
            'process.stdout.write(stripTypeScriptTypes(fs.readFileSync(process.argv[1], "utf8"), {mode:"strip"}));',
            str(pinned / "bootstrap-node.ts"),
        ],
        capture_output=True,
        timeout=20,
    )
    assert stripped.returncode == 0 and not stripped.stderr and stripped.stdout, stripped
    with tempfile.TemporaryDirectory(prefix="mite-code-oss-bootstrap-") as directory:
        work = Path(directory).resolve()
        source = work / "out/bootstrap-node.mjs"
        source.parent.mkdir()
        source.write_bytes(stripped.stdout)
        shutil.copyfile(pinned / "LICENSE.txt", work / "LICENSE.txt")
        portable = work / "portable"
        (portable / "tmp").mkdir(parents=True)
        env = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith(("VSCODE_", "ELECTRON_"))
        }
        # Upstream source mode; no ASAR or loader-hook adaptation.
        env["VSCODE_DEV"] = "1"
        results = [
            subprocess.run(
                [
                    str(binary),
                    str(ROOT / "mite/tests/process-events/code-oss.mjs"),
                    str(source),
                    str(portable),
                ],
                cwd=work,
                env=env,
                capture_output=True,
                text=True,
                timeout=20,
            )
            for binary in [args.node, args.mite.resolve()]
        ]
        assert all(result.returncode == 0 and not result.stderr for result in results), results
        assert (
            results[0].stdout
            == results[1].stdout
            == ("PASS actual pinned Code-OSS source bootstrap SIGPIPE/cwd/portable paths\n")
        ), results
        print(results[1].stdout, end="", flush=True)


if __name__ == "__main__":
    main()
