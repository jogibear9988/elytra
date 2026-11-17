#!/usr/bin/env python3
"""Pinned PTY-host inspector omission: exact source provenance and native leaf policy."""

import _support  # noqa: F401 — configure owned tooling imports
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from code_oss.pty_host_debug import (
    SOURCE,
    SOURCE_SHA256,
    HELPER_SOURCE,
    IMPORT,
    ADDED_IMPORT,
    OPTION,
    REPLACEMENT,
    adapt_pty_host_debug,
    revert_pty_host_debug,
)

ROOT = Path(__file__).resolve().parents[2]


def refused(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("PTY host source drift accepted")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--electron", type=Path)
    args = parser.parse_args()
    if not args.node:
        parser.error("Node reference required")
    original = (ROOT / "vendor/code-oss-tests" / SOURCE).read_bytes()
    manifest = json.loads((ROOT / "vendor/code-oss-tests/manifest.json").read_text())
    assert (
        manifest["files"][SOURCE]["sha256"] == SOURCE_SHA256 == hashlib.sha256(original).hexdigest()
    )
    adapted = adapt_pty_host_debug(original)
    assert adapted == original.replace(IMPORT.encode(), (IMPORT + ADDED_IMPORT).encode()).replace(
        OPTION.encode(), REPLACEMENT.encode()
    )
    assert revert_pty_host_debug(adapted) == original
    for seam in (IMPORT, OPTION):
        refused(lambda seam=seam: adapt_pty_host_debug(original.replace(seam.encode(), b"")))
        refused(lambda seam=seam: adapt_pty_host_debug(original + seam.encode()))
    for data in (original + b"\n", adapted):
        refused(lambda data=data: adapt_pty_host_debug(data))
    for data in (
        original,
        adapted + b"\n",
        adapted + REPLACEMENT.encode(),
        adapted.replace(ADDED_IMPORT.encode(), b""),
    ):
        refused(lambda data=data: revert_pty_host_debug(data))
    with tempfile.TemporaryDirectory(prefix="elytra-pty-host-debug-") as directory:
        work = Path(directory)
        transform = """import fs from 'node:fs';import{transformSync,version}from './tooling/runtime-js/node_modules/esbuild/lib/main.js';if(version!=='0.25.12')throw Error('Unexpected compiler');fs.writeFileSync(process.argv[1],transformSync(fs.readFileSync(process.argv[2],'utf8'),{loader:'ts',format:'esm',target:'es2024'}).code);"""
        subprocess.run(
            [
                args.node,
                "--input-type=module",
                "-e",
                transform,
                str(work / "helper.mjs"),
                str(ROOT / HELPER_SOURCE),
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=True,
            timeout=20,
        )
        subprocess.run(
            [
                args.node,
                "--input-type=module",
                "-e",
                transform,
                str(work / "engine-policy.mjs"),
                str(ROOT / "firefly/compat/code-oss/engine-policy.ts"),
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=True,
            timeout=20,
        )
        (work / "main.mjs").write_text("""
import {libJSPtyHostDebugParams as select} from './helper.mjs';
const mode=process.argv[2],ordinary=typeof process.versions.node==='string'||typeof process.versions.electron==='string';
const fail=()=>{throw Error('PTY inspector policy '+mode)};
process._vscodeLibJSLocalDesktopStatus={enabled:true,scope:'experimental-local-desktop',disabled:['engine-code-cache']};
process._vscodeLibJSEngineStatus={engine:'LibJS/LibWeb',scope:'implicit-code-oss-engine-tuning-only'};
if(mode==='missing-engine')delete process._vscodeLibJSEngineStatus;
if(mode==='wrong-engine')process._vscodeLibJSEngineStatus.engine='other';
if(mode==='missing-profile')delete process._vscodeLibJSLocalDesktopStatus;
if(mode==='wrong-profile')process._vscodeLibJSLocalDesktopStatus.scope='other';
if(mode==='enabled-false')process._vscodeLibJSLocalDesktopStatus.enabled=false;
if(mode==='missing-cache-disclosure')process._vscodeLibJSLocalDesktopStatus.disabled=[];
if(mode==='conflict')process._vscodeLibJSPtyHostDebugStatus={};
const input={};
for(const [name,prefix] of [['inspect-ptyhost','inspect'],['inspect-brk-ptyhost','break']]){
 for(const [suffix,value] of [['empty',''],['zero','0'],['false',false],['explicit','5999']]){
  if(mode===prefix+'-'+suffix)input[name]=value;
 }
}
let built=mode==='built'||mode==='built-unexpected';
if(mode==='bad-built')built='false';
let original={port:built===true?null:5877,break:false,metadata:{identity:true}};
if(mode==='unexpected')original.port=5999;
if(mode==='built-unexpected')original.port=5877;
if(mode==='parser-zero')original.port=0;
if(mode==='parser-brk')original.break=true;
if(mode==='parser-null')original=null;
if(mode==='parser-string')original.port='5877';
if(mode==='parser-missing')delete original.break;
let getterCalls=0;
if(mode==='parser-accessor')Object.defineProperty(original,'port',{get(){getterCalls++;throw Error('must not read parser accessor')}});
const sentinel={owned:true};
let calls=0;
const parse=(value,flag)=>{
 if(value!==input||flag!==built)fail();calls++;
 if(mode==='parser-throws')throw sentinel;
 if(mode==='mutate-args')input['inspect-ptyhost']=0;
 if(mode==='mutate-profile')process._vscodeLibJSLocalDesktopStatus.enabled=false;
 return original;
};
let result,error;
try{result=select(input,built,parse)}catch(value){error=value}
if(mode==='parser-throws'){
 if(error!==sentinel||calls!==1)fail();
}else if(ordinary){
 if(error||calls!==1||result!==original)fail();
}else if(['valid','built','late-explicit'].includes(mode)){
 if(error||calls!==1||result.port!==null||result.break!==false||result.metadata!==original.metadata)fail();
 if(mode==='built'){
  if(result!==original||process._vscodeLibJSPtyHostDebugStatus!==undefined)fail();
 }else{
  if(original.port!==5877||result===original)fail();
  const descriptor=Object.getOwnPropertyDescriptor(process,'_vscodeLibJSPtyHostDebugStatus'),status=descriptor.value;
  if(descriptor.writable||descriptor.configurable||descriptor.enumerable||!Object.isFrozen(status)||!Object.isFrozen(status.omitted)||status.available!==false||status.feature!=='implicit-pty-host-inspector'||status.omitted.join(',')!=='--nolazy,--inspect=5877')fail();
  if(mode==='late-explicit'){
   input['inspect-brk-ptyhost']='';let late;
   try{select(input,built,parse)}catch(value){late=value}
   if(late?.code!=='ERR_CODE_OSS_LIBJS_ENGINE_OPTION'||calls!==1)fail();
  }else{
   if(select(input,built,parse).port!==null||process._vscodeLibJSPtyHostDebugStatus!==status)fail();
  }
 }
}else{
 if(error?.code!=='ERR_CODE_OSS_LIBJS_ENGINE_OPTION')fail();
 const afterParser=['unexpected','built-unexpected','parser-zero','parser-brk','parser-null','parser-string','parser-missing','parser-accessor','mutate-args','mutate-profile','conflict'].includes(mode);
 if(calls!==(afterParser?1:0))fail();
 if(mode!=='conflict'&&process._vscodeLibJSPtyHostDebugStatus!==undefined)fail();
}
if(getterCalls!==0)fail();
const {assertLibJSStartupOptions}=await import('./engine-policy.mjs');
for(const key of ['inspect-ptyhost','inspect-brk-ptyhost']){
 for(const value of ['',false,0,'0']){
  for(const nativePresence of [false,true]){
   let refused=false;
   try{assertLibJSStartupOptions({sandbox:true},nativePresence?{}:{[key]:value},{hasSwitch:name=>nativePresence&&name===key})}
   catch(error){if(error.code!=='ERR_CODE_OSS_LIBJS_ENGINE_OPTION')throw error;refused=true}
   if(refused!==!ordinary)fail();
  }
 }
}
console.log('PASS PTY host inspector '+mode);
""")
        modes = [
            "valid",
            "built",
            "late-explicit",
            "inspect-empty",
            "inspect-zero",
            "inspect-false",
            "inspect-explicit",
            "break-empty",
            "break-zero",
            "break-false",
            "break-explicit",
            "unexpected",
            "built-unexpected",
            "parser-zero",
            "parser-brk",
            "parser-null",
            "parser-string",
            "parser-missing",
            "parser-accessor",
            "parser-throws",
            "bad-built",
            "mutate-args",
            "mutate-profile",
            "missing-engine",
            "wrong-engine",
            "missing-profile",
            "wrong-profile",
            "enabled-false",
            "missing-cache-disclosure",
            "conflict",
            "wrong-env",
            "cache",
        ]
        runtimes = [(args.node, False), (str(args.mite.resolve()), False)]
        if args.electron:
            runtimes.append((str(args.electron.resolve()), True))
        for mode in modes:
            for binary, electron in runtimes:
                environment = dict(os.environ)
                environment.pop("VSCODE_CODE_CACHE_PATH", None)
                environment.pop("ELECTRON_RUN_AS_NODE", None)
                environment["ELYTRA_CODE_OSS_LOCAL_DESKTOP"] = "0" if mode == "wrong-env" else "1"
                if mode == "cache":
                    environment["VSCODE_CODE_CACHE_PATH"] = "requested-cache"
                if electron:
                    environment["ELECTRON_RUN_AS_NODE"] = "1"
                result = subprocess.run(
                    [binary, str(work / "main.mjs"), mode],
                    env=environment,
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
                assert (
                    result.returncode == 0
                    and not result.stderr
                    and result.stdout.strip() == "PASS PTY host inspector " + mode
                ), result
    print(
        f"PASS exact PTY-host source reversal/drift and {len(modes)} Node/Mite inspector policy cases"
    )


if __name__ == "__main__":
    main()
