#!/usr/bin/env python3
"""Compare real ESM-owned require contexts with Node, including actual host GC."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(binary, source, *options, host=False):
    result = subprocess.run(
        [str(binary), *options, str(source)], capture_output=True, text=True, timeout=20
    )
    assert result.returncode == 0, result
    assert result.stderr == ("HOST_SHUTDOWN\n" if host else ""), result
    assert result.stdout and all(line.startswith("PASS ") for line in result.stdout.splitlines()), (
        result
    )
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for differential tests")
    with tempfile.TemporaryDirectory(prefix="mite module %# Grüße ") as temporary:
        # Resolve the physical temp directory so macOS /var aliases do not obscure
        # comparisons with the loader's canonical identities. The URL still needs
        # spaces, UTF-8, percent and fragment-marker encoding.
        fixtures = Path(temporary).resolve() / "fixture"
        shutil.copytree(ROOT / "mite/tests/module-utilities", fixtures)
        dependency = fixtures / "node_modules/fixture-dependency"
        dependency.mkdir(parents=True)
        (dependency / "package.json").write_text(
            '{"exports":{"require":"./require.cjs","import":"./import.mjs"}}'
        )
        (dependency / "require.cjs").write_text("module.exports = 'require';\n")
        (dependency / "import.mjs").write_text("export default 'import';\n")
        for name in ["runtime.mjs", "validation.mjs", "commonjs-entry.cjs"]:
            source = fixtures / name
            reference = run(args.node, source)
            assert reference == run(args.mite.resolve(), source), name
            print(reference, end="", flush=True)
        source = fixtures / "runtime.mjs"
        collected = run(args.node, source, "--expose-gc")
        assert "PASS created require/resolve survive actual GC\n" in collected
        assert collected == run(args.host.resolve(), source, host=True)
        print(
            "PASS actual GC with retained generated require/resolve and uncached synthetic parent"
        )
        print(run(args.mite.resolve(), fixtures / "mite-only.mjs"), end="", flush=True)


if __name__ == "__main__":
    main()
