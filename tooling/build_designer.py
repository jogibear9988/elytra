#!/usr/bin/env python3
"""Build and stage the pinned upstream Designer demo with its Firefly main module."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
EXAMPLE=ROOT/'examples/web-component-designer'

def run(*args,cwd=ROOT):
    subprocess.run(list(map(str,args)),cwd=cwd,check=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=ROOT/'build/designer-source')
    parser.add_argument('--output',type=Path,default=ROOT/'build/designer-app')
    args=parser.parse_args();source=args.source.resolve();output=args.output.resolve()
    if output.exists():parser.error('Output already exists; choose a fresh staging directory')
    if source==output or source in output.parents:parser.error('Output must be separate from the source checkout')
    pin=json.loads((EXAMPLE/'upstream.json').read_text())
    if not source.exists():
        source.parent.mkdir(parents=True,exist_ok=True)
        run('git','clone','--no-checkout',pin['repository'],source)
    remote=subprocess.check_output(['git','remote','get-url','origin'],cwd=source,text=True).strip()
    if remote!=pin['repository']:parser.error('Source remote differs from pinned repository')
    dirty=subprocess.check_output(['git','status','--porcelain','--untracked-files=normal'],cwd=source,text=True)
    if dirty:parser.error('Source checkout contains local changes')
    run('git','fetch','--depth','1','origin',pin['revision'],cwd=source)
    run('git','checkout','--detach',pin['revision'],cwd=source)
    if hashlib.sha256((source/'package-lock.json').read_bytes()).hexdigest()!=pin['packageLockSha256']:parser.error('Pinned package-lock hash differs')
    npm=shutil.which('npm.cmd' if os.name=='nt' else 'npm')
    if not npm:parser.error('npm is a build-time prerequisite')
    run(npm,'ci','--ignore-scripts','--no-audit','--no-fund',cwd=source)
    # These two reviewed source scripts are explicit, not implicit lifecycle hooks.
    run(npm,'run','bundleMonaco',cwd=source)
    run(npm,'run','build',cwd=source)
    required=['dist/appShell.js','node_modules/monaco-editor/esm/vs/editor/editor.main.min.js','node_modules/monaco-editor/esm/vs/editor/editor.worker.js']
    for name in required:
        if not (source/name).is_file():raise RuntimeError('Build output missing: '+name)
    web=output/'web';web.mkdir(parents=True)
    for name in ['index.html','manifest.json','favicon.ico','assets','dist','node_modules']:
        item=source/name
        if item.is_dir():shutil.copytree(item,web/name,ignore=shutil.ignore_patterns('.DS_Store','.bin'))
        else:shutil.copy2(item,web/name)
    for name in ['main.mjs','server.mjs','smoke.js','upstream.json']:shutil.copy2(EXAMPLE/name,output/name)
    notices=output/'licenses';notices.mkdir()
    shutil.copy2(source/'LICENSE',notices/'designer-MIT.txt')
    shutil.copy2(source/'package-lock.json',output/'designer-package-lock.json')
    # Keep all dependency package/license files in node_modules, including workers.
    (output/'BUILD.json').write_text(json.dumps({'upstream':pin,'runtime':'firefly','webRoot':'web','status':'compiled; native UI/features require separate validation'},indent=2)+'\n')
    print('Staged compiled Designer application at',output)

if __name__=='__main__':main()
