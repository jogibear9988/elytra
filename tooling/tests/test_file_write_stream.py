#!/usr/bin/env python3
"""Compare genuine WriteStream file I/O, lifecycle and descriptor ownership in Node and Mite."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/file-write-stream"


def files(directory):
    return {
        str(path.relative_to(directory)): path.read_bytes()
        for path in directory.rglob("*")
        if path.is_file()
    }


def same_stdout(name, node_stdout, mite_stdout):
    """Keep exact output except Node's documented pre-abort open/close race."""
    if name != "errors":
        return node_stdout == mite_stdout
    node_lines = node_stdout.splitlines(keepends=True)
    mite_lines = mite_stdout.splitlines(keepends=True)
    prefix = 'cancel abort ["error:ABORT_ERR","close"] '
    mite_abort = prefix + "false null true 0\n"
    node_abort = {mite_abort, prefix + "true null true 0\n"}
    # Require exactly one complete observation; do not hide changed events, descriptor
    # ownership, pending state, byte counts, extra lines or Mite opening the target.
    if mite_lines.count(mite_abort) != 1:
        return False
    index = mite_lines.index(mite_abort)
    return (
        len(node_lines) == len(mite_lines)
        and node_lines[index] in node_abort
        and node_lines[:index] == mite_lines[:index]
        and node_lines[index + 1 :] == mite_lines[index + 1 :]
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for genuine filesystem differential tests")
    failures = 0
    for name in [
        "basic",
        "flags",
        "ownership",
        "filehandle",
        "filehandle-close",
        "errors",
        "close",
    ]:
        results = []
        snapshots = []
        with tempfile.TemporaryDirectory(prefix="mite-file-write-stream-") as directory:
            # Reuse the same pathname in both native runs; never rewrite observed output/errors.
            work = Path(directory) / "work"
            for executable in [args.node, str(args.mite.resolve())]:
                if work.exists():
                    shutil.rmtree(work)
                work.mkdir()
                try:
                    result = subprocess.run(
                        [executable, str(FIXTURES / (name + ".mjs"))],
                        cwd=work,
                        capture_output=True,
                        text=True,
                        timeout=25,
                    )
                except subprocess.TimeoutExpired as error:
                    print(f"FAIL file-write-stream/{name}: native runtime timed out: {error.cmd}")
                    failures += 1
                    break
                results.append(result)
                snapshots.append(files(work))
            if len(results) != 2:
                continue
            same_files = snapshots[0] == snapshots[1]
            if name == "errors":
                # Measured Node behavior: its submitted pre-abort open may finish after close.
                # Mite avoids opening altogether. Permit only this one empty-file difference.
                expected = dict(snapshots[0])
                abort_bytes = expected.pop("abort.bin", None)
                same_files = (
                    abort_bytes == b""
                    and "abort.bin" not in snapshots[1]
                    and expected == snapshots[1]
                )
            passed = (
                all(result.returncode == 0 and not result.stderr for result in results)
                and same_stdout(name, results[0].stdout, results[1].stdout)
                and same_files
            )
            print(f"{'PASS' if passed else 'FAIL'} file-write-stream/{name}", flush=True)
            if passed and name == "errors":
                print(
                    "PASS explicit pre-abort difference: Node creates an empty file; Mite performs no open"
                )
            if not passed:
                failures += 1
                print("Node:", results[0])
                print("Mite:", results[1])
                if snapshots[0] != snapshots[1]:
                    print("Actual native output files differ")
    with tempfile.TemporaryDirectory(prefix="mite-file-write-policy-") as directory:
        result = subprocess.run(
            [str(args.mite.resolve()), str(FIXTURES / "policy.mjs")],
            cwd=directory,
            capture_output=True,
            text=True,
            timeout=25,
        )
        passed = (
            result.returncode == 0
            and not result.stderr
            and "PASS Mite WriteStream validation:" in result.stdout
            and files(Path(directory)) == {"sentinel.bin": b"must remain unchanged"}
        )
        print(f"{'PASS' if passed else 'FAIL'} Mite-only file-write-stream/validation", flush=True)
        if not passed:
            failures += 1
            print(result)
    with tempfile.TemporaryDirectory(prefix="mite-write-partial-unit-") as directory:
        # Internal implementation integration, not public custom-fs support or Node/Mite parity.
        result = subprocess.run(
            [args.node, str(FIXTURES / "partial-native.mjs")],
            cwd=directory,
            capture_output=True,
            text=True,
            timeout=25,
        )
        passed = (
            result.returncode == 0
            and not result.stderr
            and "PASS internal writer actual short syscalls:" in result.stdout
            and result.stdout.count("PASS internal writer simulated retry exhaustion:") == 2
        )
        print(
            f"{'PASS' if passed else 'FAIL'} internal writer native partial writes and simulated no-progress failures",
            flush=True,
        )
        if not passed:
            failures += 1
            print(result)
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
