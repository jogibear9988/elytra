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
        root = Path(directory) / 'project é文🪲 ~[]#%'
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
import {fileURLToPath,pathToFileURL} from 'node:url';
if(fileURLToPath(import.meta.url)!==import.meta.filename)throw Error('Native import.meta path');
if(pathToFileURL(import.meta.filename).href!==import.meta.url)throw Error('Canonical module file URL');
console.log(JSON.stringify({message,dep,pkg,filename:import.meta.filename,dirname:import.meta.dirname,entry:process.argv[1],url:import.meta.url,main:import.meta.main}));
''')
        def compare(fixture):
            observations = []
            for exe in [args.node, args.mite]:
                result = subprocess.run([exe, str(fixture)], capture_output=True, text=True, encoding='utf-8', timeout=20)
                assert result.returncode == 0 and not result.stderr, result
                observations.append(json.loads(result.stdout))
            assert observations[0] == observations[1], observations
            # Python's Windows resolve expands 8.3 aliases; Node deliberately
            # preserves them. Keep the exact Node comparison above and verify
            # the actual file identity separately, without rewriting output.
            assert os.path.samefile(observations[1]['filename'], fixture), observations
            assert os.path.samefile(observations[1]['dirname'], root), observations

        compare(cjs)
        compare(esm)
        alias = Path(directory) / 'linked project'
        if os.name == 'nt':
            # Directory junctions do not require symlink/developer privileges.
            result = subprocess.run(['cmd', '/d', '/c', 'mklink', '/J', str(alias), str(root)], capture_output=True, timeout=10)
            assert result.returncode == 0, result
        else:
            alias.symlink_to(root, target_is_directory=True)
        try:
            compare(alias / cjs.name)
            compare(alias / esm.name)
            write('identity.cjs', 'module.exports={};')
            write('identity.mjs', 'export default {};')
            write('tilde~.mjs', 'export default {};')
            write('bracket[].mjs', 'export default {};')
            identity = write('link-cache.mjs', r'''
import a from './identity.mjs';
import b from '../linked%20project/identity.mjs';
import c from './identity.cjs';
import d from '../linked%20project/identity.cjs';
import raw from './tilde~.mjs';
import escaped from './tilde%7E.mjs';
import bracket from './bracket[].mjs';
import bracketEscaped from './bracket%5B%5D.mjs';
console.log(JSON.stringify({esm:a===b,cjs:c===d,tilde:raw===escaped,brackets:bracket===bracketEscaped}));
''')
            for exe in [args.node, args.mite]:
                result = subprocess.run([exe, str(identity)], capture_output=True, text=True, encoding='utf-8', timeout=20)
                assert result.returncode == 0 and not result.stderr, result
                assert json.loads(result.stdout) == {'esm': True, 'cjs': True, 'tilde': True, 'brackets': True}, result
            print('PASS linked entry metadata and shared ESM/CJS cache through directory link/junction')
        finally:
            if os.name == 'nt':
                alias.rmdir()
            else:
                alias.unlink()

        if os.name == 'nt':
            compare(cjs.with_name(cjs.name.replace('entrée', 'ENTRÉE')))
            compare(esm.with_name(esm.name.replace('entrée', 'ENTRÉE')))
            import ctypes
            from ctypes import wintypes
            short_path = ctypes.WinDLL('kernel32', use_last_error=True).GetShortPathNameW
            short_path.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD]
            short_path.restype = wintypes.DWORD
            size = short_path(str(root), None, 0)
            assert size, ctypes.get_last_error()
            buffer = ctypes.create_unicode_buffer(size)
            count = short_path(str(root), buffer, size)
            assert 0 < count < size, ctypes.get_last_error()
            short_root = Path(buffer.value)
            if str(short_root) != str(root):
                compare(short_root / cjs.name)
                compare(short_root / esm.name)
                print('PASS exact Node module metadata through native Windows short-name alias')
            else:
                print('Windows volume exposes no additional short-name alias for this fixture')
        # On Windows, UNC module loading remains explicit unsupported behavior.
        # Do not ask the Node reference to probe a remote SMB host for this test.
        if os.name == 'nt':
            policy = write('unc-policy.mjs', "try{await import('file://remote/test.mjs');throw Error('UNC accepted')}catch(e){if(e.code!=='ERR_MITE_UNSUPPORTED_API')throw e;console.log('UNC_UNSUPPORTED')}")
            result = subprocess.run([args.mite, str(policy)], capture_output=True, text=True, encoding='utf-8', timeout=10)
            assert result.returncode == 0 and not result.stderr and result.stdout.strip() == 'UNC_UNSUPPORTED', result
    print('PASS Unicode entry/ESM/CJS/package paths, escaped URL names and native filename/dirname/module.paths metadata against Node')


if __name__ == '__main__':
    main()
