#!/usr/bin/env python3
"""Verify the generated C++ commands really use the requested final size flag."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex


def check(build):
    cache = {}
    for line in (build / 'CMakeCache.txt').read_text(encoding='utf-8').splitlines():
        if not line.startswith(('#', '//')) and ':' in line and '=' in line:
            name, value = line.split('=', 1)
            cache[name.split(':', 1)[0]] = value
    expected = cache.get('ELYTRA_CPP_SIZE_FLAG')
    if cache.get('ELYTRA_OPTIMIZE_SIZE') != 'ON' or expected not in ['-Oz', '/O1']:
        raise ValueError('Requested size profile is not configured')
    count = 0
    for record in json.loads((build / 'compile_commands.json').read_text(encoding='utf-8')):
        file = Path(record['file'])
        if file.suffix not in ['.cpp', '.cc', '.cxx', '.mm', '.C']:
            continue
        arguments = record.get('arguments')
        if arguments is None:
            arguments = shlex.split(record['command'], posix=os.name != 'nt')
        levels = [arg.strip('"') for arg in arguments
                  if re.fullmatch(r'-O(?:[0-3sgz]|fast)?|/O[d12x]', arg.strip('"'))]
        if not levels or levels[-1] != expected:
            raise ValueError(f'Size flag overridden or absent for {file}: {levels}')
        count += 1
    if not count:
        raise ValueError('No generated C++ compile commands checked')
    print(f'PASS final size flag {expected} in {count} generated C++ commands')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    check(parser.parse_args().build)
