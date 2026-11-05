#!/usr/bin/env python3
"""Stage actual resource files in the prototype helper bundle, keeping its sandbox."""
import argparse
from pathlib import Path
import shutil

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',required=True,type=Path)
parser.add_argument('--destination',required=True,type=Path)
args=parser.parse_args()
source=args.source.resolve()
destination=args.destination.absolute()
if not source.is_dir():parser.error('Source resources are missing')
if destination.is_symlink():destination.unlink()
# The previous symlink layout made resource ancestor metadata inaccessible to
# sandboxed helpers. Physical files keep access within the helper's own bundle.
shutil.copytree(source,destination,dirs_exist_ok=True)
