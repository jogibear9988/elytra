#!/usr/bin/env python3
"""Compare random API contracts without comparing independently random byte values."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

PREFIX = r"""
(async()=>{
const crypto=require('node:crypto'),{Buffer}=require('node:buffer');
const rows=[];
function observe(fn){try{const value=fn();rows.push({value:Buffer.isBuffer(value)?{buffer:true,length:value.length}:typeof value==='string'?{uuid:/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(value)}:value});}catch(error){rows.push({error:error.name,code:error.code,argument:error.message.includes('disableEntropyCache')?'cache':error.message.includes('size')?'size':error.message.includes('callback')?'callback':error.message.includes('options')?'options':undefined});}}
"""
SUFFIX = r"""
let gets=0;const thrown={identity:true};let preserved=false;
try{crypto.randomUUID({get disableEntropyCache(){gets++;throw thrown}})}catch(error){preserved=error===thrown}
Math.random=()=>{throw Error('JS random must not be used')};
const uuids=Array.from({length:256},()=>crypto.randomUUID());
const uuidChecks=uuids.every(value=>/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(value)) && new Set(uuids).size===uuids.length;
const a=crypto.randomBytes(1024),b=crypto.randomBytes(1024);a.fill(0);
const buffers=Buffer.isBuffer(a)&&Object.getPrototypeOf(a)===Buffer.prototype&&a.buffer!==b.buffer&&b.some(value=>value!==0)&&new Set(b).size>1;
const ns=await import('node:crypto');const identity=crypto===require('crypto')&&ns.randomBytes===crypto.randomBytes&&ns.randomUUID===crypto.randomUUID&&ns.default===crypto;
const trace=[];let zeroThis=false,zeroArgs=0;
const zeroReturn=crypto.randomBytes(.9,function(error,value){'use strict';trace.push('zero');zeroThis=this===undefined;zeroArgs=arguments.length;if(error!==null || value.length!==0)throw Error('Zero callback contract')})===undefined;
let receiver;
const callback=await new Promise((resolve,reject)=>{
 const returned=crypto.randomBytes(65536,function(error,value){'use strict';receiver=this===undefined?'undefined':this.constructor.name;
  if(error)return reject(error);trace.push('random');resolve({arguments:arguments.length,buffer:Buffer.isBuffer(value),length:value.length,entropy:new Set(value).size>1});
 });
 if(returned!==undefined)throw Error('Async return changed');
 trace.push('sync');Promise.resolve().then(()=>trace.push('promise'));
});
const stress=await Promise.all(Array.from({length:16},(_,i)=>new Promise((resolve,reject)=>{
 const held={tag:'kept-'+i};crypto.randomBytes(4096+i,(error,value)=>{if(error)reject(error);else resolve(value.length===4096+i && held.tag==='kept-'+i && Buffer.isBuffer(value))});
 for(let j=0;j<2000;j++)({value:new Array(10).fill(j)});
})));
console.log(JSON.stringify({rows,gets,preserved,uuidChecks,buffers,identity,trace,zeroThis,zeroArgs,zeroReturn,callback,stress:stress.every(Boolean),receiver,
 functionInfo:[crypto.randomBytes.name,crypto.randomBytes.length,crypto.randomUUID.name,crypto.randomUUID.length]}));
})().catch(error=>{console.error(error);process.exitCode=1});
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required")
    invalid = [
        "undefined",
        "null",
        "true",
        "false",
        '"8"',
        "{}",
        "[]",
        "()=>{}",
        "1n",
        'Symbol("size")',
        "NaN",
        "Infinity",
        "-Infinity",
        "-1",
        "-0.1",
        "2147483648",
        "Number.MAX_SAFE_INTEGER",
    ]
    cases = ["crypto.randomBytes(" + value + ")" for value in invalid]
    for value in ["null", "0", "true", "{}", "[]", 'Symbol("callback")']:
        for size in ["0", ".9", "1", "NaN", '"bad"']:
            cases.append(f"crypto.randomBytes({size},{value})")
    cases += ["crypto.randomBytes(" + size + ")" for size in ["0", "-0", ".9", "1.9", "64", "4096"]]
    cases += [
        "crypto.randomUUID(" + value + ")"
        for value in [
            "undefined",
            "null",
            "0",
            "true",
            '"x"',
            "[]",
            "()=>{}",
            'Symbol("options")',
            "{}",
            "new Date()",
            "Object.create({disableEntropyCache:true})",
        ]
    ]
    cases += [
        "crypto.randomUUID({disableEntropyCache:" + value + "})"
        for value in [
            "undefined",
            "null",
            "false",
            "true",
            "0",
            "1",
            '"false"',
            "{}",
            "[]",
            "()=>{}",
            'Symbol("cache")',
            "1n",
        ]
    ]
    source = PREFIX + "\n".join("observe(()=>" + expr + ");" for expr in cases) + SUFFIX
    with tempfile.TemporaryDirectory(prefix="mite-crypto-random-") as directory:
        root = Path(directory)
        fixture = root / "random.cjs"
        fixture.write_text(source)
        results = [
            subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=20)
            for exe in [args.node, args.mite]
        ]
        for result in results:
            assert result.returncode == 0 and not result.stderr, result
        expected, actual = [json.loads(result.stdout) for result in results]
        receiver = [value.pop("receiver") for value in [expected, actual]]
        assert receiver[0] in ["RandomBytesJob", "undefined"] and receiver[1] == "undefined", (
            receiver
        )
        if expected != actual:
            print(
                [
                    (expr, node, mite)
                    for expr, node, mite in zip(cases, expected["rows"], actual["rows"])
                    if node != mite
                ]
            )
            raise AssertionError((expected, actual))
        assert len(actual["rows"]) == len(cases), actual
        assert all(
            actual[key]
            for key in [
                "preserved",
                "uuidChecks",
                "buffers",
                "identity",
                "zeroThis",
                "zeroReturn",
                "stress",
            ]
        ), actual
        assert (
            actual["trace"] == ["zero", "sync", "promise", "random"]
            and actual["zeroArgs"] == 2
            and actual["gets"] == 1
        ), actual
        assert actual["callback"] == dict(arguments=2, buffer=True, length=65536, entropy=True), (
            actual["callback"]
        )
        fatal = root / "fatal.cjs"
        fatal.write_text(
            "require('node:crypto').randomBytes(4096,()=>{console.log('RANDOM_CALLBACK');throw Error('random-callback-fatal')});"
        )
        for executable in [args.node, args.mite]:
            result = subprocess.run(
                [executable, str(fatal)], capture_output=True, text=True, timeout=10
            )
            assert (
                result.returncode == 1
                and "RANDOM_CALLBACK" in result.stdout
                and "random-callback-fatal" in result.stderr
            ), result
        unsupported = root / "unsupported.cjs"
        unsupported.write_text(
            "const crypto=require('crypto');for(const key of ['createSecretKey','randomFillSync','webcrypto']){if(crypto[key]!==undefined)throw Error('Unexpected crypto surface');}try{crypto.createSecretKey('key');throw Error('Unsupported KeyObject creation succeeded')}catch(error){if(!(error instanceof TypeError))throw error;}console.log('UNSUPPORTED_CRYPTO_REJECTED');"
        )
        result = subprocess.run(
            [args.mite, str(unsupported)], capture_output=True, text=True, timeout=10
        )
        assert (
            result.returncode == 0
            and not result.stderr
            and result.stdout.strip() == "UNSUPPORTED_CRYPTO_REJECTED"
        ), result
        named = root / "unsupported.mjs"
        named.write_text("import {createSecretKey} from 'node:crypto';console.log('BAD');")
        result = subprocess.run([args.mite, str(named)], capture_output=True, text=True, timeout=10)
        assert result.returncode != 0 and result.stderr and "BAD" not in result.stdout, result
    print(
        f"PASS {len(cases)} random API validation/shape comparisons, UUIDv4/native byte smoke, async ordering/liveness/stress/error and CJS/ESM identity"
    )
    print(
        "INTENTIONAL DIFFERENCE: nonempty async callback receiver is undefined; UUID entropy is uncached"
    )


if __name__ == "__main__":
    main()
