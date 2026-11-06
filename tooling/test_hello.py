#!/usr/bin/env python3
"""Run the ESM-authored sample and its bundled preload in Firefly and Electron."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
from test_firefly import execute

ROOT = Path(__file__).resolve().parents[1]


def check(result):
    assert result.returncode == 0 and 'HELLO_ESM_PRELOAD_OK' in result.stdout, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firefly', required=True, type=Path)
    parser.add_argument('--electron', required=True, type=Path)
    parser.add_argument('--app', required=True, type=Path)
    args = parser.parse_args()
    app = args.app.resolve()
    check(execute(args.firefly.resolve(), app / 'main.mjs', extra_args=['--smoke']))
    # Same authored module sources; only the explicitly imported desktop module
    # changes in the reference fixture. A real Electron window must initialize.
    with tempfile.TemporaryDirectory(prefix='hello-esm-reference-') as directory:
        reference = Path(directory).resolve() / 'app é文'
        reference.mkdir()
        main_source = (app / 'main.mjs').read_text()
        preload_source = (ROOT / 'examples/firefly-hello-world/preload.mjs').read_text()
        assert main_source.count("from 'firefly'") == preload_source.count("from 'firefly'") == 1
        (reference / 'main.mjs').write_text(main_source.replace("from 'firefly'", "from 'electron'"))
        (reference / 'preload.mjs').write_text(preload_source.replace("from 'firefly'", "from 'electron'"))
        shutil.copy2(app / 'index.html', reference / 'index.html')
        subprocess.run(['node', str(ROOT / 'tooling/bundle_preload.mjs'), str(reference / 'preload.mjs'), str(reference / 'preload.cjs')], check=True)
        check(execute(args.electron.resolve(), reference / 'main.mjs', 'electron', extra_args=['--smoke']))
    print('PASS modern ESM main/page/preload sources, bundled sandbox preload and real Electron/Firefly app boot')


if __name__ == '__main__':
    main()
