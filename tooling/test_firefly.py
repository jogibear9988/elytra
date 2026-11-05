#!/usr/bin/env python3
"""Run real Firefly windows and an Electron migration reference; missing binaries fail."""
import argparse
import json
import os
from pathlib import Path
import re
import selectors
import signal
import subprocess
import tempfile
import time
from test_renderer import ROOT, environment, stop


def execute(binary, script, module='firefly'):
    with tempfile.TemporaryDirectory(prefix='firefly-app-env-') as directory:
        env=environment(directory)
        env['FIREFLY_API_MODULE']=module
        env.pop('ELECTRON_RUN_AS_NODE',None)
        command=[str(binary)]
        if module=='electron':command.append(f'--user-data-dir={directory}')
        command.append(str(script))
        process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env,start_new_session=True)
        try:
            stdout,stderr=process.communicate(timeout=40)
            result=subprocess.CompletedProcess(command,process.returncode,stdout,stderr)
            (ROOT/f'build/{module}-last-app-test.log').write_text(str(result))
            return result
        except subprocess.TimeoutExpired as error:
            (ROOT/f'build/{module}-last-app-test.log').write_text(str((error.stdout,error.stderr)))
            raise
        finally:
            stop(process);process.communicate()


def success(result):
    assert result.returncode==0,result
    rows=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
    assert len(rows)==1,result
    return rows[0]


def crash_probe(binary, script):
    with tempfile.TemporaryDirectory(prefix='firefly-crash-env-') as directory:
        process=subprocess.Popen([str(binary),str(script)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                                 text=True,env=environment(directory),start_new_session=True)
        try:
            renderer=None;deadline=time.monotonic()+15
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout,selectors.EVENT_READ)
                while process.poll() is None and time.monotonic()<deadline:
                    if not selector.select(0.1):continue
                    match=re.fullmatch(r'CRASH_READY (\d+)\n',process.stdout.readline())
                    if match:renderer=int(match[1]);break
            assert renderer is not None,'No renderer ready for crash test'
            rows=[row.strip().split(maxsplit=2) for row in subprocess.check_output(['ps','-axo','pid=,ppid=,comm='],text=True).splitlines()]
            assert any(int(pid)==renderer and int(parent)==process.pid and Path(command).name=='WebContent' for pid,parent,command in rows),'Renderer must be an owned OS child'
            os.kill(renderer,signal.SIGKILL)
            stdout,stderr=process.communicate(timeout=15)
            result=subprocess.CompletedProcess([],process.returncode,stdout,stderr)
            assert success(result)=={'crash':True},result
        finally:
            stop(process);process.communicate()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firefly',required=True,type=Path)
    parser.add_argument('--electron',required=True,type=Path)
    args=parser.parse_args()
    firefly=args.firefly.resolve();electron=args.electron.resolve()
    if not firefly.is_file() or not electron.is_file():parser.error('Build Firefly and install the pinned Electron reference first')
    fixture=ROOT/'firefly/tests/migration/lifecycle.cjs'
    expected={'trace':['ready','did-finish-load','load-resolved','close-cancelled','close','closed','window-all-closed','before-quit','will-quit','quit:0']}
    actual=success(execute(firefly,fixture))
    reference=success(execute(electron,fixture,'electron'))
    assert actual==reference==expected,(actual,reference)
    print('PASS shared Electron/Firefly lifecycle, native controls, load promise and renderer isolation')
    result=success(execute(firefly,ROOT/'firefly/tests/migration/main-esm.mjs'))
    assert result=={'esm':True,'http':True,'rendererIsolated':True},result
    print('PASS static ESM firefly import, top-level await, Mite HTTP server and native loadURL')
    with tempfile.TemporaryDirectory(prefix='firefly-app-fixtures-') as directory:
        root=Path(directory)
        script=root/'main.cjs'
        script.write_text("""
const {app,BrowserWindow}=require('firefly');
app.whenReady().then(async()=>{
  for(const options of [{webPreferences:{preload:'x.js'}},{webPreferences:{nodeIntegration:true}},{webPreferences:{contextIsolation:false}},{width:0},{transparent:true}]) {
    let denied=false;try{new BrowserWindow(options);}catch(e){denied=!!e.code;}
    if(!denied)throw Error('unsupported or invalid option accepted');
  }
  const win=new BrowserWindow();let rejected=false;
  try{await win.loadFile('missing.html');}catch{rejected=true;}
  if(!rejected)throw Error('missing file did not reject');
  console.log(JSON.stringify({rejected:true}));win.on('closed',()=>app.quit());win.destroy();
});
""")
        assert success(execute(firefly,script))=={'rejected':True}
        script.write_text("""
const {app,BrowserWindow}=require('firefly');const {createServer}=require('node:http');
app.whenReady().then(async()=>{
 const server=createServer();await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const port=server.address().port;await new Promise(resolve=>server.close(resolve));
 const win=new BrowserWindow();let failure;
 try{await win.loadURL(`http://127.0.0.1:${port}/`);}catch(e){failure=e;}
 if(!failure||failure.code==='ERR_TIMED_OUT')throw Error('network failure must reject promptly');
 console.log(JSON.stringify({networkFailed:true}));win.on('closed',()=>app.quit());win.destroy();
});
""")
        started=time.monotonic()
        assert success(execute(firefly,script))=={'networkFailed':True}
        assert time.monotonic()-started<15,'Network failure used a deadline instead of its error callback'
        page=str(ROOT/'firefly/tests/migration/page.html')
        script.write_text("const {app,BrowserWindow}=require('firefly');app.whenReady().then(async()=>{const win=new BrowserWindow();win.on('closed',()=>app.quit());win.webContents.on('render-process-gone',(_e,details)=>{if(details.reason!=='crashed')throw Error('crash reason');console.log(JSON.stringify({crash:true}));win.destroy();});await win.loadFile("+json.dumps(page)+");console.log('CRASH_READY '+win.webContents.getOSProcessId());});")
        crash_probe(firefly,script)
        script.write_text("const {app,BrowserWindow}=require('firefly');app.whenReady().then(()=>{new BrowserWindow();setTimeout(()=>{throw Error('fatal GUI timer')},20)});")
        fatal=execute(firefly,script)
        assert fatal.returncode==1 and 'fatal GUI timer' in fatal.stderr,fatal
        if hasattr(os,'mkfifo'):
            fifo=root/'blocked';os.mkfifo(fifo)
            script.write_text("const {app,BrowserWindow}=require('firefly');app.whenReady().then(()=>{new BrowserWindow();require('node:fs').readFile("+json.dumps(str(fifo))+",()=>{});setTimeout(()=>{console.log(JSON.stringify({quit:true}));app.quit();},30)});")
            assert success(execute(firefly,script))=={'quit':True}
    print('PASS explicit unsupported/security options, file/network errors, actual renderer crash, fatal GUI callback and blocked-work quit')
    return 0


if __name__=='__main__':raise SystemExit(main())
