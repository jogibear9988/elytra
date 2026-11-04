#!/usr/bin/env python3
"""Reject Ladybird source changes and mismatches between manifest, gitlink and HEAD."""
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def check(root=ROOT):
    revision = json.loads((root / 'vendor/ladybird.json').read_text())['revision']
    if not isinstance(revision, str) or not re.fullmatch('[0-9a-f]{40}', revision):
        raise ValueError('Ladybird manifest must contain a full commit ID')
    entry = git(root, 'ls-files', '--stage', '--', 'vendor/ladybird').split()
    if len(entry) != 4 or entry[0] != '160000' or entry[2] != '0' or entry[1] != revision:
        raise ValueError('Ladybird manifest and indexed submodule pin differ; review and stage both together')
    upstream = root / 'vendor/ladybird'
    if Path(git(upstream, 'rev-parse', '--show-toplevel')).resolve() != upstream.resolve():
        raise ValueError('Initialize Ladybird: git submodule update --init --recursive')
    if git(upstream, 'rev-parse', 'HEAD') != revision:
        raise ValueError('Ladybird checkout differs from the pin; initialize or finish the upstream update')
    if git(upstream, 'status', '--porcelain=v1', '--untracked-files=all', '--ignore-submodules=none'):
        raise ValueError('Ladybird source tree is modified; keep integration changes outside vendor/ladybird')
    return revision


def main():
    try:
        print(f'Unmodified Ladybird pin verified: {check()}')
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(f'Upstream check failed: {error}') from error


if __name__ == '__main__':
    main()
