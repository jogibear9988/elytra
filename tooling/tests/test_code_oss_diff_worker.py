#!/usr/bin/env python3
"""Pinned lazy worker construction; genuine Node constructor and actual Mite refusal."""

import _support
import argparse
import hashlib
import json
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
from code_oss.diff_worker import (
    SOURCE,
    SOURCE_SHA256,
    IMPORT,
    REPLACEMENT_IMPORT,
    CONSTRUCT,
    REPLACEMENT_CONSTRUCT,
    adapt_diff_worker,
    revert_diff_worker,
)
from code_oss.overlay import prepare

ROOT = Path(__file__).resolve().parents[2]


def refused(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("Diff worker source drift accepted")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    args = parser.parse_args()
    node = shutil.which("node")
    original = (ROOT / "vendor/code-oss-tests" / SOURCE).read_bytes()
    manifest = json.loads((ROOT / "vendor/code-oss-tests/manifest.json").read_text())
    assert (
        hashlib.sha256(original).hexdigest() == SOURCE_SHA256 == manifest["files"][SOURCE]["sha256"]
    )
    adapted = adapt_diff_worker(original)
    assert adapted == original.replace(IMPORT.encode(), REPLACEMENT_IMPORT.encode()).replace(
        CONSTRUCT.encode(), REPLACEMENT_CONSTRUCT.encode()
    )
    assert revert_diff_worker(adapted) == original
    for data in [
        original + b"\n",
        original.replace(CONSTRUCT.encode(), b""),
        original + IMPORT.encode(),
        adapted,
    ]:
        refused(lambda data=data: adapt_diff_worker(data))
    refused(lambda: revert_diff_worker(adapted + b"\n"))
    with tempfile.TemporaryDirectory(prefix="elytra-diff-worker-") as directory:
        work = Path(directory)
        prepare(
            args.source, work / "overlay", libjs_cache=True, libjs_engine=True, local_desktop=True
        )
        compile_helper = """
import fs from 'node:fs';import path from 'node:path';import{build}from './tooling/runtime-js/node_modules/esbuild/lib/main.js';
const root=process.cwd(),seen=new Set();
const result=await build({entryPoints:['firefly/compat/code-oss/diff-worker.ts'],bundle:true,format:'esm',platform:'neutral',target:'es2022',external:['worker_threads'],write:false,plugins:[{name:'owned-leaves',setup(build){build.onResolve({filter:/elytra(LocalDesktop|CompileCache)\\.js$/},args=>{const key=args.path.includes('LocalDesktop')?'local-desktop.ts':'compile-cache.ts';seen.add(key);return{path:path.join(root,'firefly/compat/code-oss',key)}})}}]});
if(seen.size!==2)throw Error('Unverified helper graph');fs.writeFileSync(process.argv[1],result.outputFiles[0].text);
"""
        subprocess.run(
            [node, "--input-type=module", "-e", compile_helper, str(work / "helper.mjs")],
            cwd=ROOT,
            check=True,
        )
        fixture = work / "main.mjs"
        fixture.write_text("""
import assert from 'node:assert/strict';
if(process.argv[2]==='conflict')process._vscodeLibJSAgentDiffWorkerStatus={};
const {requireAgentDiffWorker}=await import('./helper.mjs');
if(typeof process.versions.node==='string'){
 const actual=(await import('worker_threads')).Worker;assert.equal(requireAgentDiffWorker(),actual);assert.equal(requireAgentDiffWorker(),actual);
 if(process.argv[2]!=='conflict')assert.equal(process._vscodeLibJSAgentDiffWorkerStatus,undefined);
}else{
 if(process.argv[2]!=='conflict')assert.equal(process._vscodeLibJSAgentDiffWorkerStatus,undefined);
 assert.equal(process.versions.node,undefined);
 assert.throws(()=>requireAgentDiffWorker(),{code:'ERR_CODE_OSS_LIBJS_DIFF_WORKER_UNAVAILABLE'});
 assert.throws(()=>requireAgentDiffWorker(),{code:'ERR_CODE_OSS_LIBJS_DIFF_WORKER_UNAVAILABLE'});
 if(process.argv[2]==='valid'){
  const d=Object.getOwnPropertyDescriptor(process,'_vscodeLibJSAgentDiffWorkerStatus');assert.equal(d.writable,false);assert.equal(d.configurable,false);assert.equal(d.enumerable,false);assert.equal(Object.isFrozen(d.value),true);assert.equal(d.value.available,false);assert.equal(d.value.feature,'agent-diff-worker');
 }else if(process.argv[2]!=='conflict')assert.equal(process._vscodeLibJSAgentDiffWorkerStatus,undefined);
}
console.log('PASS lazy real constructor and explicit diff worker refusal');
""")
        for mode in ["valid", "disabled", "cache", "conflict"]:
            env = dict(os.environ)
            env["ELYTRA_CODE_OSS_LOCAL_DESKTOP"] = "0" if mode == "disabled" else "1"
            for key in [
                "VSCODE_CODE_CACHE_PATH",
                "VSCODE_GENERATE_NODE_COMPILE_CACHE",
                "VSCODE_NODE_COMPILE_CACHE_MEASURE",
            ]:
                env.pop(key, None)
            if mode == "cache":
                env["VSCODE_CODE_CACHE_PATH"] = "explicit-cache"
            for binary in [node, str(args.mite.resolve())]:
                result = subprocess.run(
                    [binary, str(fixture), mode],
                    cwd=ROOT,
                    env=env,
                    text=True,
                    capture_output=True,
                    timeout=15,
                )
                assert result.returncode == 0, result
                assert (
                    result.stdout.strip()
                    == "PASS lazy real constructor and explicit diff worker refusal"
                ), result
                assert not result.stderr or result.stderr.startswith(
                    "Elytra experimental local desktop:"
                ), result
    print("PASS pinned diff worker source/overlay/refusal and real Node/Mite policy")


if __name__ == "__main__":
    main()
