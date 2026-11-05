#!/usr/bin/env python3
"""Compare native CPU/user snapshots and userInfo encoding/option semantics."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

SOURCE=r'''
(async()=>{
const os=require('node:os'),{Buffer}=require('node:buffer');
const normalize=user=>({keys:Object.keys(user),nullPrototype:Object.getPrototypeOf(user)===null,
 value:Object.fromEntries(Object.entries(user).map(([key,value])=>[key,Buffer.isBuffer(value)?{buffer:[...value]}:value]))});
const rows=[];
const encodings=[undefined,null,'','utf8','UTF8','utf-8','buffer','BUFFER','BuFfEr','hex','HEX','ascii','ASCII','latin1','binary','base64','base64url','ucs2','ucs-2','utf16le','utf-16le','bad','raw','latin-1','hex\0bad','bUffER\0',1,true,{},[],Symbol('encoding'),new String('hex')];
for(const encoding of encodings){try{rows.push({value:normalize(os.userInfo({encoding}))});}catch(error){rows.push({error:error.name,code:error.code});}}
for(const options of [undefined,null,false,42,'hex',1n,Symbol('options'),()=>{},[],Object.create({encoding:'hex'})]){
 try{rows.push({value:normalize(os.userInfo(options))});}catch(error){rows.push({error:error.name,code:error.code});}
}
let traps=0;
const encoding={toString(){traps++;throw Error('Must not coerce encoding')},valueOf(){traps++;throw Error('Must not coerce encoding')}};
let reads=0;rows.push({value:normalize(os.userInfo({get encoding(){reads++;return encoding}})),reads,traps});
const trace=[];const proxy=new Proxy({},{get(target,key){trace.push(String(key));return 'buffer'}});rows.push({value:normalize(os.userInfo(proxy)),trace});
const thrown={marker:true};let preserved=false;
try{os.userInfo({get encoding(){throw thrown}})}catch(error){preserved=error===thrown}
const callable=()=>{};Object.defineProperty(callable,'encoding',{get(){throw Error('Must ignore callable options')}});
rows.push({value:normalize(os.userInfo(callable)),preserved});
const pair=Proxy.revocable({},{});pair.revoke();try{os.userInfo(pair.proxy);rows.push({unexpected:true})}catch(error){rows.push({error:error.name,code:error.code})}
const old=process.env.HOME;
try{process.env.HOME='/user-info-must-use-native-account';rows.push(normalize(os.userInfo()));}
finally{if(old===undefined)delete process.env.HOME;else process.env.HOME=old;}
const a=os.userInfo({encoding:'buffer'}),b=os.userInfo({encoding:'buffer'});a.username[0]^=255;
rows.push({fresh:a!==b && a.username!==b.username && a.homedir!==b.homedir && a.username[0]!==b.username[0] && Buffer.isBuffer(a.username)});
const namespace=await import('node:os');rows.push({identity:namespace.userInfo===os.userInfo && namespace.cpus===os.cpus});
console.log(JSON.stringify(rows));
const cpus=os.cpus(),later=os.cpus();let argumentTraps=0;
const bomb=new Proxy({},{get(){argumentTraps++;throw Error('CPU arguments must be ignored')}});
const args=Reflect.apply(os.cpus,bomb,[bomb]);
const fields=['model','speed','times'],timeFields=['user','nice','sys','idle','irq'];
const valid=Array.isArray(cpus)&&cpus.length>0&&cpus.every(cpu=>
 Object.keys(cpu).join()===fields.join() && Object.getPrototypeOf(cpu)===Object.prototype && typeof cpu.model==='string' &&
 Number.isFinite(cpu.speed)&&cpu.speed>=0&&Object.keys(cpu.times).join()===timeFields.join() && Object.getPrototypeOf(cpu.times)===Object.prototype &&
 timeFields.every(key=>Number.isSafeInteger(cpu.times[key])&&cpu.times[key]>=0));
const counters=cpus.map(cpu=>({...cpu.times}));
cpus[0].times.user=-1;cpus[0].model='mutated';
const fresh=cpus!==later && cpus[0]!==later[0] && cpus[0].times!==later[0].times && later[0].times.user>=0 && later[0].model!=='mutated';
console.log(JSON.stringify({models:later.map(cpu=>cpu.model),counters,valid,fresh,argumentTraps,argsValid:Array.isArray(args)&&args.length===cpus.length,
 monotonic:later.every((cpu,i)=>timeFields.every(key=>cpu.times[key]>=counters[i][key]))}));
})().catch(error=>{console.error(error);process.exitCode=1});
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite',required=True)
    parser.add_argument('--node',default=shutil.which('node'))
    args=parser.parse_args()
    if not args.node:parser.error('Node is required')
    with tempfile.TemporaryDirectory(prefix='mite-os-data-') as directory:
        fixture=Path(directory)/'os-data.cjs';fixture.write_text(SOURCE)
        results=[subprocess.run([exe,str(fixture)],capture_output=True,text=True,timeout=20) for exe in [args.node,args.mite]]
    for result in results:assert result.returncode==0 and not result.stderr,result
    expected,actual=[[json.loads(line) for line in result.stdout.splitlines()] for result in results]
    assert len(expected)==len(actual)==2,(expected,actual)
    assert expected[0]==actual[0],[(i,node,mite) for i,(node,mite) in enumerate(zip(expected[0],actual[0])) if node!=mite]
    for result in [expected,actual]:
        assert result[1]['valid'] and result[1]['fresh'] and result[1]['monotonic'] and result[1]['argsValid'] and result[1]['argumentTraps']==0,result[1]
    assert expected[1]['models']==actual[1]['models'],(expected[1]['models'],actual[1]['models'])
    for earlier,later in zip(expected[1]['counters'],actual[1]['counters']):
        assert all(later[key]>=value for key,value in earlier.items()),(earlier,later)
    print(f"PASS {len(expected[0])} userInfo encoding/options/ownership comparisons and {len(actual[1]['models'])} native CPU records with advancing counters/fresh snapshots")

if __name__=='__main__':main()
