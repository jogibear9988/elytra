#!/usr/bin/env python3
"""Load actual Unicode entry/module/package paths and compare native metadata with Node."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite', required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    args = parser.parse_args()
    if not args.node:
        parser.error('Node is required')
    with tempfile.TemporaryDirectory(prefix='mite-native-paths-') as directory:
        root = Path(directory) / 'project é文🪲 #%'
        root.mkdir()

        def write(name, text):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding='utf-8')
            return path

        write('依存#.mjs', "export const message='ESM 文🪲';")
        write('依存#.cjs', "module.exports={message:'CJS 文🪲',filename:__filename,dirname:__dirname,search:module.paths[0]};")
        write('node_modules/demo-pkg/package.json', json.dumps({'main': './組.cjs'}, ensure_ascii=False))
        write('node_modules/demo-pkg/組.cjs', "module.exports={package:'pkg 文🪲',filename:__filename};")
        cjs = write('entrée 文🪲.cjs', r'''
const path=require('node:path'),dep=require('./依存#.cjs'),pkg=require('demo-pkg');
if(module.paths[0]!==path.join(__dirname,'node_modules'))throw Error('Native module.paths');
console.log(JSON.stringify({dep,pkg,filename:__filename,dirname:__dirname,entry:process.argv[1],resolved:require.resolve('./依存#.cjs')}));
''')
        esm = write('entrée 文🪲.mjs', r'''
import {message} from './依存%23.mjs';
import dep from './依存%23.cjs';
import pkg from 'demo-pkg';
import {fileURLToPath} from 'node:url';
if(fileURLToPath(import.meta.url)!==import.meta.filename)throw Error('Native import.meta path');
console.log(JSON.stringify({message,dep,pkg,filename:import.meta.filename,dirname:import.meta.dirname,entry:process.argv[1],url:import.meta.url,main:import.meta.main}));
''')
        for fixture in [cjs, esm]:
            observations = []
            for exe in [args.node, args.mite]:
                result = subprocess.run([exe, str(fixture)], capture_output=True, text=True, encoding='utf-8', timeout=20)
                assert result.returncode == 0 and not result.stderr, result
                observations.append(json.loads(result.stdout))
            assert observations[0] == observations[1], observations
            assert observations[1]['filename'] == str(fixture.resolve()), observations
            assert observations[1]['dirname'] == str(root.resolve()), observations
        # On Windows, UNC module loading remains explicit unsupported behavior.
        # Do not ask the Node reference to probe a remote SMB host for this test.
        if os.name == 'nt':
            policy = write('unc-policy.mjs', "try{await import('file://remote/test.mjs');throw Error('UNC accepted')}catch(e){if(e.code!=='ERR_MITE_UNSUPPORTED_API')throw e;console.log('UNC_UNSUPPORTED')}")
            result = subprocess.run([args.mite, str(policy)], capture_output=True, text=True, encoding='utf-8', timeout=10)
            assert result.returncode == 0 and not result.stderr and result.stdout.strip() == 'UNC_UNSUPPORTED', result
    print('PASS Unicode entry/ESM/CJS/package paths, escaped URL names and native filename/dirname/module.paths metadata against Node')


if __name__ == '__main__':
    main()
