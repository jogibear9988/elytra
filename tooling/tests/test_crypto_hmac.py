#!/usr/bin/env python3
"""Compare native HMAC/byte equality with Node and independent Python HMAC vectors."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import hmac
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

PREFIX = r"""
(async()=>{
const {createHmac,Hmac,timingSafeEqual}=require('node:crypto');
const {Buffer}=require('node:buffer');
const {Transform,Readable,pipeline}=require('node:stream');
const rows=[];
function observe(fn){try{const value=fn();rows.push({value:Buffer.isBuffer(value)?{hex:value.toString('hex')}:value});}catch(e){rows.push({error:e.name,code:e.code});}}
"""
SUFFIX = r"""
const ns=await import('node:crypto');
const identity=ns.default===require('crypto')&&ns.createHmac===createHmac&&ns.Hmac===Hmac&&ns.timingSafeEqual===timingSafeEqual;
const h=Hmac('sha256','key');
const types=[h instanceof Hmac,h instanceof Transform,h.update('abc')===h,Buffer.isBuffer(h.digest()),typeof h.copy==='undefined'];
const source=Buffer.from('key');const saved=createHmac('sha256',source);source.fill(0);
const snapshot=saved.update('abc').digest('hex');
let gets=0;const sentinel={};let preserved=false;
try{createHmac('sha256','key',{get encoding(){gets++;throw sentinel}})}catch(e){preserved=e===sentinel}
const recover=createHmac('sha256','key').update('abc');let symbolError;
try{recover.digest(Symbol())}catch(e){symbolError=e.name}
const recovered=recover.digest('hex');
const stream=createHmac('sha256','key');const chunks=[];stream.on('data',chunk=>chunks.push(chunk));
await new Promise((resolve,reject)=>{stream.on('end',resolve);stream.on('error',reject);stream.write('a');stream.write(Buffer.from('b'));stream.end('c')});
const streamed=Buffer.concat(chunks).toString('hex');
// Do not compare Node's post-stream manual digest: that path can return
// uninitialized bytes in the pinned reference. Mite's safe policy is tested below.
const piped=createHmac('sha256','key');piped.setEncoding('base64url');let output='';piped.on('data',chunk=>output+=chunk);
await new Promise((resolve,reject)=>{piped.on('end',resolve);pipeline(Readable.from(['a',Buffer.from('b'),'c']),piped,error=>{if(error)reject(error)})});
const encodedStreams=[];
for(const encoding of ['hex','base64url']){
 const stream=createHmac('sha256','616263',{encoding});let text='';stream.on('data',chunk=>{if(typeof chunk!=='string')throw Error('Missing stream encoding');text+=chunk});
 await new Promise((resolve,reject)=>{stream.on('end',resolve);stream.on('error',reject);stream.end('abc')});encodedStreams.push(text);
}
const stress=[];
for(let i=0;i<128;i++){
 const active=createHmac('sha256','key').update('abc');
 for(let j=0;j<16;j++){createHmac('sha512',Buffer.alloc(257,j)).update('abandoned');new Uint8Array(8192);}
 stress.push(active.digest('hex')===snapshot);
}
console.log(JSON.stringify({rows,identity,types,snapshot,gets,preserved,symbolError,recovered,streamed,output,encodedStreams,stress:stress.every(Boolean)}));
})().catch(error=>{console.error(error);process.exitCode=1});
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node reference required")
    cases = []
    for algorithm in [
        "md5",
        "sha1",
        "sha224",
        "sha256",
        "sha384",
        "sha512",
        "sha3-256",
        "sha3-512",
        "blake2b512",
        "blake2s256",
        "SHA256",
        "sha-256",
        "sha256\\0tail",
    ]:
        for key in ["''", "'key'", "'é😀\\0tail'", "Buffer.alloc(131,0xaa)"]:
            for data in ["''", "'abc'", "'é😀\\0data'"]:
                cases.append(f"createHmac('{algorithm}',{key}).update({data}).digest('hex')")
    binary = [
        "Buffer.from([9,1,2,3,9]).subarray(1,4)",
        "new Uint16Array([0x1234,0x5678])",
        "new DataView(new Uint8Array([9,1,2,3,9]).buffer,1,3)",
        "new Uint8Array(0)",
        "new Uint8Array([1,2,3])",
        "new BigInt64Array([123n])",
        "new ArrayBuffer(3)",
        "new SharedArrayBuffer(3)",
    ]
    for value in binary:
        cases.append(f"createHmac('sha256',{value}).update('abc').digest('hex')")
        if "ArrayBuffer(" not in value:
            cases.append(f"createHmac('sha256','key').update({value}).digest('hex')")
    for value in ["undefined", "null", "1", "{}", "[]", "1n", "Symbol()", 'new String("key")']:
        cases.append(f"createHmac('sha256',{value}).digest('hex')")
        cases.append(f"createHmac('sha256','key').update({value}).digest('hex')")
        cases.append(f"timingSafeEqual({value},Buffer.alloc(0))")
        cases.append(f"timingSafeEqual(Buffer.alloc(0),{value})")
    for algorithm in [
        "undefined",
        "null",
        "1",
        "{}",
        "Symbol()",
        "'bad'",
        "'md4'",
        "'shake128'",
        "'shake256'",
        "''",
    ]:
        cases.append(f"createHmac({algorithm},'key').digest('hex')")
    for opts in [
        "undefined",
        "null",
        "1",
        "'options'",
        "()=>{}",
        "[]",
        "{}",
        "{outputLength:1}",
        '{encoding:"hex"}',
        '{encoding:"base64"}',
        '{encoding:"utf16le"}',
        '{encoding:"latin1"}',
        '{encoding:"buffer"}',
        '{encoding:"bad"}',
        '{encoding:"hex\\0tail"}',
        "{encoding:null}",
        "{encoding:1}",
        "{encoding:false}",
        "{encoding:Symbol()}",
        'Object.create({encoding:"hex"})',
    ]:
        for key in ["'616263'", "Buffer.from('abc')"]:
            cases.append(f"createHmac('sha256',{key},{opts}).digest('hex')")
    for opts in [
        '{encoding:"buffer"}',
        '{encoding:"bad"}',
        "{highWaterMark:-1}",
        '{highWaterMark:"4"}',
    ]:
        cases.append(
            f"(()=>{{const h=createHmac('sha256',Buffer.from('abc'),{opts});h.on('data',()=>{{}});return true}})()"
        )
    for codec in [
        "undefined",
        "null",
        "false",
        "true",
        "0",
        "1",
        "Symbol()",
        "{}",
        "'hex'",
        "'HEX'",
        "'hex\\0tail'",
        "'base64'",
        "'base64url'",
        "'utf16le'",
        "'latin1'",
        "'buffer'",
        "'bad'",
    ]:
        cases.append(f"createHmac('sha256','key').update('616263',{codec}).digest('hex')")
        cases.append(f"createHmac('sha256','key').update('abc').digest({codec})")
        cases.append(
            f"(()=>{{const h=createHmac('sha256','key');h.digest();return h.digest({codec})}})()"
        )
    for method in ["update('abc')", "digest()"]:
        cases.append(f"(()=>{{const h=createHmac('sha256','key');h.digest();return h.{method}}})()")
    for method in ["update", "digest"]:
        cases.append(f"Hmac.prototype.{method}.call({{}},'abc')")
    for data in ["a", "abc", "xyz", "61z", "616z"]:
        cases.append(f"createHmac('sha256','key').update('{data}','hex').digest('hex')")
        cases.append(f"createHmac('sha256','{data}',{{encoding:'hex'}}).digest('hex')")
    for value in [
        "new ArrayBuffer(8)",
        "new Uint8Array(new ArrayBuffer(8),2,4)",
        "new DataView(new ArrayBuffer(8),2,4)",
    ]:
        cases.append(
            f"(()=>{{const v={value};(v.buffer||v).transfer();return createHmac('sha256',v).digest('hex')}})()"
        )
    cases += [
        "(()=>{const b=new ArrayBuffer(8,{maxByteLength:16}),v=new Uint8Array(b,2,4);v.set([1,2,3,4]);const h=createHmac('sha256',v);b.resize(0);return h.update('abc').digest('hex')})()",
        "(()=>{const b=new ArrayBuffer(8,{maxByteLength:16}),v=new Uint8Array(b,2,4);b.resize(0);return createHmac('sha256',v).digest('hex')})()",
        "(()=>{const h=createHmac('sha256','key');try{h.update('abc','hex')}catch{}return h.update('abc').digest('hex')})()",
        "(()=>{const h=createHmac('sha256','key').update('abc');return h.digest({toString(){return 'hex'}})})()",
        "(()=>{const h=createHmac('sha256','key').update('abc');h.destroy();return h.digest('hex')})()",
    ]
    for count in [0, 1, 2, 16, 33, 4096]:
        for position in [None, 0, count // 2, count - 1]:
            if position is not None and position < 0:
                continue
            change = "" if position is None else f"b[{position}]=1;"
            cases.append(
                f"(()=>{{const a=Buffer.alloc({count}),b=Buffer.alloc({count});{change}return timingSafeEqual(a,b)}})()"
            )
    for left, right in [
        ("new ArrayBuffer(0)", "new Uint8Array(0)"),
        ("new SharedArrayBuffer(0)", "new Uint8Array(0)"),
        ("new Uint16Array([0x1234])", "new Uint8Array(new Uint16Array([0x1234]).buffer)"),
        ("Buffer.alloc(1)", "Buffer.alloc(2)"),
        (
            "new Uint8Array([0,1,2,0]).subarray(1,3)",
            "new DataView(new Uint8Array([0,1,2,0]).buffer,1,2)",
        ),
    ]:
        cases.append(f"timingSafeEqual({left},{right})")
    # Python stdlib is an independent oracle, including block-size boundaries.
    vectors = [
        (
            "sha256",
            "0b" * 20,
            b"Hi There".hex(),
            "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
        )
    ]
    for algorithm in ["sha256", "sha512"]:
        for key_size in [0, 1, 63, 64, 65, 127, 128, 129, 1024]:
            for size in [0, 55, 56, 64, 112, 128, 65537]:
                key = bytes(i % 256 for i in range(key_size))
                data = bytes((i * 17) % 256 for i in range(size))
                vectors.append(
                    (algorithm, key.hex(), data.hex(), hmac.new(key, data, algorithm).hexdigest())
                )
    with tempfile.TemporaryDirectory(prefix="mite-crypto-hmac-") as directory:
        fixture = Path(directory) / "hmac.cjs"
        fixture.write_text(PREFIX + "\n".join(f"observe(()=>{case});" for case in cases) + SUFFIX)
        results = [
            subprocess.run(
                [exe, "--no-deprecation", str(fixture)]
                if exe == args.node
                else [exe, str(fixture)],
                capture_output=True,
                text=True,
                timeout=60,
            )
            for exe in [args.node, args.mite]
        ]
        for result in results:
            assert result.returncode == 0 and not result.stderr, result
        expected, actual = [json.loads(result.stdout) for result in results]
        if expected != actual:
            for case, node, mite in zip(cases, expected["rows"], actual["rows"]):
                if node != mite:
                    print(case, node, mite)
            for key in expected:
                if key != "rows" and expected[key] != actual.get(key):
                    print(key, expected[key], actual.get(key))
            raise AssertionError("HMAC differential mismatch")
        known = "9c196e32dc0175f86f4b1cb89289d6619de6bee699e4c378e68309ed97a1a6ab"
        assert actual["snapshot"] == actual["recovered"] == actual["streamed"] == known, actual
        assert (
            actual["identity"]
            and all(actual["types"])
            and actual["preserved"]
            and actual["gets"] == 1
            and actual["stress"]
        ), actual
        assert actual["symbolError"] == "TypeError", actual
        fixture.write_text(
            "const {createHmac}=require('crypto');const vectors="
            + json.dumps(vectors)
            + ";for(const [algorithm,key,data,expected] of vectors){const h=createHmac(algorithm,Buffer.from(key,'hex'));const b=Buffer.from(data,'hex');for(let i=0;i<b.length;i+=13)h.update(b.subarray(i,i+13));if(h.digest('hex')!==expected)throw Error('Independent HMAC mismatch '+algorithm);}console.log('PYTHON_HMAC_VECTORS_OK');"
        )
        result = subprocess.run(
            [args.mite, str(fixture)], capture_output=True, text=True, timeout=60
        )
        assert (
            result.returncode == 0
            and not result.stderr
            and result.stdout.strip() == "PYTHON_HMAC_VECTORS_OK"
        ), result
        # Safe deterministic post-stream policy; Node's native post-stream digest
        # is deliberately not an expected output. Also keep invalid object-mode
        # chunks in JavaScript rather than inheriting native engine assertions.
        fixture.write_text(
            "const {createHmac}=require('crypto');const h=createHmac('sha256','key');h.resume();h.on('end',()=>{const value=h.digest();if(!Buffer.isBuffer(value)||value.length!==0)throw Error('Unsafe post-stream digest');console.log('SAFE_POST_STREAM');});h.end('abc');const invalid=createHmac('sha256','key',{objectMode:true});invalid.on('error',e=>{if(e.code!=='ERR_INVALID_ARG_TYPE')throw e;console.log('SAFE_OBJECT_MODE_ERROR');});invalid.end({invalid:true});"
        )
        result = subprocess.run(
            [args.mite, str(fixture)], capture_output=True, text=True, timeout=10
        )
        assert (
            result.returncode == 0
            and not result.stderr
            and sorted(result.stdout.splitlines()) == ["SAFE_OBJECT_MODE_ERROR", "SAFE_POST_STREAM"]
        ), result
    example = Path(__file__).resolve().parents[2] / "examples/mite-cli/hmac.mjs"
    env = {**os.environ, "MITE_HMAC_KEY": "example-key"}
    message = "request é文 with spaces"
    signature = hmac.new(b"example-key", message.encode(), "sha256").hexdigest()
    for executable in [args.node, args.mite]:
        signed = subprocess.run(
            [executable, str(example), message], env=env, capture_output=True, text=True, timeout=10
        )
        assert (
            signed.returncode == 0 and not signed.stderr and signed.stdout.strip() == signature
        ), signed
        for text, provided, valid in [
            (message, signature, True),
            (message + "!", signature, False),
            (message, "bad", False),
            (message, signature + "\n", False),
            (message, signature.upper(), True),
        ]:
            result = subprocess.run(
                [executable, str(example), text, provided],
                env=env,
                capture_output=True,
                text=True,
                timeout=10,
            )
            assert (
                result.returncode == (0 if valid else 1)
                and not result.stderr
                and result.stdout.strip() == ("VALID" if valid else "INVALID")
            ), result
    print(
        f"PASS {len(cases)} HMAC/equality Node comparisons; {len(vectors)} Python vectors; key snapshots, ESM/CJS identity, Transform/pipeline and native-state allocation stress"
    )
    print(
        "INTENTIONAL DIFFERENCE: deterministic empty post-stream digest and safe invalid object-mode JS errors; no uninitialized bytes/native assertion copied from Node"
    )


if __name__ == "__main__":
    main()
