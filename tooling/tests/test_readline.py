#!/usr/bin/env python3
"""Exercise real Node/Mite readline and native file streams without a desktop UI."""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(command):
    result = subprocess.run(
        [str(x) for x in command], cwd=ROOT, capture_output=True, text=True, timeout=45
    )
    assert result.returncode == 0, (command, result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--host", required=True, type=Path)
    args = parser.parse_args()
    node = shutil.which("node")
    assert node
    for binary in [node, args.mite.resolve(), args.host.resolve()]:
        result = run(
            ([binary, "--expose-gc"] if binary == node else [binary])
            + ["mite/tests/readline/core.mjs"]
        )
        extra = run([binary, "mite/tests/readline/lifetime.mjs"])
        assert "PASS readline delivery" in extra.stdout, extra
        assert "PASS readline UTF-8" in result.stdout, result
    policy = run([args.mite.resolve(), "mite/tests/readline/policy.mjs"])
    assert "PASS explicit readline" in policy.stdout, policy
    with tempfile.TemporaryDirectory(prefix="elytra-lines-") as directory:
        file = Path(directory) / "lines.txt"
        file.write_bytes((("Unicode ä 😀 " + "x" * 70 + "\r\n") * 30000).encode())
        for binary in [node, args.mite.resolve(), args.host.resolve()]:
            result = run(
                ([binary, "--expose-gc"] if binary == node else [binary])
                + ["mite/tests/readline/files.mjs", file]
            )
            assert "PASS real FileHandle" in result.stdout, result
            aborted = run([binary, "mite/tests/readline/file-abort.mjs", file])
            assert "PASS real FileHandle pre/active abort" in aborted.stdout, aborted
    print(
        "PASS Node/Mite UTF-8 readline, CRLF/EOF, iterator pressure, lifecycle/questions/abort and real native FileHandle streams"
    )


if __name__ == "__main__":
    main()
