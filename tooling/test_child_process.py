#!/usr/bin/env python3
"""Compare native process/pipe workflows using an independent Python child."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


SOURCE = r'''
(async()=>{
const cp=require('node:child_process'),{spawn,ChildProcess}=cp,{once}=require('node:events'),{createHash}=require('node:crypto');
const python=PYTHON,cwd=CWD,own=OWN;
const ns=await import('node:child_process');
if(ns.spawn!==spawn||ns.ChildProcess!==ChildProcess||ns.default!==cp||require('child_process')!==cp)throw Error('Process module identity');
async function execute(command,args,options={},input){
 const child=spawn(command,args,options),events=[],out=[],err=[];
 if(!(child instanceof ChildProcess)||child.connected!==false||typeof child.kill!=='function'||child.ref()!==undefined||child.unref()!==undefined||child.ref()!==undefined)throw Error('Child identity');
 child.on('spawn',()=>events.push('spawn'));
 child.on('exit',(code,signal)=>events.push(['exit',code,signal]));
 let failure=null;child.on('error',error=>{failure={name:error.name,code:error.code,errno:error.errno,path:error.path,syscall:error.syscall,spawnargs:error.spawnargs};events.push('error')});
 if(child.stdout){child.stdout.on('error',error=>{throw error});if(input)child.stdout.pause();child.stdout.on('data',chunk=>out.push(chunk));if(input)setTimeout(()=>{if(out.length)throw Error('Paused output leaked data');child.stdout.resume()},20);}
 if(child.stderr){child.stderr.on('error',error=>{throw error});child.stderr.on('data',chunk=>err.push(chunk));}
 const closing=new Promise(resolve=>child.once('close',(code,signal)=>{events.push(['close',code,signal]);resolve([code,signal])}));
 if(child.stdin){
  child.stdin.on('error',error=>{if(error.code!=='EPIPE')throw error});
  if(input){
   child.stdin.cork();let complete;const written=new Promise((resolve,reject)=>complete=e=>e?reject(e):resolve());
   if(child.stdin.write(input,complete)!==false)throw Error('Expected buffered stdin pressure');
   const drained=once(child.stdin,'drain');child.stdin.uncork();await Promise.all([written,drained]);child.stdin.end();
  }else child.stdin.end();
 }
 const closed=await closing;
 return {events,closed,failure,out:Buffer.concat(out).toString('hex'),err:Buffer.concat(err).toString(),exitCode:child.exitCode,signalCode:child.signalCode,killed:child.killed,pid:typeof child.pid,stdio:child.stdio.map(stream=>stream===null),spawnfile:child.spawnfile===command,spawnargs:child.spawnargs.slice(1)};
}
const payload=Buffer.alloc(1024*1024+37);for(let i=0;i<payload.length;i++)payload[i]=i%251;
const code="import sys,os,json;data=sys.stdin.buffer.read();sys.stdout.buffer.write(data);sys.stderr.write(json.dumps({'args':sys.argv[1:],'cwd':os.getcwd(),'env':os.environ.get('MITE_CHILD_TOKEN')},ensure_ascii=False));sys.exit(7)";
const echo=await execute(python,['-c',code,'hello world','"quoted"','back\\slash\\','文🪲'],{cwd:new URL('file://'+URLCWD),env:{...process.env,MITE_CHILD_TOKEN:'é文🪲',MITE_CHILD_ABSENT:undefined}},payload);
if(echo.out!==payload.toString('hex'))throw Error('Pipe data corruption');
echo.out={length:payload.length,hash:createHash('sha256').update(Buffer.from(echo.out,'hex')).digest('hex')};echo.err=JSON.parse(echo.err);
const missing=await execute('mite-no-such-command-6a851',['arg']);
const badCwd=await execute(python,['-c','pass'],{cwd:cwd+'/missing'});
const stringCwd=await execute(python,['-c','import os;print(os.getcwd())'],{cwd});
const emptyCwd=await execute(python,['-c','pass'],{cwd:'',stdio:'ignore'});
const ignored=await execute(python,['-c','pass'],{stdio:'ignore'});
const inherited=await execute(python,['-c',"print('INHERITED')"],{stdio:'inherit'});
const argv0=await execute(own,['-e','console.log(process.argv0)'],{argv0:'custom argv0'});
const coerced=await execute(own,['-e','console.log(JSON.stringify(process.argv.slice(1)))',12,Symbol('s'),null]);
const killed=await new Promise((resolve,reject)=>{
 const child=spawn(python,['-c',"import sys,time;print('READY',flush=True);time.sleep(10)"]);
 child.on('error',reject);child.stdout.setEncoding('utf8');let ready='';
 child.stdout.on('data',text=>{ready+=text;if(ready.includes('READY')&&!child.killed){if(!child.kill('SIGTERM'))reject(Error('Kill failed'))}});
 child.on('close',(code,signal)=>resolve({code,signal,killed:child.killed,again:child.kill()}));
});
const timeout=await new Promise((resolve,reject)=>{
 const child=spawn(python,['-c','import time;time.sleep(10)'],{timeout:30});child.on('error',reject);
 child.on('close',(code,signal)=>resolve({code,signal,killed:child.killed}));
});
const stress=await Promise.all(Array.from({length:8},(_,i)=>execute(python,['-c',`import sys;sys.stdout.write('child-${i}')`])));
if(!stress.every((row,i)=>Buffer.from(row.out,'hex').toString()===`child-${i}`&&row.closed[0]===0&&row.events[0]==='spawn'))throw Error('Concurrent child isolation');
const unconsumed=await new Promise((resolve,reject)=>{
 const child=spawn(python,['-c',"print('unconsumed')"]);child.on('error',reject);child.on('close',(code,signal)=>resolve([code,signal]));
});
console.log(JSON.stringify({echo,missing,badCwd,stringCwd,emptyCwd,ignored,inherited,argv0,coerced,killed,timeout,stress:true,unconsumed}));
})().catch(error=>{console.error(error);process.exitCode=1});
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite', required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    args = parser.parse_args()
    if not args.node:
        parser.error('Node is required')
    with tempfile.TemporaryDirectory(prefix='mite-child-') as directory:
        root = Path(directory)
        work = root / 'work é文🪲'
        work.mkdir()
        results = []
        for executable in [args.node, str(Path(args.mite).resolve())]:
            fixture = root / 'child.cjs'
            source = SOURCE.replace('PYTHON', json.dumps(sys.executable)).replace('URLCWD', json.dumps(work.as_uri()[7:])).replace('CWD', json.dumps(str(work))).replace('OWN', json.dumps(executable))
            fixture.write_text(source, encoding='utf-8')
            result = subprocess.run([executable, str(fixture)], capture_output=True, text=True, encoding='utf-8', timeout=30)
            assert result.returncode == 0 and not result.stderr, result
            lines = result.stdout.splitlines()
            assert lines[:-1] == ['INHERITED'], lines[:-1]
            results.append(json.loads(lines[-1]))
        assert results[0] == results[1], {key: [results[0][key], results[1][key]] for key in results[0] if results[0][key] != results[1][key]}
        fixture = root / 'validate.cjs'
        cases = ["()", "('')", "(null)", "(42)", "('x\\0y')", "('x',['a\\0b'])",
                 "('x',[],{cwd:42})", "('x',[],{cwd:'a\\0b'})", "('x',[],{env:{x:'a\\0b'}})",
                 "('x',[],{detached:1})", "('x',[],{windowsHide:1})", "('x',[],{argv0:1})",
                 "('x',[],{timeout:-1})", "('x',[],{timeout:'1'})", "('x',[],{timeout:NaN})",
                 "('x',[],{timeout:null})", "('x',[Symbol()])", "('x',[],{env:{x:Symbol()}})"]
        validation = "const {spawn}=require('node:child_process'),rows=[];function observe(fn){try{const child=fn();child.on('error',()=>{});rows.push('ACCEPTED')}catch(e){rows.push([e.name,e.code])}}\n"
        validation += '\n'.join(f'observe(()=>spawn{case});' for case in cases)
        validation += '\nconsole.log(JSON.stringify(rows));'
        fixture.write_text(validation, encoding='utf-8')
        observations = []
        for exe in [args.node, args.mite]:
            result = subprocess.run([exe, str(fixture)], capture_output=True, text=True, encoding='utf-8', timeout=10)
            assert result.returncode == 0 and not result.stderr, result
            observations.append(json.loads(result.stdout))
        assert observations[0] == observations[1], list(zip(cases, *observations))
        assert [i for i,row in enumerate(observations[0]) if row=='ACCEPTED'] == [15,16], observations
        fixture.write_text("const cp=require('child_process');for(const options of [{shell:true},{signal:{}},{uid:0},{gid:0},{stdio:'ipc'},{stdio:['pipe','pipe','pipe','pipe']}]){let caught=false;try{cp.spawn('x',[],options)}catch(e){if(e.code!=='ERR_MITE_UNSUPPORTED_API')throw e;caught=true}if(!caught)throw Error('Unsupported process option accepted')}for(const name of ['exec','execFile','fork','spawnSync'])if(cp[name]!==undefined)throw Error('Unexpected process export');console.log('POLICY_OK');", encoding='utf-8')
        result = subprocess.run([args.mite, str(fixture)], capture_output=True, text=True, timeout=10)
        assert result.returncode == 0 and not result.stderr and result.stdout.strip() == 'POLICY_OK', result
        fixture.write_text("require('child_process').spawn(" + json.dumps(sys.executable) + ",['-c','pass'],{stdio:'ignore'}).on('exit',()=>{console.log('CHILD_EXIT');throw Error('child-exit-fatal')});", encoding='utf-8')
        for exe in [args.node, args.mite]:
            result = subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=10)
            assert result.returncode == 1 and result.stdout.strip() == 'CHILD_EXIT' and 'child-exit-fatal' in result.stderr, result
        helper = root / 'detached.py'
        helper.write_text("import pathlib,sys,time\nack=pathlib.Path(sys.argv[1]);result=pathlib.Path(sys.argv[2])\nfor _ in range(400):\n if ack.exists():\n  result.write_text('CHILD_SURVIVED');break\n time.sleep(.02)\n", encoding='utf-8')
        for index, exe in enumerate([args.node, args.mite]):
            ack, marker = root / f'ack-{index}', root / f'marker-{index}'
            fixture.write_text("const child=require('child_process').spawn(" + json.dumps(sys.executable) + "," + json.dumps([str(helper), str(ack), str(marker)]) + ",{detached:true,stdio:'ignore'});child.on('error',e=>{throw e});child.unref();console.log('UNREF_DONE');", encoding='utf-8')
            try:
                result = subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=3)
                assert result.returncode == 0 and not result.stderr and result.stdout.strip() == 'UNREF_DONE', result
            finally:
                ack.touch()  # Release the owned helper even if the parent fails.
            deadline = time.monotonic() + 5
            while not marker.exists() and time.monotonic() < deadline:
                time.sleep(.02)
            assert marker.read_text() == 'CHILD_SURVIVED', marker
    print('PASS native spawn: Unicode args/cwd/env, 1 MiB pipes, exit/close/error, ignore/inherit, argv0, kill/timeout, concurrent children, unconsumed output and module identity')


if __name__ == '__main__':
    main()
