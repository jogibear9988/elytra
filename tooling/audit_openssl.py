#!/usr/bin/env python3
"""Measure OpenSSL global definitions in a physical macOS app's native binaries."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import subprocess
import sys

PROVIDER = 'libfirefly-openssl.dylib'


def symbols(path):
    result = subprocess.run(['nm', '-mU', str(path)], check=True, capture_output=True, text=True)
    # Mach-O private externals become local definitions in a dylib; they are not
    # its exported ABI. Exclude them on both sides instead of waiving symbol names.
    return {match[1] for line in result.stdout.splitlines()
            if 'private external' not in line and (match := re.search(r'\sexternal\s+(_\S+)$', line))}


def audit(app, build, require_shared=False):
    from package_macos import cache_value, dependencies
    installed = Path(cache_value(build, 'OPENSSL_CRYPTO_LIBRARY'))
    ssl = Path(cache_value(build, 'OPENSSL_SSL_LIBRARY'))
    if not installed.is_file() or not ssl.is_file():
        raise ValueError('Build must identify both pinned OpenSSL archives')
    archives = [installed, ssl]
    archive_symbols = set().union(*(symbols(file) for file in archives))
    contents = app / 'Contents'
    binaries = [p for folder in ['MacOS', 'lib'] for p in (contents / folder).iterdir()
                if p.is_file() and not p.is_symlink()]
    hits = {}
    counts = {}
    consumers = []
    provider_symbols = set()
    for binary in sorted(binaries):
        definitions = symbols(binary) & archive_symbols
        if definitions:
            hits[str(binary.relative_to(contents))] = len(definitions)
            for name in definitions:
                counts[name] = counts.get(name, 0) + 1
        if binary.name == PROVIDER:
            provider_symbols = definitions
        if any(Path(name).name == PROVIDER for name in dependencies(binary)):
            consumers.append(str(binary.relative_to(contents)))
    if require_shared:
        expected = contents / 'lib' / PROVIDER
        if not expected.is_file() or expected.is_symlink():
            raise ValueError('Missing physical shared provider')
        if set(hits) != {'lib/' + PROVIDER}:
            raise ValueError('OpenSSL definitions remain outside the provider: ' + str(hits))
        # Check the complete input API, not just functions called in smoke tests.
        if provider_symbols != archive_symbols:
            raise ValueError('Provider does not define the complete pinned archive symbol set')
        if 'MacOS/firefly' not in consumers or 'lib/liblagom-crypto.0.1.0.dylib' not in consumers:
            raise ValueError('Main/engine consumers do not link the explicit provider')
    return {
        'app': str(app), 'rendererBuild': str(build),
        'archives': [{'name': p.name, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in archives],
        'globalDefinitionsByBinary': hits,
        'uniqueOpenSSLSymbols': len(counts),
        'duplicatedOpenSSLSymbols': sum(count > 1 for count in counts.values()),
        'extraSymbolDefinitions': sum(count - 1 for count in counts.values()),
        'providerBytes': (contents / 'lib' / PROVIDER).stat().st_size if (contents / 'lib' / PROVIDER).is_file() else None,
        'providerConsumers': consumers,
        'requiredSharedProvider': require_shared,
        'measurement': 'External Mach-O definitions excluding private externals; not a byte-saving estimate',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', required=True, type=Path)
    parser.add_argument('--renderer-build', default=Path('build/renderer'), type=Path)
    parser.add_argument('--require-shared', action='store_true')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('Native macOS Mach-O audit only')
    result = audit(args.app.resolve(), args.renderer_build.resolve(), args.require_shared)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS OpenSSL audit:', result['duplicatedOpenSSLSymbols'], 'duplicated definitions;',
          len(result['providerConsumers']), 'explicit provider consumers')


if __name__ == '__main__':
    main()
