#!/usr/bin/env python3
"""Prove wire data survives complete VM/process teardown, including bad remote input."""
import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', required=True, type=Path)
    args = parser.parse_args()
    command = str(args.probe.resolve())
    checks = subprocess.run([command, 'checks'], capture_output=True, timeout=30, check=True)
    assert b'PASS wire graph round trip' in checks.stdout, checks
    print(checks.stdout.decode().strip())
    encoded = subprocess.run([command, 'encode'], capture_output=True, timeout=15, check=True).stdout
    assert encoded and len(encoded) <= 1024 * 1024
    # run() waited for the producer to exit: no VM/heap references can survive it.
    decoded = subprocess.run([command, 'decode'], input=encoded, capture_output=True, timeout=15, check=True).stdout
    assert decoded == encoded, 'Fresh VM data/re-capture changed graph semantics'
    for bad in [b'', b'{}', b'[1,[[7,0]]]', b'[1,[[5,false,[[[],99,true]]]]]',
                b'[' * 9 + b'0' + b']' * 9, b' ' * (1024 * 1024 + 1)]:
        result = subprocess.run([command, 'decode'], input=bad, capture_output=True, timeout=15)
        assert result.returncode == 1 and not result.stdout, (result.returncode, result.stdout, result.stderr)
    assert subprocess.run([command, 'decode'], input=encoded, capture_output=True, timeout=15, check=True).stdout == encoded
    print('PASS separate process/VM wire transfer after producer exit, fresh prototypes/GC/cycles/UTF-16/IEEE values and bounded malformed-input rejection')


if __name__ == '__main__':
    main()
