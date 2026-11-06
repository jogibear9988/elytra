#!/usr/bin/env python3
"""Require review when the upstream process-monitor implementation or ABI changes."""
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    'ProcessMonitor.cpp': '4753fabacb0aa6fbf8ccbffe01c633f0ecc69e12354f9be6384be9ca622ad1b0',
    'ProcessMonitor.h': '281f8db9182c201454d0e79cc67bf675ec056f25e94f063b00d8058a48946bef',
}


def main():
    for name, expected in EXPECTED.items():
        source = ROOT / 'vendor/ladybird/Libraries/LibWebView' / name
        if hashlib.sha256(source.read_bytes()).hexdigest() != expected:
            raise SystemExit(f'{name} changed; review/remove the owned selective process monitor')
    print('PASS pinned process-monitor source and ABI boundary')


if __name__ == '__main__':
    main()
