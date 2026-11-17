#!/usr/bin/env python3
"""Compare genuine CommonJS interception/cache/lookup with Node and native Mite GC."""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(binary, fixture, *, host=False):
    result = subprocess.run(
        [str(binary), "--expose-gc", str(fixture / "main.mjs")],
        cwd=fixture,
        capture_output=True,
        text=True,
        timeout=25,
    )
    assert result.returncode == 0 and result.stderr == ("HOST_SHUTDOWN\n" if host else ""), result
    lines = result.stdout.splitlines()
    assert len(lines) == 5 and all(line.startswith("PASS ") for line in lines), result
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--host", type=Path)
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.node or (not args.reference_only and not args.mite):
        parser.error("Node and --mite required (or --reference-only)")
    with tempfile.TemporaryDirectory(prefix="mite-commonjs-interception-") as directory:
        fixture = Path(directory).resolve() / "fixture"
        shutil.copytree(ROOT / "mite/tests/commonjs-interception", fixture)
        package = fixture / "external/node_modules/owned-alternate"
        package.mkdir(parents=True)
        (package / "package.json").write_text('{"main":"index.cjs"}\n')
        (package / "index.cjs").write_text("module.exports = 'external-package';\n")
        reference = run(args.node, fixture)
        print(reference, end="", flush=True)
        if not args.reference_only:
            assert run(args.mite.resolve(), fixture) == reference
            if args.host:
                assert run(args.host.resolve(), fixture, host=True) == reference
            print("PASS native CommonJS interception differential with actual GC")


if __name__ == "__main__":
    main()
