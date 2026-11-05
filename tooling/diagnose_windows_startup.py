#!/usr/bin/env python3
"""Exercise the native fault reporter, then retain the real VM startup result."""
import argparse
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Requires native Windows; no alternate runtime is used')
    reports = []
    result = None
    for arguments in [['--diagnostic-crash'], ['-e', 'console.log("MITE_NATIVE_STARTUP_OK")']]:
        result = subprocess.run([str(args.probe.resolve()), *arguments], capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=30)
        reports.append(f'ARGS {arguments!r}\nSTATUS {result.returncode}\n{result.stdout}\n{result.stderr}')
        args.output.write_text('\n'.join(reports), encoding='utf-8')
        print(reports[-1], flush=True)
        if arguments[0] == '--diagnostic-crash':
            if result.returncode != 0xc0000005 or 'MITE_NATIVE_EXCEPTION' not in result.stderr or 'MITE_NATIVE_FRAME' not in result.stderr:
                raise SystemExit('Native crash reporter negative control failed')
    if result.returncode or result.stdout.strip() != 'MITE_NATIVE_STARTUP_OK':
        raise SystemExit('Native VM startup failed; inspect the symbolized native report')


if __name__ == '__main__':
    main()
