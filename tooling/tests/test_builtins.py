#!/usr/bin/env python3
"""Compare supported builtin behavior with Node; separate known unsupported modes."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
from pathlib import Path
import shutil
import sys
from test_runtime import run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for differential tests")
    fixtures = Path(__file__).resolve().parents[2] / "mite/tests/builtins"
    cases = [
        ("process-global.cjs", 0, "true true\n", None),
        (
            "process-info.mjs",
            0,
            "true true true true true\ntrue true true true\ntrue true\ntrue true\ntrue true\n",
            None,
        ),
        (
            "process-env.cjs",
            0,
            "true false\n42 string true true\ntrue true\ntrue true true\ndefined\n"
            "TypeError ERR_INVALID_OBJECT_DEFINE_PROPERTY\ntrue true\n",
            None,
        ),
        (
            "process-cwd.cjs",
            0,
            "true\ntrue\nENOENT true chdir true mite-working-directory-missing-fixture\nTypeError ERR_INVALID_ARG_TYPE\n",
            None,
        ),
        (
            "process-exit-code.cjs",
            0,
            "undefined undefined\n" * 2
            + "number 7\nnumber 0\nnumber -2147483648\n"
            + "TypeError ERR_INVALID_ARG_TYPE\n" * 2
            + "RangeError ERR_OUT_OF_RANGE\n" * 3,
            None,
        ),
        ("process-drain.cjs", 7, "drained\n", None),
        ("process-exit-now.cjs", 6, "before\n", None),
        ("process-unsettled.mjs", 5, "pending\n", None),
        ("events-basic.cjs", 0, "true true\nfalse\ntrue 5\ntrue 1\n", None),
        (
            "events-order.cjs",
            0,
            "first,second,first,late,prependOnce,prepend,normal,prepend,normal,once\n",
            None,
        ),
        ("events-raw.cjs", 0, "true true\ntrue\n1 0\n", None),
        ("events-hooks.cjs", 0, "new:one,new:two,removed:two,removed:one\n0\n", None),
        (
            "events-errors.cjs",
            0,
            "monitor true\nhandled true\ntrue\nmonitored\ntrue\nERR_UNHANDLED_ERROR context\ntrue false\n",
            None,
        ),
        ("events-symbols.cjs", 0, "text,Symbol(event)\n2 2\ntrue\ntrue 1\ntext\n", None),
        (
            "events-max.cjs",
            0,
            "10\n20\nundefined 3 3\n"
            + "TypeError ERR_INVALID_ARG_TYPE\n" * 3
            + "RangeError ERR_OUT_OF_RANGE\n" * 2
            + "true Infinity\n"
            + "TypeError ERR_INVALID_ARG_TYPE\n" * 5,
            None,
        ),
        ("events-promise.mjs", 0, '[1,2] 0 0\nfailure 0\n["payload"]\n', None),
        (
            "events-identity.mjs",
            0,
            "true true true true\ntrue\ntrue\nTypeError ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE\n",
            None,
        ),
        (
            "events-cache.cjs",
            0,
            "true true\nevents node:events true\nERR_UNKNOWN_BUILTIN_MODULE\nMODULE_NOT_FOUND\n",
            None,
        ),
        ("events-warning.cjs", 0, "2\n", "MaxListenersExceededWarning"),
        ("events-uncaught.cjs", 1, "", "emitter fixture"),
    ]
    failures = 0
    for name, status, output, marker in cases:
        node = run(args.node, fixtures / name)
        mite = run(args.mite, fixtures / name)
        passed = (
            node.returncode == mite.returncode == status and node.stdout == mite.stdout == output
        )
        if marker:
            passed = passed and marker in node.stderr and marker in mite.stderr
        else:
            passed = passed and node.stderr == mite.stderr == ""
        print(f"{'PASS' if passed else 'FAIL'} builtins/{name}")
        if not passed:
            failures += 1
            print(f"Node: {node}\nMite: {mite}", file=sys.stderr)
    unsupported = run(args.mite, fixtures / "events-unsupported.cjs")
    passed = (
        unsupported.returncode == 0
        and unsupported.stdout == "ERR_MITE_UNSUPPORTED_API\n" * 3
        and unsupported.stderr == ""
    )
    print(f"{'PASS' if passed else 'FAIL'} explicit unsupported EventEmitter modes")
    if not passed:
        failures += 1
        print(unsupported, file=sys.stderr)
    identity = run(args.mite, fixtures / "process-identity.cjs")
    passed = (
        identity.returncode == 0
        and identity.stdout == "mite v0.1.0 0.1.0 undefined\ntrue true\n"
        and identity.stderr == ""
    )
    print(f"{'PASS' if passed else 'FAIL'} explicit Mite runtime identity")
    if not passed:
        failures += 1
        print(identity, file=sys.stderr)
    return int(bool(failures))


if __name__ == "__main__":
    sys.exit(main())
