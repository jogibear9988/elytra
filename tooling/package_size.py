#!/usr/bin/env python3
"""Measure physical bundle contents without counting library symlink aliases twice."""
from pathlib import Path

def tree_bytes(root):
    return sum(file.stat().st_size for file in Path(root).rglob('*') if file.is_file() and not file.is_symlink())

def bundle_size(bundle):
    contents=Path(bundle)/'Contents'
    groups={name:tree_bytes(contents/path) for name,path in {
        'executables':'MacOS','libraries':'lib','application':'Resources/app',
        'notices':'Resources/Licenses','resources':'Resources','signature':'_CodeSignature'}.items()}
    groups['resources']-=groups['application']+groups['notices']
    return {'bytes':tree_bytes(bundle),'components':groups}
