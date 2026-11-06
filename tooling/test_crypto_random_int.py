#!/usr/bin/env python3
"""Differential integer-random contracts and deterministic rejection boundary checks."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(executable, path):
    result = subprocess.run([executable, str(path)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0 and not result.stderr, result
    return json.loads(result.stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite', required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    args = parser.parse_args()
    if not args.node:
        parser.error('Node is required')
    cases = []
    for value in ['undefined', 'null', 'true', '"3"', '1n', 'Symbol()', '{}', '[]',
                  'NaN', 'Infinity', '1.5', 'Number.MAX_SAFE_INTEGER+1']:
        cases.extend([f'({value})', f'({value},5)', f'(0,{value})'])
    cases.extend(['()', '(0)', '(-1)', '(4,4)', '(5,4)', '(0,2**48)',
                  '(-(2**47),2**47)', '(Number.MIN_SAFE_INTEGER,Number.MAX_SAFE_INTEGER)',
                  '(0,1,null)', '("bad",1,null)', '(1,null)', '(1,1,false)'])
    source = r'''
(async()=>{
const crypto=require('node:crypto'),randomInt=crypto.randomInt,rows=[];
function observe(fn){try{rows.push({type:typeof fn()});}catch(e){rows.push({error:e.name,code:e.code});}}
'''
    source += '\n'.join(f'observe(()=>randomInt{case});' for case in cases)
    source += r'''
const ns=await import('node:crypto');
if(ns.randomInt!==randomInt || ns.default!==crypto || require('crypto')!==crypto)throw Error('Module identity');
Math.random=()=>{throw Error('Insecure random source')};
const ranges=[[0,1],[-1,0],[-10,10],[0,2**48-1],[-(2**47),1],
 [Number.MAX_SAFE_INTEGER-1,Number.MAX_SAFE_INTEGER],
 [Number.MIN_SAFE_INTEGER,Number.MIN_SAFE_INTEGER+1]];
let checks=0;
for(const [min,max] of ranges){
 for(let i=0;i<64;i++){
  const n=randomInt(min,max);
  if(!Number.isSafeInteger(n) || n<min || n>=max)throw Error('Range escape');
  checks++;
 }
}
if(randomInt(1)!==0 || randomInt(-1,0)!==-1)throw Error('Exclusive endpoint');
const values=new Set(Array.from({length:128},()=>randomInt(7)));
if(values.size!==7)throw Error('Degenerate small range');
let calls=0;
await Promise.all(Array.from({length:48},(_,i)=>new Promise((resolve,reject)=>{
 let returned=false;
 const callback=function(error,n){'use strict';
  if(error!==undefined)return reject(error || Error('Expected undefined error'));
  if(!returned || this!==undefined || arguments.length!==2 || !Number.isSafeInteger(n) || n<i || n>=i+7)return reject(Error('Callback contract'));
  calls++;resolve();
 };
 const result=i%2?randomInt(i,i+7,callback):randomInt(7,function(e,n){callback(e,n+i)});
 if(result!==undefined)throw Error('Callback return');returned=true;
})));
console.log(JSON.stringify({rows,checks,calls,name:randomInt.name,length:randomInt.length}));
})().catch(e=>{console.error(e);process.exitCode=1});
'''
    # The factory is evaluated only in a Node test VM. No entropy override or
    # host-factory access is added to the shipping Mite runtime.
    controlled = r'''
const assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs');
const factory=vm.runInNewContext(fs.readFileSync(SOURCE,'utf8'));
const space=2**48,range=10,limit=space-space%range;
function bytes(n){const a=new Uint8Array(6);for(let i=5;i>=0;i--){a[i]=n%256;n=Math.floor(n/256)}return a;}
let pending=[],reads=0,values=[space-1,limit,limit-1];
const token={failure:true};
const api=factory({Buffer,stream:{Transform:function(){}},native:{random(size,callback){
 assert.equal(size,6);reads++;
 const value=values.shift();assert.notEqual(value,undefined);
 if(callback)pending.push(()=>callback(value===token?token:undefined,value===token?undefined:bytes(value)));
 else return bytes(value);
}}});
assert.equal(api.randomInt(-5,5),(limit-1)%10-5);assert.equal(reads,3);
values=[limit,0];let result,calls=0;
assert.equal(api.randomInt(-5,5,(e,n)=>{assert.equal(e,undefined);result=n;calls++}),undefined);
assert.equal(calls,0);pending.shift()();assert.equal(calls,0);pending.shift()();
assert.equal(result,-5);assert.equal(calls,1);
values=[token];api.randomInt(5,e=>{assert.equal(e,token);calls++});pending.shift()();assert.equal(calls,2);
assert.equal(pending.length,0);assert.equal(values.length,0);
console.log(JSON.stringify({rejection:true,errorIdentity:true}));
'''.replace('SOURCE', json.dumps(str(ROOT / 'mite/modules/generated/crypto.js')))
    with tempfile.TemporaryDirectory(prefix='mite-random-int-') as directory:
        root = Path(directory)
        fixture = root / 'integer.cjs'
        fixture.write_text(source)
        expected, actual = [run(exe, fixture) for exe in [args.node, args.mite]]
        assert expected == actual, (expected, actual)
        assert actual['calls'] == 48 and actual['checks'] == 448, actual
        fixture.write_text(controlled)
        assert run(args.node, fixture) == {'rejection': True, 'errorIdentity': True}
        fixture.write_text("require('crypto').randomInt(10,()=>{console.log('INT_CALLBACK');throw Error('int-fatal')});")
        for exe in [args.node, args.mite]:
            result = subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=10)
            assert result.returncode == 1 and 'INT_CALLBACK' in result.stdout and 'int-fatal' in result.stderr, result
    print(f'PASS {len(cases)} validation comparisons, 448 bounded native samples, 48 async calls, module identity, callback failure and deterministic rejection/error boundaries')


if __name__ == '__main__':
    main()
