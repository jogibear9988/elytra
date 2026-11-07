#!/usr/bin/env python3
"""Execute real ESM/CJS resolve chains against Node and Mite, including owned-host GC."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(binary, source, *options, host=False):
    result = subprocess.run(
        [str(binary), *options, str(source)], capture_output=True, text=True, timeout=25
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
    assert args.node
    with tempfile.TemporaryDirectory(prefix="module hooks %# Grüße ") as directory:
        root = Path(directory).resolve() / "fixture"
        shutil.copytree(ROOT / "mite/tests/module-hooks", root)
        nested = root / "nested"
        nested.mkdir()
        (nested / "relative.mjs").write_text("export default 'nested';\n")
        dependency = root / "node_modules/conditional"
        dependency.mkdir(parents=True)
        (dependency / "package.json").write_text(
            '{"exports":{"custom":{"import":"./import.mjs","require":"./require.cjs"},"default":"./default.mjs"}}'
        )
        (dependency / "import.mjs").write_text("export default 'custom';\n")
        (dependency / "require.cjs").write_text("module.exports = 'custom-cjs';\n")
        (dependency / "default.mjs").write_text("throw new Error('Wrong export condition');\n")
        for name in ["runtime.mjs", "validation.mjs"]:
            source = root / name
            reference = run(args.node, source)
            assert reference == run(args.mite.resolve(), source), name
            print(reference, end="", flush=True)
        source = root / "runtime.mjs"
        collected = run(args.node, source, "--expose-gc")
        assert "PASS registered hooks and retained continuation survive actual GC\n" in collected
        assert collected == run(args.host.resolve(), source, host=True)
        print("PASS actual host GC with registered/retained resolver continuations")
        print(run(args.mite.resolve(), root / "mite-only.mjs"), end="", flush=True)


if __name__ == "__main__":
    main()
