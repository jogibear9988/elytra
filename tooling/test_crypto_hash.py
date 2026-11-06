#!/usr/bin/env python3
"""Exercise native hash state, byte encodings, copies and Transform workflows against Node."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

PREFIX = r'''
(async()=>{
const {createHash,Hash}=require('node:crypto'),{Buffer}=require('node:buffer');
const {Transform,Readable,pipeline}=require('node:stream');
const rows=[];
function observe(fn){try{const v=fn();rows.push({value:Buffer.isBuffer(v)?{buffer:v.toString('hex')}:v});}catch(e){rows.push({error:e.name,code:e.code});}}
'''
SUFFIX = r'''
const source=createHash('sha256').update('prefix');
const left=source.copy().update('left').digest('hex'),right=source.copy().update('right').digest('hex');
const original=source.update('original').digest('hex');
const symbol=createHash('sha256').update('abc');let symbolError;
try{symbol.digest(Symbol())}catch(e){symbolError=e.name}
const recovered=symbol.digest('hex');
let gets=0;const token={};let preserved=false;
try{createHash('sha256',{get outputLength(){gets++;throw token}})}catch(e){preserved=e===token}
const ns=await import('node:crypto');
const identity=ns.Hash===Hash&&ns.createHash===createHash&&ns.default===require('crypto');
const constructed=Hash('sha256');
const types=[constructed instanceof Hash,constructed instanceof Transform,constructed.update('abc')===constructed,Buffer.isBuffer(constructed.digest())];
const stream=createHash('sha256');let chunks=[];stream.on('data',c=>chunks.push(c));
await new Promise((resolve,reject)=>{stream.on('end',resolve);stream.on('error',reject);stream.write('a');stream.write(Buffer.from('b'));stream.end('c')});
const streamed=Buffer.concat(chunks).toString('hex');
const postStream=stream.digest('hex');
const piped=createHash('sha256');piped.setEncoding('hex');let output='';piped.on('data',c=>output+=c);
await new Promise((resolve,reject)=>{piped.on('end',resolve);pipeline(Readable.from(['a',Buffer.from('b'),'c']),piped,e=>{if(e)reject(e)})});
const stress=[];for(let i=0;i<128;i++){const h=createHash('sha256').update('value'+i);const copy=h.copy();for(let j=0;j<50;j++)new Uint8Array(1024);stress.push(h.digest('hex')===copy.digest('hex'));}
console.log(JSON.stringify({rows,left,right,original,symbolError,recovered,gets,preserved,identity,types,streamed,postStream,output,stress:stress.every(Boolean)}));
})().catch(e=>{console.error(e);process.exitCode=1});
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite',required=True)
    parser.add_argument('--node',default=shutil.which('node'))
    args=parser.parse_args()
    if not args.node:parser.error('Node required')
    cases=[]
    for algorithm in ['md5','sha1','sha224','sha256','sha384','sha512','sha3-256','sha3-512','blake2b512','blake2s256','shake128','shake256','SHA256','sha-256','sha256\\0tail']:
        for data in ["''","'abc'","'😀é\\0tail'","'x'.repeat(65537)"]:
            cases.append(f"createHash('{algorithm}').update({data}).digest('hex')")
    for codec in ['undefined',"'hex'","'HEX'","'base64'","'base64url'","'latin1'","'utf16le'","'bad'","'buffer'","'hex\\0tail'",'null','true','1','Symbol()']:
        cases.append(f"createHash('sha256').update('616263',{codec}).digest('hex')")
        cases.append(f"createHash('sha256').update('abc').digest({codec})")
    for data in ['new Uint8Array([1,2,3])','new Uint16Array([0x1234,0x5678])','new DataView(new Uint8Array([9,1,2,3,9]).buffer,1,3)','Buffer.from([9,1,2,3,9]).subarray(1,4)','new Uint8Array(0)','new ArrayBuffer(2)','undefined','null','3','{}','[1,2]','1n']:
        cases.append(f"createHash('sha256').update({data}).digest('hex')")
    for algorithm in ['undefined','null','1','{}',"'bad'","'md4'",'Symbol()']:
        cases.append(f'createHash({algorithm})')
    for options in ['undefined','null','1',"'options'",'()=>{}','[]','{}','{outputLength:32}','{outputLength:0}','{outputLength:1}','{outputLength:NaN}','{outputLength:-1}','{outputLength:1.5}','{outputLength:"32"}','{outputLength:0x100000000}']:
        cases.append(f"createHash('sha256',{options}).update('abc').digest('hex')")
    for length in [0,1,16,32,65,1024]:
        cases.append(f"createHash('shake128',{{outputLength:{length}}}).update('abc').digest('hex')")
        cases.append(f"createHash('shake128').update('abc').copy({{outputLength:{length}}}).digest('hex')")
    for method in ["update('x')",'digest()', 'copy()']:
        cases.append(f"(()=>{{const h=createHash('sha256');h.digest();return h.{method}}})()")
    for method in ['update','digest','copy']:
        cases.append(f"Hash.prototype.{method}.call({{}},'abc')")
    for data in ['a','abc','xyz','61z','616z']:
        cases.append(f"createHash('sha256').update('{data}','hex').digest('hex')")
    for options in ['{outputLength:null}','{outputLength:Infinity}','{outputLength:Symbol()}','Object.create({outputLength:32})']:
        cases.append(f"createHash('sha256',{options}).digest('hex')")
    for codec in ["'utf8'", "'utf-8'", "'ascii'", "'binary'", "'ucs2'", "'ucs-2'", "'base64url'"]:
        cases.append(f"createHash('sha256').update('aé😀',{codec}).digest('hex')")
    cases += [
        "(()=>{const b=new ArrayBuffer(8,{maxByteLength:16}),v=new Uint8Array(b,2,4);v.set([1,2,3,4]);const h=createHash('sha256');h.update(v);b.resize(0);return h.digest('hex')})()",
        "(()=>{const b=new ArrayBuffer(8,{maxByteLength:16}),v=new Uint8Array(b,2,4);b.resize(0);return createHash('sha256').update(v).digest('hex')})()",
        "(()=>{const h=createHash('sha256').update('abc');return h.digest({toString(){return 'hex'}})})()",
        "(()=>{const h=createHash('sha256');try{h.update('abc','hex')}catch{}return h.update('abc').digest('hex')})()",
        "(()=>{const h=createHash('sha256');try{h.copy({outputLength:1})}catch{}return h.update('abc').digest('hex')})()",
        "(()=>{const h=createHash('sha256').update('a');const x=h.copy().update('b').copy().update('c');return x.digest('hex')})()",
    ]
    source=PREFIX+'\n'.join(f'observe(()=>{case});' for case in cases)+SUFFIX
    with tempfile.TemporaryDirectory(prefix='mite-crypto-hash-') as directory:
        fixture=Path(directory)/'hash.cjs';fixture.write_text(source)
        results=[subprocess.run([exe,*(['--no-deprecation'] if exe==args.node else []),str(fixture)],capture_output=True,text=True,timeout=30) for exe in [args.node,args.mite]]
        for result in results:assert result.returncode==0 and not result.stderr,result
        expected,actual=[json.loads(result.stdout) for result in results]
        if expected!=actual:
            for case,node,mite in zip(cases,expected['rows'],actual['rows']):
                if node!=mite:print(case,node,mite)
            for key in expected:
                if key!='rows' and expected[key]!=actual.get(key):print(key,expected[key],actual.get(key))
            raise AssertionError('Hash differential mismatch')
        assert actual['identity'] and actual['preserved'] and actual['stress'] and all(actual['types']),actual
        assert actual['gets']==1 and actual['symbolError']=='TypeError',actual
        sha256='ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'
        assert actual['recovered']==actual['streamed']==actual['postStream']==actual['output']==sha256,actual
        fixture.write_text("const {createHash}=require('crypto');for(const create of [()=>createHash('shake128',{outputLength:0x80000000}),()=>createHash('shake128').copy({outputLength:0x80000000})]){try{create();throw Error('Limit accepted')}catch(e){if(e.code!=='ERR_MITE_UNSUPPORTED_API')throw e;}}console.log('HASH_LIMIT_REJECTED');")
        result=subprocess.run([args.mite,str(fixture)],capture_output=True,text=True,timeout=10)
        assert result.returncode==0 and not result.stderr and result.stdout.strip()=='HASH_LIMIT_REJECTED',result
        fixture.write_text("const h=require('crypto').createHash('sha256');h.on('data',()=>{console.log('HASH_DATA');throw Error('hash-data-fatal')});h.end('abc');")
        for executable in [args.node,args.mite]:
            result=subprocess.run([executable,str(fixture)],capture_output=True,text=True,timeout=10)
            assert result.returncode==1 and 'HASH_DATA' in result.stdout and 'hash-data-fatal' in result.stderr,result
        # Node 24.15 aborts on -0 at its native IsUint32 assertion. Keep it
        # isolated; Mite accepts numeric zero without inheriting the crash.
        fixture.write_text("console.log(JSON.stringify(require('crypto').createHash('shake128',{outputLength:-0}).digest('hex')))")
        node=subprocess.run([args.node,str(fixture)],capture_output=True,text=True,timeout=10)
        abort_status=134 if os.name=='nt' else -6
        assert (node.returncode==0 and node.stdout.strip()=='""') or (node.returncode==abort_status and 'IsUint32' in node.stderr),node
        mite=subprocess.run([args.mite,str(fixture)],capture_output=True,text=True,timeout=10)
        assert mite.returncode==0 and not mite.stderr and mite.stdout.strip()=='""',mite
        fixture.write_text("const h=require('crypto').createHash('sha256',{objectMode:true});h.on('error',e=>console.log(e.name+':'+e.code));h.end({invalid:true});")
        node=subprocess.run([args.node,str(fixture)],capture_output=True,text=True,timeout=10)
        assert (node.returncode==0 and node.stdout.strip()=='TypeError:ERR_INVALID_ARG_TYPE') or (node.returncode==abort_status and 'IsSharedArrayBuffer' in node.stderr),node
        mite=subprocess.run([args.mite,str(fixture)],capture_output=True,text=True,timeout=10)
        assert mite.returncode==0 and not mite.stderr and mite.stdout.strip()=='TypeError:ERR_INVALID_ARG_TYPE',mite
    print('INTENTIONAL DIFFERENCE: safe negative-zero SHAKE length and invalid object-mode JS error instead of Node 24.15 native assertions')
    print(f'PASS {len(cases)} hash comparisons, independent copies, coercion recovery, ESM/CJS identity, Transform/pipeline and allocation stress')

if __name__=='__main__':main()
