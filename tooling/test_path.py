#!/usr/bin/env python3
"""Compare POSIX and Windows lexical paths with Node, on the current host."""
import argparse
import itertools
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite', required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    args = parser.parse_args()
    if not args.node:
        parser.error('Node is required for differential tests')
    paths = ['', '.', '..', '...', '.name', '.name.ext', 'a.', 'a..', 'a/b', 'a//b/',
             '/', '//', '///', '//foo', '///foo', '/foo//bar/', '/../../a', './', '../',
             'foo/../C:/bar', 'foo/../:', 'é/文/../x', 'a\\b', '\\', '\\\\',
             'C:', 'C:foo', 'C:/', 'C:/a/../b/', 'C:\\a\\b.txt', 'c:\\A\\b',
             '\\\\server\\share', '\\\\server\\share\\', '\\\\server\\share\\a\\b',
             '//server//share/a', '\\\\?\\C:\\a', '\\\\.\\pipe\\name', 'a:b', 'x:y/z', 'a/../:x', 'a/../1:foo', 'a/../é:foo', 'a/../x:foo',
             'a/abc:/z', 'a/abc:def', 'a/:x', 'C:////']
    # Cartesian dot/separator cases catch distinctions between normalize and raw parsing.
    paths += [a + s + b for a, s, b in itertools.product(
        ['a', '.', '..', '/', 'C:', '//host/share'], ['/', '//', '\\'], ['b', '.', '..', ''])]
    cases = []
    for style in ['posix', 'win32']:
        for path in paths:
            for method in ['normalize', 'isAbsolute', 'dirname', 'basename', 'extname', 'parse', 'toNamespacedPath']:
                cases.append([style, method, [path]])
            for suffix in ['', 'foo', 'b', '.txt', path]:
                cases.append([style, 'basename', [path, suffix]])
        for a, b in itertools.product(['', '.', '..', '/', '//', 'a', '/a/b', 'C:', 'C:/a', 'D:/b', '//host/share/a', '//host/other/b', '//other/share/b'], repeat=2):
            for method in ['join', 'resolve', 'relative']:
                cases.append([style, method, [a, b]])
        for value in [{}, {'root': '/', 'base': 'x'}, {'dir': '/', 'base': 'x'},
                      {'dir': 'a/', 'name': 'x', 'ext': 'txt'}, {'base': 42}, {'name': 'x', 'ext': 5},
                      {'root': 'C:\\', 'name': '.x', 'ext': '.txt'}]:
            cases.append([style, 'format', [value]])
        for method in ['normalize', 'isAbsolute', 'dirname', 'basename', 'extname', 'parse', 'join', 'resolve', 'relative', 'format']:
            for value in [None, 42, [], {}]:
                cases.append([style, method, [value, 'x'] if method == 'relative' else [value]])
        cases += [[style, 'resolve', [None, '/absolute']], [style, 'join', ['//', 'server', 'share']]]
    source = "const path = require('node:path');\nconst cases = " + json.dumps(cases, ensure_ascii=False) + ";\n" + '''
for (const [style, method, args] of cases) {
    try { console.log(JSON.stringify({ value: path[style][method](...args) })); }
    catch (error) { console.log(JSON.stringify({ error: error.name, code: error.code })); }
}
'''
    with tempfile.TemporaryDirectory(prefix='mite-path-') as directory:
        fixture = Path(directory) / 'paths.cjs'
        fixture.write_text(source)
        outputs = []
        for executable in [args.node, args.mite]:
            result = subprocess.run([executable, str(fixture)], capture_output=True, text=True, timeout=30)
            if result.returncode or result.stderr:
                print(result)
                return 1
            outputs.append([json.loads(line) for line in result.stdout.splitlines()])
        failures = 0
        for case, node, mite in zip(cases, *outputs):
            if node != mite:
                failures += 1
                if failures <= 30:
                    print(f'FAIL {case!r}: Node={node!r} Mite={mite!r}')
        if any(len(output) != len(cases) for output in outputs):
            failures += 1
            print('FAIL missing output rows')
        print(f'{len(cases)} lexical path comparisons, {failures} differences')
        identity = Path(directory) / 'identity.mjs'
        identity.write_text('''
import path, { posix, win32, join } from 'node:path';
import posixDefault from 'path/posix';
import winDefault from 'node:path/win32';
import { resolve } from 'node:path/posix';
import process from 'node:process';
console.log(path === (process.platform === 'win32' ? win32 : posix));
console.log(posix === posixDefault, win32 === winDefault, posix.win32 === win32, win32.posix === posix);
console.log(join === path.join, resolve === posix.resolve);
const old = process.cwd();
process.chdir('..');
console.log(posix.resolve('child') === posix.join(process.cwd().replaceAll('\\\\', '/'), 'child'));
process.chdir(old);
''')
        results = [subprocess.run([exe, str(identity)], capture_output=True, text=True, timeout=15) for exe in [args.node, args.mite]]
        expected = 'true\ntrue true true true\ntrue true\ntrue\n'
        passed = all(r.returncode == 0 and r.stdout == expected and not r.stderr for r in results)
        print(f'{"PASS" if passed else "FAIL"} builtin path identities and live cwd')
        if not passed:
            print(results)
        return int(bool(failures) or not passed)


if __name__ == '__main__':
    raise SystemExit(main())
