#!/usr/bin/env python3
"""Compare buffered native executions with independent Python children and Node."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


SOURCE = r"""
(async()=>{
const cp=require('node:child_process'),{execFile,ChildProcess}=cp,{createHash}=require('node:crypto');
const python=PYTHON,cwd=CWD,rows={};
const ns=await import('node:child_process');
if(ns.execFile!==execFile || ns.default!==cp || require('child_process')!==cp)throw Error('ExecFile export identity');
function value(output){
 const bytes=Buffer.isBuffer(output)?output:Buffer.from(output);
 return {buffer:Buffer.isBuffer(output),length:bytes.length,hash:createHash('sha256').update(bytes).digest('hex'),head:bytes.subarray(0,24).toString('hex')};
}
async function execute(file,args,options={},input,termination=false){
 return await new Promise((resolve,reject)=>{
  let inside=true,count=0,result;const events=[];
  const child=execFile(file,args,options,(error,stdout,stderr)=>{
   if(inside)throw Error('Synchronous execFile callback');
   count++;events.push('callback');
   result={stdout:value(stdout),stderr:value(stderr),error:error?{name:error.name,code:error.code,killed:error.killed,signal:error.signal,cmd:error.cmd,
     errno:error.errno,path:error.path,syscall:error.syscall,spawnargs:error.spawnargs,message:error.errno===undefined?error.message:undefined}:null};
  });
  if(!(child instanceof ChildProcess) || !child.stdin || !child.stdout || !child.stderr)throw Error('ExecFile child/stdio identity');
  child.on('error',()=>events.push('error'));
  child.on('close',(code,signal)=>{
   events.push('close');if(count!==1)reject(Error('ExecFile callback count '+count));
   else resolve({...result,events,...(termination?{termination:[child.killed,code,signal]}:{})});
  });
  inside=false;
  child.stdin.on('error',e=>{if(e.code!=='EPIPE')reject(e)});
  child.stdin.end(input);
 });
}
rows.text=await execute(python,['-c',"import os,sys;sys.stdout.buffer.write(('é文🪲 '+sys.argv[1]+' '+os.environ['EXEC_TOKEN']).encode());sys.stderr.buffer.write(b'ERR')",'hello world'],{cwd,env:{...process.env,EXEC_TOKEN:'ok'}});
rows.split=await execute(python,['-c',"import os,time;data='é文🪲'.encode();[(os.write(1,bytes([b])),time.sleep(.002)) for b in data]"]);
rows.binary=await execute(python,['-c','import sys;sys.stdout.buffer.write(bytes(range(256)));sys.stderr.buffer.write(bytes([255,0,26]))'],{encoding:'buffer'});
rows.invalidEncoding=await execute(python,['-c',"print('abc',end='')"],{encoding:'unknown-encoding'});
rows.nullEncoding=await execute(python,['-c',"print('abc',end='')"],{encoding:null});
rows.hex=await execute(python,['-c',"import sys;sys.stdout.buffer.write(bytes([0,255,26]))"],{encoding:'hex'});
rows.nonzero=await execute(python,['-c',"import sys;sys.stdout.buffer.write(b'OUT');sys.stderr.buffer.write(b'ERR');sys.exit(7)"]);
rows.missing=await execute('mite-execfile-missing-0946',['one']);
rows.cwdFailure=await execute(python,['-c','pass'],{cwd:cwd+'/missing'});
rows.stdoutLimit=await execute(python,['-c',"print('abcdef',end='')"],{maxBuffer:3});
rows.stderrLimit=await execute(python,['-c',"import sys;sys.stderr.write('abcdef')"],{maxBuffer:3});
rows.limitKill=await execute(python,['-c',"import sys,time;sys.stdout.write('abcdef');sys.stdout.flush();time.sleep(10)"],{maxBuffer:3},undefined,true);
rows.zeroLimit=await execute(python,['-c',"print('abc',end='')"],{maxBuffer:0});
rows.nullLimit=await execute(python,['-c',"print('abc',end='')"],{maxBuffer:null});
rows.fractionLimit=await execute(python,['-c',"print('abcdef',end='')"],{maxBuffer:1.5});
rows.unicodeLimit=await execute(python,['-c',"import sys;sys.stdout.buffer.write('文🪲a'.encode())"],{maxBuffer:5});
rows.defaultLimit=await execute(python,['-c','import sys;sys.stdout.buffer.write(b"x"*(1024*1024+23))'],{encoding:'buffer'});
rows.undefinedLimit=await execute(python,['-c','import sys;sys.stdout.buffer.write(b"x"*(1024*1024+23))'],{encoding:'buffer',maxBuffer:undefined});
const payload=Buffer.alloc(1024*1024+37);for(let i=0;i<payload.length;i++)payload[i]=i%251;
rows.stdin=await execute(python,['-c','import sys;sys.stdout.buffer.write(sys.stdin.buffer.read())'],{encoding:'buffer',maxBuffer:Infinity},payload);
if(rows.stdin.stdout.hash!==createHash('sha256').update(payload).digest('hex'))throw Error('ExecFile stdin capture corrupt');
rows.timeout=await execute(python,['-c','import time;time.sleep(10)'],{timeout:80});
rows.timeoutKill=await execute(python,['-c','import time;time.sleep(10)'],{timeout:80,killSignal:'SIGKILL'});
rows.coercedFlags=await execute(python,['-c','pass'],{windowsHide:1,windowsVerbatimArguments:0,argv0:'ignored by execFile'});
rows.overload=await new Promise((resolve,reject)=>execFile(python,['-c',"print('args callback',end='')"],(e,out,err)=>e?reject(e):resolve([out,err])));
rows.optionsOverload=await new Promise((resolve,reject)=>{const child=execFile(python,{env:{...process.env}},(e,out,err)=>e?reject(e):resolve([out,err]));child.stdin.end();});
rows.callbackOverload=await new Promise((resolve,reject)=>{const child=execFile(python,(e,out,err)=>e?reject(e):resolve([out,err]));child.stdin.end();});
rows.noCallback=await new Promise((resolve,reject)=>{const child=execFile(python,['-c','import sys;sys.exit(37)']);child.on('error',reject);child.on('close',(code,signal)=>resolve([code,signal]));});
console.log(JSON.stringify(rows));
})().catch(error=>{console.error(error);process.exitCode=1});
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required")
    with tempfile.TemporaryDirectory(prefix="mite-execfile-") as directory:
        root = Path(directory)
        work = root / "cwd é文🪲"
        work.mkdir()
        fixture = root / "execute.cjs"
        fixture.write_text(
            SOURCE.replace("PYTHON", json.dumps(sys.executable)).replace(
                "CWD", json.dumps(str(work))
            ),
            encoding="utf-8",
        )
        observations = []
        for exe in [args.node, args.mite]:
            result = subprocess.run(
                [exe, str(fixture)],
                input="",
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=30,
            )
            assert result.returncode == 0 and not result.stderr, result
            observations.append(json.loads(result.stdout))
        assert observations[0] == observations[1], {
            key: [observations[0][key], observations[1][key]]
            for key in observations[0]
            if observations[0][key] != observations[1][key]
        }
        cases = [
            "()",
            "('')",
            "(null)",
            "(42)",
            "('x\\0y')",
            "('x',['a\\0b'])",
            "('x',[],[])",
            "('x',[],1)",
            "('x',[],{},1)",
            "('x',[],{},null)",
            "('x',[],{maxBuffer:-1})",
            "('x',[],{maxBuffer:NaN})",
            "('x',[],{maxBuffer:'1'})",
            "('x',[],{maxBuffer:true})",
            "('x',[],{maxBuffer:1.5})",
            "('x',[],{maxBuffer:Infinity})",
            "('x',[],{maxBuffer:null})",
            "('x',[],{maxBuffer:undefined})",
            "('x',[],{timeout:-1})",
            "('x',[],{timeout:'1'})",
            "('x',[],{timeout:1.5})",
            "('x',[],{timeout:NaN})",
            "('x',[],{timeout:null})",
            "('x',[],{killSignal:null})",
            "('x',[],{killSignal:'BAD'})",
            "('x',[],{argv0:1})",
            "('x',[],{argv0:'a\\0b'})",
            "('x',[],{windowsHide:1,windowsVerbatimArguments:0})",
            "('x',[],{encoding:1})",
        ]
        validation = "const {execFile}=require('child_process'),rows=[];function observe(fn){try{fn();rows.push('accepted')}catch(e){rows.push([e.name,e.code])}}\n"
        validation += "\n".join("observe(()=>execFile" + case + ");" for case in cases)
        validation += "\nconsole.log(JSON.stringify(rows));"
        fixture.write_text(validation, encoding="utf-8")
        observations = []
        for exe in [args.node, args.mite]:
            result = subprocess.run(
                [exe, str(fixture)], capture_output=True, text=True, encoding="utf-8", timeout=10
            )
            assert result.returncode == 0 and not result.stderr, result
            observations.append(json.loads(result.stdout))
        assert observations[0] == observations[1], list(zip(cases, *observations))
        fixture.write_text(
            "const {execFile}=require('child_process');for(const options of [{shell:true},{signal:{}},{uid:0},{gid:0}]){let caught=false;try{execFile('x',[],options)}catch(e){if(e.code!=='ERR_MITE_UNSUPPORTED_API')throw e;caught=true}if(!caught)throw Error('Unsupported execFile mode accepted')}console.log('POLICY_OK');",
            encoding="utf-8",
        )
        result = subprocess.run(
            [args.mite, str(fixture)], capture_output=True, text=True, encoding="utf-8", timeout=10
        )
        assert (
            result.returncode == 0 and not result.stderr and result.stdout.strip() == "POLICY_OK"
        ), result
        fixture.write_text(
            'require("child_process").execFile('
            + json.dumps(sys.executable)
            + ',["-c","pass"],()=>{throw Error("execfile-callback-fatal")});',
            encoding="utf-8",
        )
        for exe in [args.node, args.mite]:
            result = subprocess.run(
                [exe, str(fixture)], capture_output=True, text=True, encoding="utf-8", timeout=10
            )
            assert result.returncode == 1 and "execfile-callback-fatal" in result.stderr, result
    print(
        "PASS execFile native buffered output, encoding, Unicode, failures, byte limits, stdin, timeout, overloads and export identity against Node"
    )


if __name__ == "__main__":
    main()
