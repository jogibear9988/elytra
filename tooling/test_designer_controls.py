#!/usr/bin/env python3
"""Exercise the sample-local window capability with an actual Mite HTTP server."""
import argparse
import json
from pathlib import Path
import selectors
import subprocess
import tempfile
import urllib.request
import urllib.error

ROOT=Path(__file__).resolve().parents[1]

def check_controls(mite):
    with tempfile.TemporaryDirectory(prefix='designer-controls-') as directory:
        root=Path(directory);(root/'index.html').write_text('<html><head></head><body>public</body></html>')
        script=root/'main.mjs'
        script.write_text('import {startServer} from '+json.dumps((ROOT/'examples/web-component-designer/server.mjs').as_uri())+';\n'+
            'const s=await startServer('+json.dumps(str(root))+',undefined,{token:"test-capability",execute:action=>({action})});console.log(`http://127.0.0.1:${s.address().port}`);')
        process=subprocess.Popen([str(Path(mite).resolve()),str(script)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout,selectors.EVENT_READ);assert selector.select(10),'Control server readiness'
            origin=process.stdout.readline().strip();assert origin.startswith('http://127.0.0.1:'),origin
            host=origin.removeprefix('http://')
            def request(action='state',method='POST',changes=None,data=None):
                headers={'Origin':origin,'Host':host,'X-Firefly-Window-Token':'test-capability'};headers.update(changes or {})
                try:
                    with urllib.request.urlopen(urllib.request.Request(origin+'/__firefly_window/'+action,data=data,headers=headers,method=method),timeout=5) as response:return response.status,response.read()
                except urllib.error.HTTPError as error:return error.code,error.read()
            for action in ['state','minimize','maximize','unmaximize','restore','close']:
                status,body=request(action);assert status==200 and json.loads(body)=={'action':action},(status,body)
            for changes in [{'Origin':'http://evil.invalid'},{'Host':'evil.invalid'},{'X-Firefly-Window-Token':'wrong'},{'Origin':''}]:assert request(changes=changes)[0]==403,changes
            assert request(method='GET')[0]==403
            assert request(method='OPTIONS')[0]==403
            assert request(data=b'arbitrary payload')[0]==403
            assert request(action='executeJavaScript')[0]==400
            print('PASS window capability: fixed actions; token/origin/host/method/body checks; no arbitrary channel')
        finally:
            process.terminate();process.communicate(timeout=10)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--mite',required=True,type=Path);args=parser.parse_args();check_controls(args.mite)
