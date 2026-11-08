#!/usr/bin/env python3
"""Verify the opt-in package adapter without loading a native addon or reading OS policy."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("A real Node runtime is required for the build-tool adapter")
    script = r"""
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import vm from 'node:vm';
import {createHash} from 'node:crypto';
import {planPolicyWatcher,applyPolicyWatcher,transformPolicyWatcher} from './tooling/code_oss/policy-watcher.mjs';
const check=(value,message)=>{if(!value)throw Error(message)};
const fails=(action)=>{let caught;try{action()}catch(error){caught=error}check(caught,'Expected rejection')};
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
const vendor='vendor/policy-watcher';
const pin=JSON.parse(fs.readFileSync(path.join(vendor,'manifest.json')));
for(const [name,sha]of Object.entries(pin.files))check(hash(fs.readFileSync(path.join(vendor,name)))===sha,'Pinned input drift '+name);
const original=fs.readFileSync(path.join(vendor,'index.js'));
const adapted=transformPolicyWatcher(original);
check(adapted.toString().startsWith(original.toString().split('exports.createWatcher')[0]),'License remains verbatim');
fails(()=>transformPolicyWatcher(Buffer.concat([original,Buffer.from('\n')])));
for(const mite of [false,true]){
 const calls=[];
 const sentinel=()=>{};
 const exports={};const module={exports};
 const require=name=>{calls.push(name);if(name==='mite:policy-watcher')return{createWatcher:sentinel};if(name==='bindings')return binary=>{check(binary==='vscode-policy-watcher','Original binary name');return sentinel};throw Error(name)};
 require.main={};
 vm.runInNewContext(adapted.toString(),{process:{versions:mite?{mite:'test'}:{node:'24'}},exports,module,require});
 check(exports.createWatcher===sentinel,'Selected factory export');
 check(JSON.stringify(calls)===JSON.stringify([mite?'mite:policy-watcher':'bindings']),'Only selected runtime branch executes');
}
const root=fs.mkdtempSync(path.join(os.tmpdir(),'elytra-policy-adapter-'));
try{
 const packageDir=path.join(root,'node_modules/@vscode/policy-watcher');
 fs.mkdirSync(path.dirname(packageDir),{recursive:true});fs.cpSync(vendor,packageDir,{recursive:true});
 const plan=planPolicyWatcher(root);
 check(plan.record.name==='@vscode/policy-watcher'&&plan.record.version==='1.7.0','Exact package selection');
 check(plan.record.sourceSha256===hash(original)&&plan.record.outputSha256===hash(adapted),'Input/output provenance');
 check(plan.record.packageSha256===pin.files['package.json'],'Package metadata provenance');
 check(hash(fs.readFileSync(plan.record.adapterSource.path))===plan.record.adapterSource.sha256,'Owned adapter provenance');
 applyPolicyWatcher(root,plan);
 check(hash(fs.readFileSync(path.join(root,plan.record.path)))===plan.record.outputSha256,'Fresh copied package adapted');
 check(hash(fs.readFileSync(path.join(vendor,'index.js')))===plan.record.sourceSha256,'Upstream input untouched');
 fails(()=>applyPolicyWatcher(root,plan));
 fs.writeFileSync(path.join(packageDir,'package.json'),'{"name":"@vscode/policy-watcher","version":"1.8.0","main":"index.js"}');
 fails(()=>planPolicyWatcher(root));
}finally{fs.rmSync(root,{recursive:true,force:true})}
console.log('PASS exact 1.7.0 adapter provenance drift refusal fresh-copy isolation and conditional Node/Mite loader branches; no native addon loaded');
"""
    result = subprocess.run(
        [args.node, "--input-type=module", "-e", script],
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=15,
    )
    assert result.returncode == 0 and not result.stderr, result
    print(result.stdout, end="")


if __name__ == "__main__":
    main()
