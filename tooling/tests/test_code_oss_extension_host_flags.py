#!/usr/bin/env python3
"""Pinned extension-host defaults: reversal and focused Node/Mite policy checks."""

import _support
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from code_oss.extension_host_flags import (
    SOURCE,
    HELPER_SOURCE,
    IMPORT,
    ADDED_IMPORT,
    START,
    CHECK,
    FLAGS,
    SELECT,
    adapt_extension_host_flags,
    revert_extension_host_flags,
)

ROOT = Path(__file__).resolve().parents[2]


def refused(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("Extension-host source drift accepted")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    original = (ROOT / "vendor/code-oss-tests" / SOURCE).read_bytes()
    adapted = adapt_extension_host_flags(original)
    assert adapted == original.replace(IMPORT.encode(), (IMPORT + ADDED_IMPORT).encode()).replace(
        START.encode(), (CHECK + START).encode()
    ).replace(FLAGS.encode(), (FLAGS + SELECT).encode())
    assert revert_extension_host_flags(adapted) == original
    for seam in (IMPORT, START, FLAGS):
        refused(lambda seam=seam: adapt_extension_host_flags(original + seam.encode()))
        refused(lambda seam=seam: adapt_extension_host_flags(original.replace(seam.encode(), b"")))
    for data in (original, adapted + b"\n", adapted + SELECT.encode()):
        refused(lambda data=data: revert_extension_host_flags(data))
    with tempfile.TemporaryDirectory(prefix="elytra-extension-host-flags-") as temp:
        work = Path(temp)
        shutil.copyfile(ROOT / HELPER_SOURCE, work / "helper.ts")
        shutil.copyfile(
            ROOT / "firefly/compat/code-oss/local-desktop.ts", work / "elytraLocalDesktop.ts"
        )
        shutil.copyfile(
            ROOT / "firefly/compat/code-oss/engine-policy.ts", work / "engine-policy.ts"
        )
        transform = """import fs from 'node:fs';import{pathToFileURL}from'node:url';const[d,c]=process.argv.slice(1);const{transformSync,version}=await import(pathToFileURL(c));if(version!=='0.25.12')throw Error('Unexpected compiler');for(const name of ['helper','elytraLocalDesktop','engine-policy'])fs.writeFileSync(d+'/'+name+'.js',transformSync(fs.readFileSync(d+'/'+name+'.ts','utf8'),{loader:'ts',format:'esm',target:'es2024'}).code);"""
        (work / "package.json").write_text('{"type":"module"}\n')
        subprocess.run(
            [
                args.node,
                "--input-type=module",
                "-e",
                transform,
                str(work),
                str(ROOT / "tooling/runtime-js/node_modules/esbuild/lib/main.js"),
            ],
            check=True,
            capture_output=True,
            text=True,
            timeout=20,
        )
        (work / "main.mjs").write_text("""
import {libJSExtensionHostPolicy as policy} from './helper.js';
const mode=process.argv[2],node=typeof process.versions.node==='string';
process._vscodeLibJSEngineStatus={engine:'LibJS/LibWeb',scope:'implicit-code-oss-engine-tuning-only'};
const options={args:{'prof-v8-extensions':false},isBuilt:mode==='built',debug:{port:mode==='built'?null:5870,break:false},development:false,tests:undefined};
if(mode==='explicit')options.args['inspect-extensions']='';
if(mode==='break')options.debug.break=true;
if(mode==='prof')options.args['prof-v8-extensions']='';
if(mode==='test')options.tests={};
if(mode==='development')options.development=true;
if(mode==='unexpected-port')options.debug.port=5999;
if(mode==='missing-engine')delete process._vscodeLibJSEngineStatus;
let reads=0,select,error;
try{select=policy(()=>{reads++;return options})}catch(e){error=e}
if(node){if(select!==undefined||error||reads)throw Error('Changed ordinary Node/Electron behavior');}
else {
 const early=['explicit','break','prof','test','development','unexpected-port','missing-engine','wrong-profile'].includes(mode);
 if(early){if(error?.code!=='ERR_CODE_OSS_LIBJS_ENGINE_OPTION')throw Error('Explicit option accepted');}
 else {
  if(error||typeof select!=='function')throw Error('Local policy rejected');
  const port=mode==='built'||mode==='fallback'?0:5870;
  const flags=['--dns-result-order=ipv4first','--experimental-network-inspection',...(port?['--nolazy',`--inspect=127.0.0.1:${port}`]:['--inspect-port=0'])];
  if(mode==='unknown')flags.push('--prof');
  if(mode==='mutated')options.development=true;
  const original=JSON.stringify(flags);let result,failure;
  try{result=select(flags,port)}catch(e){failure=e}
  if(['unknown','mutated'].includes(mode)){if(failure?.code!=='ERR_CODE_OSS_LIBJS_ENGINE_OPTION')throw Error('Changed flags accepted');}
  else {
   if(failure||!Array.isArray(result)||result.length||JSON.stringify(flags)!==original)throw Error('Implicit flags not omitted safely');
   const d=Object.getOwnPropertyDescriptor(globalThis,'_vscodeLibJSExtensionHostFlagsStatus'),s=d.value;
   if(d.writable||d.configurable||!Object.isFrozen(s)||!Object.isFrozen(s.omitted)||s.inspectorAvailable!==false||s.networkInspectionAvailable!==false||s.dnsOrderingApplied!==false)throw Error('Missing immutable omission disclosure');
  }
 }
}
const {assertLibJSStartupOptions:checkCLI}=await import('./engine-policy.js');
for(const name of ['inspect-extensions','inspect-brk-extensions','prof-v8-extensions']){
 let rejected=false;try{checkCLI({[name]:false},{},{hasSwitch:key=>key===name})}catch(e){if(e.code!=='ERR_CODE_OSS_LIBJS_ENGINE_OPTION')throw e;rejected=true}
 if(rejected!==!node)throw Error('Explicit raw option presence not preserved');
}
console.log('PASS extension-host flags '+mode);
""")
        modes = [
            "valid",
            "built",
            "fallback",
            "explicit",
            "break",
            "prof",
            "test",
            "development",
            "unexpected-port",
            "missing-engine",
            "wrong-profile",
            "unknown",
            "mutated",
        ]
        for mode in modes:
            env = dict(os.environ)
            env.pop("VSCODE_CODE_CACHE_PATH", None)
            env["ELYTRA_CODE_OSS_LOCAL_DESKTOP"] = "0" if mode == "wrong-profile" else "1"
            for binary in (args.node, str(args.mite.resolve())):
                r = subprocess.run(
                    [binary, str(work / "main.mjs"), mode],
                    env=env,
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
                assert (
                    r.returncode == 0
                    and r.stdout.strip() == "PASS extension-host flags " + mode
                    and "Error" not in r.stderr
                ), r
    print(
        "PASS exact extension-host source reversal/drift + 13 Node/Mite implicit/explicit policy cases"
    )


if __name__ == "__main__":
    main()
