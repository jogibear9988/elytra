#!/usr/bin/env python3
"""Exact utility-auth source reversal and focused genuine Node/Mite opt-out checks."""

import _support

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from code_oss.utility_auth import (
    SOURCE,
    HELPER_SOURCE,
    IMPORT,
    ADDED_IMPORT,
    OPTION,
    REPLACEMENT,
    UNSIGNED_OPTION,
    UNSIGNED_REPLACEMENT,
    adapt_utility_auth,
    revert_utility_auth,
)

ROOT = Path(__file__).resolve().parents[2]


def refused(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("Utility authentication adapter accepted source drift")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    original = (ROOT / "vendor/code-oss-tests" / SOURCE).read_bytes()
    adapted = adapt_utility_auth(original)
    assert adapted == original.replace(IMPORT.encode(), (IMPORT + ADDED_IMPORT).encode()).replace(
        OPTION.encode(), REPLACEMENT.encode()
    ).replace(UNSIGNED_OPTION.encode(), UNSIGNED_REPLACEMENT.encode())
    assert revert_utility_auth(adapted) == original
    for data in (
        original + b"\n",
        original.replace(OPTION.encode(), b""),
        original + OPTION.encode(),
        adapted,
    ):
        refused(lambda data=data: adapt_utility_auth(data))
    for data in (original, adapted + b"\n", adapted + REPLACEMENT.encode()):
        refused(lambda data=data: revert_utility_auth(data))
    manifest = json.loads((ROOT / "vendor/code-oss-tests/manifest.json").read_text())
    assert (
        manifest["files"][SOURCE]["upstream"]
        == "src/vs/platform/utilityProcess/electron-main/utilityProcess.ts"
    )
    with tempfile.TemporaryDirectory(prefix="elytra-utility-auth-") as temporary:
        work = Path(temporary)
        shutil.copyfile(ROOT / HELPER_SOURCE, work / "helper.ts")
        transform = """import fs from 'node:fs';import{pathToFileURL}from'node:url';
const [dir,compiler]=process.argv.slice(1);const{transformSync,version}=await import(pathToFileURL(compiler));
if(version!=='0.25.12')throw Error('Unexpected compiler');fs.writeFileSync(dir+'/helper.mjs',transformSync(fs.readFileSync(dir+'/helper.ts','utf8'),{loader:'ts',format:'esm',target:'es2024'}).code);"""
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
const policy=process.argv[2];
const node=typeof process.versions.node==='string';
process._vscodeLibJSLocalDesktopStatus={enabled:true,scope:'experimental-local-desktop',disabled:['engine-code-cache']};
process._vscodeLibJSEngineStatus={engine:'LibJS/LibWeb',scope:'implicit-code-oss-engine-tuning-only'};
if(policy==='missing-engine')delete process._vscodeLibJSEngineStatus;
if(policy==='conflict'){process._vscodeLibJSUtilityAuthStatus={};process._vscodeLibJSUtilityUnsignedLibrariesStatus={};}
const{libJSUtilityAuthForwarding:f,libJSUtilityUnsignedLibraries:u}=await import('./helper.mjs');
const value=policy==='undefined'?undefined:policy==='false'?false:policy==='malformed'?'true':true;
let result,failed=false;try{result=f(value)}catch(e){if(e.code!=='ERR_CODE_OSS_LIBJS_UTILITY_AUTH_UNAVAILABLE')throw e;failed=true}
const allowed=!['missing-engine','conflict','malformed','wrong-env'].includes(policy);
if(failed!==(!node&&!allowed))throw Error('Unexpected admission '+policy);
if(!failed&&result!==(node?value:value===true?false:value))throw Error('Unexpected option value');
const status=process._vscodeLibJSUtilityAuthStatus;
if(!node&&allowed&&value===true){
 const d=Object.getOwnPropertyDescriptor(process,'_vscodeLibJSUtilityAuthStatus');
 if(!Object.isFrozen(status)||d.writable||d.configurable||status.available!==false||status.requested!==true||status.scope!=='code-oss-local-utility-auth-opt-out-only')throw Error('Missing immutable unavailable disclosure');
 if(f(true)!==false||process._vscodeLibJSUtilityAuthStatus!==status)throw Error('Unstable disclosure');
}else if(policy!=='conflict'&&status!==undefined)throw Error('Published omission before use');
let unsigned, unsignedError;
try{unsigned=u(value)}catch(e){unsignedError=e}
if(Boolean(unsignedError)!==(!node&&!allowed))throw Error('Unsigned library admission');
if(!unsignedError&&unsigned!==(node?value:value===true?false:value))throw Error('Unsigned library option changed');
if(!node&&allowed&&value===true){
 const d=Object.getOwnPropertyDescriptor(process,'_vscodeLibJSUtilityUnsignedLibrariesStatus');
 if(!Object.isFrozen(d.value)||d.writable||d.configurable||d.value.available!==false||!d.value.reason.includes('neither loads NAPI'))throw Error('Missing unsigned unavailable disclosure');
}else if(policy!=='conflict'&&process._vscodeLibJSUtilityUnsignedLibrariesStatus!==undefined)throw Error('Unsigned published before use');
console.log('PASS utility auth '+policy);
""")
        for policy in (
            "valid",
            "false",
            "undefined",
            "missing-engine",
            "malformed",
            "conflict",
            "wrong-env",
        ):
            env = dict(os.environ)
            env.pop("VSCODE_CODE_CACHE_PATH", None)
            env["ELYTRA_CODE_OSS_LOCAL_DESKTOP"] = "0" if policy == "wrong-env" else "1"
            for binary in (args.node, str(args.mite.resolve())):
                result = subprocess.run(
                    [binary, str(work / "main.mjs"), policy],
                    env=env,
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
                assert (
                    result.returncode == 0
                    and not result.stderr
                    and result.stdout.strip() == "PASS utility auth " + policy
                ), result
    print("PASS exact utility-auth reversal/drift controls and 7 Node/Mite local-policy scenarios")


if __name__ == "__main__":
    main()
