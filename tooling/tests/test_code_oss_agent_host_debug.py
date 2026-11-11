#!/usr/bin/env python3
"""Pinned implicit-inspector omission: source reversal and genuine Node/Mite leaf policy."""

import _support
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from code_oss.agent_host_debug import (
    SOURCE,
    SOURCE_SHA256,
    HELPER_SOURCE,
    IMPORT,
    ADDED_IMPORT,
    OPTION,
    REPLACEMENT,
    adapt_agent_host_debug,
    revert_agent_host_debug,
)

ROOT = Path(__file__).resolve().parents[2]


def refused(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("Inspector source drift was accepted")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    args = parser.parse_args()
    node = shutil.which("node")
    original = (ROOT / "vendor/code-oss-tests" / SOURCE).read_bytes()
    manifest = json.loads((ROOT / "vendor/code-oss-tests/manifest.json").read_text())
    assert (
        manifest["files"][SOURCE]["sha256"] == SOURCE_SHA256 == hashlib.sha256(original).hexdigest()
    )
    adapted = adapt_agent_host_debug(original)
    assert adapted == original.replace(IMPORT.encode(), (IMPORT + ADDED_IMPORT).encode()).replace(
        OPTION.encode(), REPLACEMENT.encode()
    )
    assert revert_agent_host_debug(adapted) == original
    for data in [
        original + b"\n",
        original.replace(OPTION.encode(), b""),
        original + OPTION.encode(),
        adapted,
    ]:
        refused(lambda data=data: adapt_agent_host_debug(data))
    for data in [original, adapted + b"\n", adapted + REPLACEMENT.encode()]:
        refused(lambda data=data: revert_agent_host_debug(data))
    with tempfile.TemporaryDirectory(prefix="elytra-shared-debug-") as directory:
        work = Path(directory)
        transform = """import fs from 'node:fs';import{transformSync}from './tooling/runtime-js/node_modules/esbuild/lib/main.js';fs.writeFileSync(process.argv[1],transformSync(fs.readFileSync(process.argv[2],'utf8'),{loader:'ts',format:'esm',target:'es2024'}).code);"""
        subprocess.run(
            [
                node,
                "--input-type=module",
                "-e",
                transform,
                str(work / "helper.mjs"),
                str(ROOT / HELPER_SOURCE),
            ],
            cwd=ROOT,
            check=True,
        )
        (work / "main.mjs").write_text("""
import {libJSAgentHostDebugParams as select} from './helper.mjs';
const mode=process.argv[2], node=typeof process.versions.node==='string';
process._vscodeLibJSLocalDesktopStatus={enabled:true,scope:'experimental-local-desktop',disabled:['engine-code-cache']};
process._vscodeLibJSEngineStatus={engine:'LibJS/LibWeb',scope:'implicit-code-oss-engine-tuning-only'};
if(mode==='missing-engine')delete process._vscodeLibJSEngineStatus;
if(mode==='conflict')process._vscodeLibJSAgentHostDebugStatus={};
const args={};
if(mode==='inspect-empty')args['inspect-agenthost']='';
if(mode==='inspect-explicit')args['inspect-agenthost']='5999';
if(mode==='break-empty')args['inspect-brk-agenthost']='';
const built=mode==='built';
const original={port:built?null:mode==='unexpected'?5999:5878,break:false,debugId:'original-data'};
let calls=0;
const parse=(value,flag)=>{if(value!==args||flag!==built)throw Error('Changed parser arguments');calls++;return original;};
let value,failure;
try{value=select(args,built,parse);}catch(error){failure=error;}
const valid=['valid','built'].includes(mode);
if(node){if(failure||calls!==1||value!==original)throw Error('Changed Node/Electron path');}
else if(valid){
 if(failure||calls!==1||value.port!==null||value.break!==false||value.debugId!=='original-data'||original.port!==(built?null:5878))throw Error('Implicit default contract');
 if(built){if(value!==original||process._vscodeLibJSAgentHostDebugStatus!==undefined)throw Error('Changed built/no-debug path');}
 else{
  const d=Object.getOwnPropertyDescriptor(process,'_vscodeLibJSAgentHostDebugStatus'),s=d.value;
  if(!Object.isFrozen(s)||!Object.isFrozen(s.omitted)||d.writable||d.configurable||s.available!==false||s.feature!=='implicit-agent-host-inspector'||s.omitted.join(',')!=='--nolazy,--inspect=5878')throw Error('Missing immutable unavailable feature');
  if(select(args,built,parse).port!==null||process._vscodeLibJSAgentHostDebugStatus!==s)throw Error('Unstable status');
 }
}else if(failure?.code!=='ERR_CODE_OSS_LIBJS_ENGINE_OPTION')throw Error('Unsupported inspector/policy accepted');
if(!node&&!valid&&mode!=='conflict'&&process._vscodeLibJSAgentHostDebugStatus!==undefined)throw Error('Published before admission');
if(!node&&['inspect-empty','inspect-explicit','break-empty','missing-engine','wrong-env','cache'].includes(mode)&&calls!==0)throw Error('Explicit request was not refused before parser');
console.log('PASS agent-host inspector '+mode);
""")
        for mode in [
            "valid",
            "built",
            "inspect-empty",
            "inspect-explicit",
            "break-empty",
            "unexpected",
            "missing-engine",
            "conflict",
            "wrong-env",
            "cache",
        ]:
            env = dict(os.environ)
            env.pop("VSCODE_CODE_CACHE_PATH", None)
            env["ELYTRA_CODE_OSS_LOCAL_DESKTOP"] = "0" if mode == "wrong-env" else "1"
            if mode == "cache":
                env["VSCODE_CODE_CACHE_PATH"] = "requested-cache"
            for binary in [node, str(args.mite.resolve())]:
                result = subprocess.run(
                    [binary, str(work / "main.mjs"), mode],
                    env=env,
                    text=True,
                    capture_output=True,
                    timeout=15,
                )
                assert (
                    result.returncode == 0
                    and not result.stderr
                    and result.stdout.strip() == "PASS agent-host inspector " + mode
                ), result
    print("PASS exact agent-host source reversal/drift and 10 Node/Mite inspector policy cases")


if __name__ == "__main__":
    main()
