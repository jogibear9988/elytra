#!/usr/bin/env python3
"""Genuine --expose-gc CLI boundaries and rooted/weak collection; Node is an oracle only.

These execute the standalone CLI, which owns binary-mode standard streams on
Windows. Embedded callers of Mite::run() retain control of their CRT stream modes.
"""

import _support
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    fixture = ROOT / "mite/tests/expose-gc/main.mjs"
    cases = [
        [str(fixture)],
        [str(fixture), "--expose-gc"],
        ["--expose-gc", str(fixture), "application"],
        ["--expose-gc", "--expose-gc", str(fixture)],
        ["--expose-gc", "--", str(fixture), "--expose-gc"],
    ]
    eval_source = "console.log(JSON.stringify({gc:typeof gc,argv:process.argv.slice(1),flags:process.execArgv}));"
    cases += [
        ["-e", eval_source, "--expose-gc"],
        ["-e", eval_source, "--expose-gc", "argument"],
        ["-e", eval_source, "argument", "--expose-gc"],
        ["-e", eval_source, "--", "--expose-gc"],
        ["-e", eval_source, "--expose-gc", "--", "--expose-gc"],
        ["-e", eval_source, "argument", "--", "--expose-gc"],
        ["-p", "typeof gc", "--expose-gc"],
        ["-p", "--expose-gc"],
        ["-p", "'first'", "-e", "'second'", "--expose-gc"],
        ["-e", "'first'", "-p", "'second'", "--expose-gc"],
        ["--expose-gc", "-e", eval_source, "argument"],
        ["--expose-gc", "-p", "typeof gc"],
        ["--expose-gc", "--print", "gc()"],
        ["--expose-gc", "--eval", "console.log(typeof gc)"],
    ]
    with tempfile.TemporaryDirectory(prefix="mite-expose-gc-") as temporary:
        root = Path(temporary)
        # The option terminator makes this an actual filename, never a flag.
        (root / "--expose-gc").write_text("console.log(typeof globalThis.gc);\n")
        cases += [["--", "./--expose-gc"], ["--expose-gc", "--", "./--expose-gc"]]
        for command in cases:

            def run(binary):
                result = subprocess.run(
                    [str(binary), *command],
                    cwd=root,
                    stdin=subprocess.DEVNULL,
                    capture_output=True,
                    text=True,
                    timeout=20,
                )
                assert result.returncode == 0, (
                    command,
                    result.returncode,
                    result.stdout,
                    result.stderr,
                )
                return result.stdout

            expected = run(args.node)
            if args.mite:
                assert run(args.mite.resolve()) == expected, command
        stdin_cases = [
            (["-p"], "40+2"),
            (["--expose-gc", "-p"], "40+2"),
            (["-p", "--expose-gc"], "40+2"),
            (
                ["-p", "--expose-gc"],
                "JSON.stringify({gc:typeof gc,flags:process.execArgv,argv:process.argv.slice(1)})",
            ),
            (
                ["-p", "--expose-gc", "--", "./--expose-gc"],
                "JSON.stringify({gc:typeof gc,flags:process.execArgv,argv:process.argv.slice(1)})",
            ),
            (["-p"], ""),
            (["--expose-gc", "-p"], ""),
            (
                ["-e", "console.log('explicit')", "-p", "--expose-gc"],
                "throw Error('must not read stdin')",
            ),
        ]
        for command, source in stdin_cases:

            def run_stdin(binary):
                result = subprocess.run(
                    [str(binary), *command],
                    cwd=root,
                    input=source,
                    capture_output=True,
                    text=True,
                    timeout=20,
                )
                assert result.returncode == 0, (
                    command,
                    result.returncode,
                    result.stdout,
                    result.stderr,
                )
                return result.stdout

            expected = run_stdin(args.node)
            if args.mite:
                assert run_stdin(args.mite.resolve()) == expected, command
        # Windows CRT text mode treats CTRL-Z as EOF. That would silently
        # truncate this invalid program to 40+2 and falsely print 42.
        for binary in [args.node, *([args.mite.resolve()] if args.mite else [])]:
            result = subprocess.run(
                [str(binary), "--expose-gc", "-p"],
                cwd=root,
                input="40+2\x1athrow Error('raw CTRL-Z must not be EOF')",
                capture_output=True,
                text=True,
                timeout=20,
            )
            assert result.returncode == 1 and not result.stdout, (
                result.returncode,
                result.stdout,
                result.stderr,
            )
        if args.mite:
            for options in [
                ["--expose_gc"],
                ["--expose-gc=true"],
                ["--expose-gc", "--jitless"],
                ["--expose-gc"],
                ["-e", "console.log('must not execute')", "--jitless"],
            ]:
                result = subprocess.run(
                    [str(args.mite.resolve()), *options],
                    cwd=root,
                    stdin=subprocess.DEVNULL,
                    capture_output=True,
                    text=True,
                    timeout=10,
                )
                assert result.returncode == 9, (
                    options,
                    result.returncode,
                    result.stdout,
                    result.stderr,
                )
            if os.name != "nt":
                descriptor = os.open(root, os.O_RDONLY)
                try:
                    result = subprocess.run(
                        [str(args.mite.resolve()), "--expose-gc", "-p"],
                        stdin=descriptor,
                        capture_output=True,
                        text=True,
                        timeout=10,
                    )
                    assert result.returncode == 1 and "cannot read print source" in result.stderr, (
                        result
                    )
                    assert not result.stdout, result
                finally:
                    os.close(descriptor)
                master, slave = os.openpty()
                try:
                    result = subprocess.run(
                        [str(args.mite.resolve()), "--expose-gc", "-p"],
                        stdin=slave,
                        capture_output=True,
                        text=True,
                        timeout=10,
                    )
                    assert result.returncode == 9 and "interactive stdin/REPL" in result.stderr, (
                        result
                    )
                finally:
                    os.close(master)
                    os.close(slave)
    print(
        f"PASS expose-gc {len(cases) + len(stdin_cases) + 1} Node CLI controls"
        + (" and actual Mite" if args.mite else " (Mite not run)")
    )


if __name__ == "__main__":
    main()
