#!/usr/bin/env python3
"""Run focused fixtures against real packages installed from the committed npm lock."""
import argparse
import json
from pathlib import Path
import shutil
import sys
import tempfile
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
    def compare(name, expected, *fixture_args):
        node = run(args.node, root / 'cases' / name, *fixture_args)
        mite = run(args.mite, root / 'cases' / name, *fixture_args)
        passed = (node.returncode == mite.returncode == 0 and node.stdout == mite.stdout == expected
                  and node.stderr == mite.stderr == '')
        print(f"{'PASS' if passed else 'FAIL'} npm/{name}")
        if not passed:
            print(f'Node: {node}\nMite: {mite}', file=sys.stderr)
        return int(not passed)

    failures = sum(compare(name, expected) for name, expected in cases)
    with tempfile.TemporaryDirectory(prefix='mite-npm-stat-') as directory:
        fixture = Path(directory)
        (fixture / 'plain.txt').write_text('stat fixture')
        (fixture / 'folder').mkdir()
        for name, target in [('link', 'plain.txt'), ('dir-link', 'folder'), ('broken', 'missing')]:
            (fixture / name).symlink_to(target)
        failures += compare('fs-stat.cjs',
            '[[true,false,false,"number"],[false,true,false,"number"],[true,false,false,"number"],'
            '[false,true,false,"number"],["ENOENT","stat"],["ENOENT","lstat"]]\n'
            '[false,false,true,"number"]\n[true,false,true,"number"]\n[false,false,true,"number"]\n'
            '12 lstat\n2.0.5 true\n', fixture)
        failures += compare('fs-stat.mjs',
            'scheduled\n[true,false,false,"number"]\n[false,true,false,"number"]\n'
            '[true,false,false,"number"]\n[false,false,true,"number"]\n[true,false,true,"number"]\n'
            '["ENOENT","stat"]\n[false,false,true,"number"]\n["ENOENT","lstat"]\n12 true\n', fixture)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
