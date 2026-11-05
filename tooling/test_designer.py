#!/usr/bin/env python3
"""Check the built Designer server and actual native renderer editing workflow."""
import argparse
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import urllib.error
import urllib.request

ROOT=Path(__file__).resolve().parents[1]

def stop(process):
    if os.name!='nt':
        try:os.killpg(process.pid,signal.SIGKILL)
        except ProcessLookupError:pass
    elif process.poll() is None:process.kill()
    process.communicate()

def request(url,method='GET'):
    try:
        with urllib.request.urlopen(urllib.request.Request(url,method=method),timeout=10) as response:return response.status,response.headers,response.read()
    except urllib.error.HTTPError as error:return error.code,error.headers,error.read()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite',required=True,type=Path)
    parser.add_argument('--app',required=True,type=Path)
    parser.add_argument('--firefly',type=Path)
    parser.add_argument('--launcher',type=Path)
    args=parser.parse_args();app=args.app.resolve()
    pin=json.loads((ROOT/'examples/web-component-designer/upstream.json').read_text())
    assert json.loads((app/'BUILD.json').read_text())['upstream']==pin,'Wrong demo source pin'
    process=subprocess.Popen([str(args.mite.resolve()),str(app/'main.mjs'),'--serve-only'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,start_new_session=os.name!='nt')
    try:
        if os.name!='nt':assert select.select([process.stdout],[],[],15)[0],'Server did not become ready'
        url=process.stdout.readline().strip();assert url.startswith('http://127.0.0.1:'),url
        code,headers,body=request(url);assert code==200 and b'node-projects-app-shell' in body and headers['Content-Type'].startswith('text/html'),(code,headers)
        code,headers,body=request(url,'HEAD');assert code==200 and not body and int(headers['Content-Length'])>0,(code,headers,body)
        for file,content_type in [('dist/appShell.js','text/javascript'),('assets/theme.js','text/javascript'),('node_modules/monaco-editor/esm/vs/editor/editor.worker.js','text/javascript'),('manifest.json','application/json')]:
            code,headers,body=request(url+file);assert code==200 and headers['Content-Type'].startswith(content_type) and body,(file,code,headers)
        for path,status in [('.git/config',400),('%00',400),('%5c..%5cmain.mjs',400),('%zz',400),('missing-file',404),('main.mjs',404),('__firefly_smoke.js',404)]:
            code,_,_=request(url+path);assert code==status,(path,code,status)
        code,_,_=request(url,'POST');assert code==405,code
        print('PASS pinned Designer assets, JS/worker MIME, HEAD, missing files, path rejection and read-only production server')
    finally:stop(process)
    if args.firefly or args.launcher:
        command=[str(args.launcher.resolve()),'--smoke'] if args.launcher else [str(args.firefly.resolve()),str(app/'main.mjs'),'--smoke']
        process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,start_new_session=os.name!='nt')
        try:
            stdout,stderr=process.communicate(timeout=65)
            assert process.returncode==0 and 'DESIGNER_DOCUMENT_LOADED' in stdout and 'DESIGNER_UI_EDIT_MONACO_ISOLATION_OK' in stdout,(process.returncode,stdout,stderr)
            print('PASS actual Firefly/Ladybird app boot, document edit/round-trip, Monaco and absent Node privileges')
        finally:stop(process)
    else:parser.error('Native acceptance requires --firefly or --launcher; a server-only run is insufficient')

if __name__=='__main__':main()
