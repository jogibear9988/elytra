#!/usr/bin/env python3
"""Pinned extension-host defaults: reversal and focused Node/Mite policy checks."""

import _support  # noqa: F401 — configure owned tooling imports
import argparse
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
const fail=()=>{throw Error('Policy assertion '+mode)};
const uri=(path)=>({scheme:'file',authority:'',path,query:'',fragment:''});
process._vscodeLibJSEngineStatus={engine:'LibJS/LibWeb',scope:'implicit-code-oss-engine-tuning-only'};
const built=mode.includes('built');
const options={args:{'prof-v8-extensions':false},isBuilt:built,debug:{port:built?null:5870,break:false},development:false,developmentLocations:undefined,tests:undefined};
function development(tests=false){
 options.development=true;
 options.args.extensionDevelopmentPath=['/owned/extension'];
 options.developmentLocations=[uri('/owned/extension')];
 if(tests){options.args.extensionTestsPath='/owned/tests.mjs';options.tests=uri('/owned/tests.mjs');}
}
const validDev=mode.startsWith('dev-')||mode.startsWith('tests-');
if(validDev)development(mode.startsWith('tests-'));
const early={
 explicit:()=>options.args['inspect-extensions']='',
 'explicit-zero':()=>options.args['inspect-extensions']='0',
 'explicit-false':()=>options.args['inspect-brk-extensions']=false,
 break:()=>options.debug.break=true,
 prof:()=>options.args['prof-v8-extensions']='',
 'js-flags':()=>options.args['js-flags']='',
 'debug-id':()=>options.debug.debugId='owned',
 'debug-env':()=>options.debug.env={},
 test:()=>options.tests={},
 development:()=>options.development=true,
 'raw-only':()=>options.args.extensionDevelopmentPath=['/owned/extension'],
 'locations-only':()=>options.developmentLocations=[uri('/owned/extension')],
 'missing-tests-uri':()=>{development();options.args.extensionTestsPath='/owned/tests.mjs'},
 'missing-tests-raw':()=>{development();options.tests=uri('/owned/tests.mjs')},
 'remote-uri':()=>{development();options.developmentLocations[0].scheme='vscode-remote'},
 authority:()=>{development();options.developmentLocations[0].authority='remote'},
 relative:()=>{development();options.developmentLocations[0].path='relative'},
 query:()=>{development();options.developmentLocations[0].query='query'},
 fragment:()=>{development(true);options.tests.fragment='fragment'},
 'bad-count':()=>{development();options.developmentLocations.push(uri('/other'))},
 'bad-raw':()=>{development();options.args.extensionDevelopmentPath=[false]},
 'unexpected-port':()=>options.debug.port=5999,
 'missing-engine':()=>delete process._vscodeLibJSEngineStatus,
 'wrong-profile':()=>{}
};
if(early[mode])early[mode]();
let reads=0,select,error;
try{select=policy(()=>{reads++;return options})}catch(e){error=e}
if(node){if(select!==undefined||error||reads)fail();}
else if(early[mode]){if(error?.code!=='ERR_CODE_OSS_LIBJS_ENGINE_OPTION')fail();}
else {
 if(error||typeof select!=='function'||reads!==1)fail();
 const port=built||mode.includes('fallback')?0:5870;
 const tests=options.tests!==undefined;
 const flags=['--dns-result-order=ipv4first','--experimental-network-inspection',...(tests?['--expose-gc']:[]),...(port?['--nolazy',`--inspect=127.0.0.1:${port}`]:['--inspect-port=0'])];
 if(mode==='unknown')flags.push('--prof');
 if(mode==='gc-unrequested')flags.splice(2,0,'--expose-gc');
 if(mode==='tests-missing-gc')flags.splice(2,1);
 if(mode==='tests-duplicate-gc')flags.splice(2,0,'--expose-gc');
 if(mode==='mutated')development();
 if(mode==='dev-mutated-path'){options.args.extensionDevelopmentPath=['/different'];options.developmentLocations=[uri('/different')];}
 if(mode==='tests-mutated-path'){options.args.extensionTestsPath='/different.mjs';options.tests=uri('/different.mjs');}
 const original=JSON.stringify(flags);let result,failure;
 try{result=select(flags,port)}catch(e){failure=e}
 const late=['unknown','mutated','gc-unrequested','tests-missing-gc','tests-duplicate-gc','dev-mutated-path','tests-mutated-path'].includes(mode);
 if(JSON.stringify(flags)!==original)fail();
 if(late){if(failure?.code!=='ERR_CODE_OSS_LIBJS_ENGINE_OPTION')fail();}
 else {
  if(failure||JSON.stringify(result)!==JSON.stringify(tests?['--expose-gc']:[]))fail();
  const d=Object.getOwnPropertyDescriptor(globalThis,'_vscodeLibJSExtensionHostFlagsStatus'),s=d.value;
  if(d.writable||d.configurable||!Object.isFrozen(s)||!Object.isFrozen(s.omitted)||!Object.isFrozen(s.supportedNativeFlags)||s.inspectorAvailable!==false||s.networkInspectionAvailable!==false||s.dnsOrderingApplied!==false||JSON.stringify(s.supportedNativeFlags)!=='["--expose-gc"]')fail();
  if(JSON.stringify(s).includes('/owned/'))fail();
 }
}
const {assertLibJSStartupOptions:checkCLI}=await import('./engine-policy.js');
for(const name of ['inspect-extensions','inspect-brk-extensions','prof-v8-extensions','js-flags']){
 let rejected=false;try{checkCLI({sandbox:true,[name]:false},{},{hasSwitch:key=>key===name})}catch(e){if(e.code!=='ERR_CODE_OSS_LIBJS_ENGINE_OPTION')throw e;rejected=true}
 if(rejected!==!node)fail();
}
console.log('PASS extension-host flags '+mode);
""")
        modes = [
            "valid",
            "built",
            "fallback",
            "dev-valid",
            "dev-built",
            "dev-fallback",
            "tests-valid",
            "tests-built",
            "tests-fallback",
            "explicit",
            "explicit-zero",
            "explicit-false",
            "break",
            "prof",
            "js-flags",
            "debug-id",
            "debug-env",
            "test",
            "development",
            "raw-only",
            "locations-only",
            "missing-tests-uri",
            "missing-tests-raw",
            "remote-uri",
            "authority",
            "relative",
            "query",
            "fragment",
            "bad-count",
            "bad-raw",
            "unexpected-port",
            "missing-engine",
            "wrong-profile",
            "unknown",
            "mutated",
            "gc-unrequested",
            "tests-missing-gc",
            "tests-duplicate-gc",
            "dev-mutated-path",
            "tests-mutated-path",
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
        f"PASS exact extension-host source reversal/drift + {len(modes)} Node/Mite implicit/explicit policy cases"
    )


if __name__ == "__main__":
    main()
