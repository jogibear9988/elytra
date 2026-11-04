#!/usr/bin/env python3
"""Compare real native filesystem workflows in isolated Node/Mite directories."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def execute(executable, fixture, cwd):
    command = [executable, str(fixture)]
    if fixture.name == 'descriptors.cjs':
        with (cwd / 'fixture.txt').open('rb') as source, (cwd / 'output.txt').open('wb') as target:
            descriptors = (source.fileno(), target.fileno())
            return subprocess.run([*command, *map(str, descriptors)], pass_fds=descriptors,
                                  cwd=cwd, capture_output=True, text=True, timeout=30)
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=30)


def prepare(cwd, fifo=False):
    cwd.mkdir()
    (cwd / 'fixture.txt').write_text('fixture')
    (cwd / 'link').symlink_to('fixture.txt')
    if fifo:
        os.mkfifo(cwd / 'fifo')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite', required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    args = parser.parse_args()
    if not args.node:
        parser.error('Node is required for differential tests')
    fixtures = Path(__file__).resolve().parents[1] / 'mite/tests/fs'
    failures = 0
    cases = [('sync.mjs', 0, None), ('async.mjs', 0, None), ('validation.cjs', 0, None),
             ('concurrent.mjs', 0, None), ('callback-throw.cjs', 1, 'fs callback fixture'),
             ('pending-throw.cjs', 1, 'pending fs fixture'), ('identity.cjs', 0, None), ('scalar.cjs', 0, None),
             ('scalar-async.mjs', 0, None), ('scalar-validation.cjs', 0, None),
             ('filehandle.mjs', 0, None), ('filehandle-close.mjs', 0, None),
             ('filehandle-validation.mjs', 0, None)]
    if hasattr(os, 'mkfifo'):
        cases.extend([('nonblocking.mjs', 0, None), ('descriptors.cjs', 0, None)])
    else:
        print('UNVERIFIED native named-pipe/descriptor checks on this platform')
    for name, status, marker in cases:
        with tempfile.TemporaryDirectory(prefix='mite-fs-') as directory:
            results, contents = [], []
            for index, executable in enumerate([args.node, args.mite]):
                cwd = Path(directory) / str(index)
                prepare(cwd, name == 'nonblocking.mjs')
                try:
                    results.append(execute(executable, fixtures / name, cwd))
                except subprocess.TimeoutExpired:
                    print(f'FAIL filesystem/{name}: runtime {index} timed out')
                    failures += 1
                    break
                contents.append({str(p.relative_to(cwd)): p.read_bytes() for p in cwd.rglob('*')
                                 if p.is_file() and not p.is_symlink()})
            if len(results) != 2:
                continue
            passed = (all(r.returncode == status and (marker in r.stderr if marker else not r.stderr) for r in results)
                      and results[0].stdout == results[1].stdout and contents[0] == contents[1])
            if marker:
                passed = passed and 'unexpected late callback' not in results[1].stdout
            print(f'{"PASS" if passed else "FAIL"} filesystem/{name}')
            if not passed:
                failures += 1
                print('Node:', results[0])
                print('Mite:', results[1])
                if contents[0] != contents[1]:
                    print('Final file contents differ')
    # Deliberate policy: fatal exceptions terminate even with blocked native work.
    # Node v24.15.0 itself hung in the pending FIFO experiment; these are Mite-only.
    if hasattr(os, 'mkfifo'):
        for name, marker in [('pending-fifo-throw.cjs', 'pending fifo fixture'),
                             ('callback-fifo-throw.cjs', 'callback fifo fixture')]:
            with tempfile.TemporaryDirectory(prefix='mite-fs-exit-') as directory:
                cwd = Path(directory) / 'work'
                prepare(cwd, True)
                try:
                    result = execute(args.mite, fixtures / name, cwd)
                    passed = result.returncode == 1 and not result.stdout and marker in result.stderr
                except subprocess.TimeoutExpired:
                    passed = False
                print(f'{"PASS" if passed else "FAIL"} explicit fatal-I/O exit policy/{name}')
                failures += int(not passed)
    policy = subprocess.run([args.mite, str(fixtures / 'unsupported.mjs')], capture_output=True, text=True, timeout=15)
    passed = policy.returncode == 0 and policy.stdout == 'ERR_MITE_UNSUPPORTED_API\n' * 4 and not policy.stderr
    print(f'{"PASS" if passed else "FAIL"} explicit unsupported filesystem modes')
    if not passed:
        failures += 1
        print(policy)
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
