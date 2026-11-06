#!/usr/bin/env python3
"""Run shared fixtures in real Node and Mite; no engine substitution or output filtering."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def run(executable, *args):
    return subprocess.run(
        [executable, *map(str, args)], capture_output=True, text=True, encoding="utf-8", timeout=20
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for differential tests")
    fixtures = Path(__file__).resolve().parents[2] / "mite/tests/fixtures"
    cases = [
        ("language.js", [], 0, "[4,16]\n18446744073709551616\nGrüße 🪲\ntrue\n"),
        (
            "exception-shape.js",
            [],
            0,
            "Error: explicit message\nTypeError: explicit message\nRangeError: explicit message\n",
        ),
        ("promises.js", [], 0, "start\nend\nfirst\nsecond\nnested\n"),
        ("handled-rejection.js", [], 0, "handled\n"),
        ("awaited-rejection.js", [], 0, "awaited rejection handled\n"),
        ("arguments.js", ["hello world", "--flag", "🪲"], 0, '["hello world","--flag","🪲"]\n'),
        ("microtasks.js", [], 0, "start\nend\nmicrotask\npromise\nlast\nnested\n"),
        ("timers.js", [], 0, "start\nend\ntrue argument 42\nmicrotask\npromise\nsecond\n"),
        ("timer-cancellation.js", [], 0, "kept\n1 true\n2 true\n3 true\n"),
        (
            "timer-disposal.js",
            [],
            0,
            "true true\nfunction\nclearTimeout clearInterval false\ndone\n",
        ),
        ("timer-ref.js", [], 0, "true\ntrue false\ntrue true\n"),
        (
            "callback-validation.js",
            [],
            0,
            "TypeError ERR_INVALID_ARG_TYPE\n" * 15 + "TypeError\n" * 2,
        ),
        ("microtask-throws.js", [], 1, ""),
        ("timer-throws.js", [], 1, ""),
        ("timer-rejection.js", [], 1, ""),
        ("promise-liveness.js", [], 0, "pending promise does not keep loop alive\n"),
        ("stderr.js", [], 0, ""),
        ("throw.js", [], 1, ""),
        ("unhandled-rejection.js", [], 1, ""),
        ("syntax-error.js", [], 1, ""),
    ]
    failures = []
    # Capture bytes directly: universal-newline decoding hides a Windows CRT
    # LF -> CRLF conversion, which corrupts console output inside native pipes.
    raw_source = r"console.log('LF\nCRLF\r\nNUL\x00SUB\x1a文🪲');console.error('err\n文🪲');"
    raw_expected = (
        "LF\nCRLF\r\nNUL\x00SUB\x1a文🪲\n".encode("utf-8"),
        "err\n文🪲\n".encode("utf-8"),
    )
    for executable in [args.node, args.mite]:
        result = subprocess.run([executable, "-e", raw_source], capture_output=True, timeout=15)
        passed = result.returncode == 0 and (result.stdout, result.stderr) == raw_expected
        print(
            f"{'PASS' if passed else 'FAIL'} raw console stdout/stderr bytes ({Path(executable).name})"
        )
        if not passed:
            failures.append("raw console stdout/stderr bytes")
            print(result, file=sys.stderr)
    # Each direct child must report this actual launcher as its parent, before
    # timers or async I/O could accidentally initialize platform services for it.
    for executable in [args.node, args.mite]:
        result = run(executable, "-e", "console.log(process.ppid)")
        passed = (
            result.returncode == 0 and result.stdout == f"{os.getpid()}\n" and not result.stderr
        )
        print(f"{'PASS' if passed else 'FAIL'} fresh process.ppid startup")
        if not passed:
            failures.append("fresh process.ppid startup")
            print(result, file=sys.stderr)
    # Exercise native argument decoding independently of script-file decoding.
    # These values cover UTF-16 surrogate pairs and Windows CRT quote/backslash rules.
    unicode_args = [
        "é文🪲",
        "",
        "hello world",
        '"quoted"',
        "back\\slash\\",
        "space and trailing\\",
        'quote\\"inside',
        "\t",
        "e\u0301",
    ]
    unicode_source = "console.log('文🪲');console.log(JSON.stringify(process.argv.slice(1)));"
    expected_unicode = (
        "文🪲\n" + json.dumps(unicode_args, ensure_ascii=False, separators=(",", ":")) + "\n"
    )
    for executable in [args.node, args.mite]:
        result = run(executable, "-e", unicode_source, *unicode_args)
        passed = result.returncode == 0 and result.stdout == expected_unicode and not result.stderr
        print(
            f"{'PASS' if passed else 'FAIL'} Unicode eval/quoted arguments ({Path(executable).name})"
        )
        if not passed:
            failures.append("Unicode eval/quoted arguments")
            print(result, file=sys.stderr)
    if os.name == "nt":
        result = run(args.mite, "-e", "console.log('UNREACHABLE')", "\ud800")
        passed = (
            result.returncode == 9
            and not result.stdout
            and "cannot encode command-line argument as UTF-8" in result.stderr
        )
        print(f"{'PASS' if passed else 'FAIL'} invalid Windows UTF-16 argument rejects explicitly")
        if not passed:
            failures.append("invalid Windows UTF-16 argument")
            print(result, file=sys.stderr)
    for name, extra, status, output in cases:
        node = run(args.node, fixtures / name, *extra)
        mite = run(args.mite, fixtures / name, *extra)
        passed = (
            node.returncode == mite.returncode == status
            and node.stdout == mite.stdout == output
            and (
                (node.stderr == mite.stderr == ("diagnostic 42\n" if name == "stderr.js" else ""))
                if status == 0
                else bool(node.stderr and mite.stderr)
            )
        )
        marker = {
            "throw.js": "fixture failed",
            "unhandled-rejection.js": "unhandled fixture",
            "syntax-error.js": "SyntaxError",
            "microtask-throws.js": "microtask fixture",
            "timer-throws.js": "timer fixture",
            "timer-rejection.js": "timer rejection fixture",
        }.get(name)
        if marker:
            passed = passed and marker in node.stderr and marker in mite.stderr
        print(f"{'PASS' if passed else 'FAIL'} {name}")
        if not passed:
            failures.append(name)
            print(f"Node: {node}\nMite: {mite}", file=sys.stderr)
    for command, expected in [
        (["-e", "console.log(6 * 7)"], 0),
        (["--version"], 0),
        (["--help"], 0),
        (["--invalid"], 9),
        (["-e"], 9),
        ([fixtures / "missing.js"], 1),
        ([fixtures], 1),
    ]:
        result = run(args.mite, *command)
        passed = result.returncode == expected
        if command[0] == "-e" and len(command) > 1:
            passed = passed and result.stdout == "42\n"
        print(f"{'PASS' if passed else 'FAIL'} CLI {command[0]}")
        if not passed:
            failures.append(str(command))
            print(result, file=sys.stderr)
    return int(bool(failures))


if __name__ == "__main__":
    sys.exit(main())
