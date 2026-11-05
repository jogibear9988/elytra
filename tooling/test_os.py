#!/usr/bin/env python3
"""Compare OS identities, coercion, native observations and live environment semantics."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

SOURCE=r'''
(async()=>{
const os=require('node:os');
const ns=await import('node:os');
const stable={};
for(const name of ['arch','platform','endianness','type','release','version','machine','hostname','homedir','tmpdir','totalmem','availableParallelism'])stable[name]=os[name]();
const methods=['arch','platform','endianness','type','release','version','machine','hostname','homedir','tmpdir','totalmem','availableParallelism','freemem','uptime','loadavg'];
let traps=0;const bomb=new Proxy({},{get(){traps++;throw Error('argument coerced')}});
const argumentChecks=methods.map(name=>{const result=Reflect.apply(os[name],bomb,[bomb,Symbol('ignored')]);return name==='loadavg'?Array.isArray(result):typeof result===typeof os[name]()});
const coercionChecks=['arch','platform','endianness','type','release','version','machine','hostname','homedir','tmpdir','totalmem','availableParallelism'].map(name=>String(os[name])===String(os[name]()));
const retained=os.type;os.type=()=> 'changed';const retainedCoercion=String(retained)===stable.type;os.type=retained;
const avg=os.loadavg(),next=os.loadavg();avg[0]=-1;
const free=os.freemem(),up=os.uptime();
const descriptors={};for(const key of ['EOL','devNull']){const d=Object.getOwnPropertyDescriptor(os,key);descriptors[key]={value:d.value,writable:d.writable,enumerable:d.enumerable,configurable:d.configurable};}
console.log(JSON.stringify({stable,argumentChecks,traps,coercionChecks,retainedCoercion,descriptors,
 identity:os===require('os') && ns.default===os && ns.arch===os.arch && ns.tmpdir===os.tmpdir && ns.EOL===os.EOL,
 dynamic:{freeValid:Number.isFinite(free)&&free>=0&&free<=os.totalmem(),uptimeValid:Number.isFinite(up)&&up>=0,
 loadValid:next!==avg && next.length===3 && next.every(v=>Number.isFinite(v)&&v>=0)},uptime:up}));
const keys=['TMPDIR','TMP','TEMP','HOME'];const previous=Object.fromEntries(keys.map(key=>[key,process.env[key]]));
const observations=[];
try{
 for(const values of [[],['/'],['//'],['relative///'],['','/second/','/third/'],['','','/third/'],['','',''],['/tmp/ä❄/'],['/long/'+ 'a'.repeat(8192)+'/']]){
  for(const key of ['TMPDIR','TMP','TEMP'])delete process.env[key];values.forEach((v,i)=>process.env[['TMPDIR','TMP','TEMP'][i]]=v);
  observations.push(os.tmpdir());
 }
 for(const value of ['/home/ä❄','relative/','', '/long/'+ 'b'.repeat(1024)]){process.env.HOME=value;try{observations.push(os.homedir());}catch(error){observations.push({name:error.name,code:error.code,errno:error.errno,syscall:error.syscall,info:error.info});}}
 process.env.HOME='/native-home-control';
 const originalEnv=process.env;process.env={TMPDIR:'/fake-global-environment',HOME:'/fake-global-home'};
 try{observations.push(os.tmpdir(),os.homedir());}finally{process.env=originalEnv;}
}finally{for(const key of keys){if(previous[key]===undefined)delete process.env[key];else process.env[key]=previous[key];}}
console.log(JSON.stringify(observations));
})().catch(error=>{console.error(error);process.exitCode=1});
'''
ESM="""import os, * as ns from 'node:os';import {arch,tmpdir,EOL} from 'os';
console.log(JSON.stringify(os.arch===arch && os.tmpdir===tmpdir && os.EOL===EOL && (await import('os'))===ns));
"""
UNSUPPORTED="""const os=require('os');for(const name of ['constants']){try{if(name==='constants')void os.constants;else os[name]();throw Error('unsupported succeeded')}catch(error){if(error.code!=='ERR_MITE_UNSUPPORTED_API')throw error;}}console.log('explicit unsupported OS APIs');"""

def run(executable,fixture):
    result=subprocess.run([executable,str(fixture)],capture_output=True,text=True,timeout=20)
    if result.returncode or result.stderr:raise AssertionError(result)
    return [json.loads(line) for line in result.stdout.splitlines()]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite',required=True)
    parser.add_argument('--node',default=shutil.which('node'))
    args=parser.parse_args()
    if not args.node:parser.error('Node is required')
    with tempfile.TemporaryDirectory(prefix='mite-os-') as directory:
        root=Path(directory);fixture=root/'os.cjs';fixture.write_text(SOURCE)
        start=time.monotonic();expected=run(args.node,fixture);actual=run(args.mite,fixture);elapsed=time.monotonic()-start
        for row in [expected,actual]:
            assert len(row)==2,row
            assert row[0]['dynamic']==dict(freeValid=True,uptimeValid=True,loadValid=True),row[0]
            assert row[0]['identity'] and row[0]['retainedCoercion'] and row[0]['traps']==0,row[0]
            assert all(row[0]['argumentChecks']) and all(row[0]['coercionChecks']),row[0]
        up=[row[0].pop('uptime') for row in [expected,actual]]
        assert abs(up[0]-up[1])<=elapsed+2,up
        assert expected==actual,(expected,actual)
        esm=root/'os.mjs';esm.write_text(ESM)
        assert run(args.node,esm)==run(args.mite,esm)==[True]
        unsupported=root/'unsupported.cjs';unsupported.write_text(UNSUPPORTED)
        result=subprocess.run([args.mite,str(unsupported)],capture_output=True,text=True,timeout=10)
        assert result.returncode==0 and result.stdout.strip()=='explicit unsupported OS APIs' and not result.stderr,result
    print('PASS OS native identity/data, 15 argument/receiver checks, 12 coercions, module identities, descriptors, 15 environment observations and explicit unsupported APIs')

if __name__=='__main__':main()
