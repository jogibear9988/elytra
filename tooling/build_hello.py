#!/usr/bin/env python3
"""Build the ESM-authored hello-world without adding a Node runtime dependency."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'examples/firefly-hello-world'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/firefly-hello-world')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        parser.error('Output already exists; choose a fresh build directory')
    npm = shutil.which('npm.cmd' if os.name == 'nt' else 'npm')
    node = shutil.which('node')
    if not npm or not node:
        parser.error('Node/npm are build-time prerequisites')
    subprocess.run([npm, 'ci', '--ignore-scripts', '--no-audit', '--no-fund', '--prefix', str(ROOT / 'tooling/runtime-js')], check=True)
    output.mkdir(parents=True)
    for name in ['main.mjs', 'index.html']:
        shutil.copy2(SOURCE / name, output / name)
    subprocess.run([node, str(ROOT / 'tooling/bundle_preload.mjs'), str(SOURCE / 'preload.mjs'), str(output / 'preload.cjs')], check=True)
    manifest = {
        'mainFormat': 'esm', 'preloadSourceFormat': 'esm', 'preloadRuntimeFormat': 'commonjs',
        'sources': {name: hashlib.sha256((SOURCE / name).read_bytes()).hexdigest() for name in ['main.mjs', 'preload.mjs', 'index.html']},
        'bundlerLockSha256': hashlib.sha256((ROOT / 'tooling/runtime-js/package-lock.json').read_bytes()).hexdigest()
    }
    (output / 'BUILD.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Built ESM-authored Firefly hello-world:', output)


if __name__ == '__main__':
    main()
