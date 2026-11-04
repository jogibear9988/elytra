#!/usr/bin/env python3
"""Run shared fixtures in real Node and Mite; no engine substitution or output filtering."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys


def run(executable, *args):
    return subprocess.run([executable, *map(str, args)], capture_output=True,
                          text=True, encoding="utf-8", timeout=20)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for differential tests")
    fixtures = Path(__file__).resolve().parents[1] / "mite/tests/fixtures"
    cases = [
        ("language.js", [], 0, "[4,16]\n18446744073709551616\nGrüße 🪲\ntrue\n"),
        ("exception-shape.js", [], 0, "Error: explicit message\nTypeError: explicit message\nRangeError: explicit message\n"),
        ("promises.js", [], 0, "start\nend\nfirst\nsecond\nnested\n"),
        ("handled-rejection.js", [], 0, "handled\n"),
        ("awaited-rejection.js", [], 0, "awaited rejection handled\n"),
        ("arguments.js", ["hello world", "--flag", "🪲"], 0,
         '["hello world","--flag","🪲"]\n'),
        ("microtasks.js", [], 0, "start\nend\nmicrotask\npromise\nlast\nnested\n"),
        ("timers.js", [], 0, "start\nend\ntrue argument 42\nmicrotask\npromise\nsecond\n"),
        ("timer-cancellation.js", [], 0, "kept\n1 true\n2 true\n3 true\n"),
        ("timer-disposal.js", [], 0, "true true\nfunction\nclearTimeout clearInterval false\ndone\n"),
        ("timer-ref.js", [], 0, "true\ntrue false\ntrue true\n"),
        ("callback-validation.js", [], 0, "TypeError ERR_INVALID_ARG_TYPE\n" * 15 + "TypeError\n" * 2),
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
    for name, extra, status, output in cases:
        node = run(args.node, fixtures / name, *extra)
        mite = run(args.mite, fixtures / name, *extra)
        passed = (node.returncode == mite.returncode == status
                  and node.stdout == mite.stdout == output
                  and ((node.stderr == mite.stderr == ("diagnostic 42\n" if name == "stderr.js" else ""))
                       if status == 0 else bool(node.stderr and mite.stderr)))
        marker = {"throw.js": "fixture failed", "unhandled-rejection.js": "unhandled fixture",
                  "syntax-error.js": "SyntaxError", "microtask-throws.js": "microtask fixture",
                  "timer-throws.js": "timer fixture", "timer-rejection.js": "timer rejection fixture"}.get(name)
        if marker:
            passed = passed and marker in node.stderr and marker in mite.stderr
        print(f"{'PASS' if passed else 'FAIL'} {name}")
        if not passed:
            failures.append(name)
            print(f"Node: {node}\nMite: {mite}", file=sys.stderr)
    for command, expected in [(["-e", "console.log(6 * 7)"], 0),
                              (["--version"], 0), (["--help"], 0),
                              (["--invalid"], 9), (["-e"], 9),
                              ([fixtures / "missing.js"], 1), ([fixtures], 1)]:
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
