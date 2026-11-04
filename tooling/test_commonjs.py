#!/usr/bin/env python3
"""Compare CommonJS execution, caches and ESM interoperation with Node."""
import argparse
from pathlib import Path
import shutil
import sys
from test_runtime import run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite', required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    args = parser.parse_args()
    if not args.node:
        parser.error('Node is required for differential tests')
    fixtures = Path(__file__).resolve().parents[1] / 'mite/tests/commonjs'
    cases = [
        ('shebang.cjs', 'shebang true\n'),
        ('bom.cjs', 'BOM\n'),
        ('alias.cjs', '{"current":true}\n'),
        ('syntax.cjs', 'SyntaxError true\nSyntaxError true\nSyntaxError\n'),
        ('scope.cjs', 'true true true\n. false true\ntrue true\ntrue\n'),
        ('cache.cjs', 'true 1 true\ntrue true\n1\n2\n'),
        ('cycles.cjs', 'BA\n'),
        ('directories.cjs', 'package main\n42\npackage main\n'),
        ('retry.cjs', 'retry fixture 0 true\n2 1\n'),
        ('errors.cjs', 'TypeError ERR_INVALID_ARG_TYPE\n' * 4 + 'MODULE_NOT_FOUND 1 true\n' * 2),
        ('json.cjs', 'true 10\n'),
        ('interop.mjs', 'prior ESM\nCommonJS\nmain 42 true true\ntrue\n'),
        ('esm-first.mjs', 'true 1\n'),
        ('require-order.cjs', 'before\nCommonJS\nafter\nmicrotask\n'),
    ]
    failures = 0
    for name, output in cases:
        node = run(args.node, fixtures / name)
        mite = run(args.mite, fixtures / name)
        passed = (node.returncode == mite.returncode == 0
                  and node.stdout == mite.stdout == output and node.stderr == mite.stderr == '')
        print(f"{'PASS' if passed else 'FAIL'} commonjs/{name}")
        if not passed:
            failures += 1
            print(f'Node: {node}\nMite: {mite}', file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
