#!/usr/bin/env python3
"""Run focused fixtures against real packages installed from the committed npm lock."""
import argparse
import json
from pathlib import Path
import shutil
import sys
from test_runtime import run


INSTALL = 'npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/npm-fixtures'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite', required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    args = parser.parse_args()
    if not args.node:
        parser.error('Node is required for differential tests')
    root = Path(__file__).resolve().parent / 'npm-fixtures'
    locked = json.loads((root / 'package-lock.json').read_text())['packages']
    for location, record in locked.items():
        if not location:
            continue
        installed = root / location
        manifest = installed / 'package.json'
        if not manifest.is_file():
            parser.error(f'Pinned npm fixtures are missing. Run: {INSTALL}')
        actual = json.loads(manifest.read_text())
        if actual['version'] != record['version']:
            parser.error(f'{location} version differs from the committed lock. Run: {INSTALL}')
        if not any(path.is_file() and path.name.lower() in {'license', 'license.md', 'license.txt'} for path in installed.iterdir()):
            parser.error(f'{location} is missing its upstream license notice. Run: {INSTALL}')
    cases = [
        ('once.cjs', '1 true 42 preserved\n5\nError true\n'),
        ('once-exports.cjs', 'strict\nundefined\nTypeError\n'),
        ('escape.mjs', 'true true\nTypeError Expected a string\nCJS imported by ESM\n'),
        ('emitter.cjs', 'context:1,once:1,context:2\n1 true true\n'),
        ('emitter.mjs', 'true true eventemitter3 5.0.1\nnative timer\n'),
        ('dependencies.cjs', 'true function\n1.4.0 1.0.2\ntrue true\n'),
    ]
    failures = 0
    for name, expected in cases:
        node = run(args.node, root / 'cases' / name)
        mite = run(args.mite, root / 'cases' / name)
        passed = (node.returncode == mite.returncode == 0 and node.stdout == mite.stdout == expected
                  and node.stderr == mite.stderr == '')
        print(f"{'PASS' if passed else 'FAIL'} npm/{name}")
        if not passed:
            failures += 1
            print(f'Node: {node}\nMite: {mite}', file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
