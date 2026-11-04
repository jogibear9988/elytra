#!/usr/bin/env python3
"""Compare real module graphs, not transformed source, in Node and Mite."""
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
    fixtures = Path(__file__).resolve().parents[1] / 'mite/tests/modules'
    cases = [
        ('json.mjs', 0, 'JSON module Grüße\ntrue true\ntrue 2 1\n', None),
        ('json-errors.mjs', 0, 'TypeError ERR_IMPORT_ATTRIBUTE_MISSING\n'
         'TypeError ERR_IMPORT_ATTRIBUTE_UNSUPPORTED\nTypeError ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE\n'
         'TypeError ERR_IMPORT_ATTRIBUTE_UNSUPPORTED\nSyntaxError\nhandled\n', None),
        ('relative.mjs', 0, 'module 1\n2 2\ntrue\nundefined undefined undefined\n', None),
        ('cache.mjs', 0, 'true false false\n1 2 3\n', None),
        ('meta.mjs', 0, 'true\ntrue\ntrue\ntrue false\ntrue\n', None),
        ('await.mjs', 0, 'before\nafter\n', None),
        ('unsettled.mjs', 13, 'before\n', None),
        ('await-reject.mjs', 1, '', 'module rejection fixture'),
        ('cycle-a.mjs', 0, 'AB\n', None),
        ('encoded.mjs', 0, 'true\nGrüße\n', None),
        ('errors.mjs', 0, 'ERR_MODULE_NOT_FOUND\nERR_UNSUPPORTED_DIR_IMPORT\n'
         'ERR_MODULE_NOT_FOUND\nERR_INVALID_MODULE_SPECIFIER\nERR_UNKNOWN_FILE_EXTENSION\n'
         'ERR_UNSUPPORTED_ESM_URL_SCHEME\nERR_INVALID_FILE_URL_HOST\n', None),
        ('dynamic.js', 0, 'before\nafter\nmodule\n', None),
        ('typed/main.js', 0, 'module true\n', None),
        ('syntax-error.mjs', 1, '', 'SyntaxError'),
    ]
    failures = 0
    for name, status, output, marker in cases:
        node = run(args.node, fixtures / name)
        mite = run(args.mite, fixtures / name)
        passed = (node.returncode == mite.returncode == status
                  and node.stdout == mite.stdout == output
                  and (node.stderr == mite.stderr == '' if status == 0
                       else bool(node.stderr and mite.stderr)))
        if marker:
            passed = passed and marker in node.stderr and marker in mite.stderr
        print(f"{'PASS' if passed else 'FAIL'} modules/{name}")
        if not passed:
            failures += 1
            print(f'Node: {node}\nMite: {mite}', file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
