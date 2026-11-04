#!/usr/bin/env python3
"""Compare incremental StringDecoder adaptation with Node's builtin."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite',required=True)
    parser.add_argument('--node',default=shutil.which('node'))
    args=parser.parse_args()
    if not args.node: parser.error('Node is required for differential tests')
    cases=[]
    def add(label,expression): cases.append((label,expression))
    def js(value): return json.dumps(value,ensure_ascii=True)
    labels=['utf8','UTF-8','utf16le','ucs2','latin1','binary','ascii','hex','base64','base64url']
    values=[[],[0,127,128,255],[0xc3,0xa9],[0xe2,0x82,0xac],[0xf0,0x9f,0x98,0x80],
            [0xed,0xa0,0x80],[0xe2,65,0x82],[0xf0,0x9f],[255,254,65,0],[0,0xd8],[0,0xd8,0,0xdc]]
    for label in labels:
        for data in values:
            for cut in range(len(data)+1):
                add(f'{label} {data} cut={cut}',f'(() => {{const d=new StringDecoder({js(label)});const b=Buffer.from({js(data)});return [d.encoding,d.write(b.subarray(0,{cut})),d.end(b.subarray({cut})),d.end()];}})()')
    for value in ['undefined','null','0','false','{}','Symbol("x")','"bad"','""']:
        add(f'constructor {value}',f'new StringDecoder({value}).encoding')
    for label in labels:
        add(f'string passthrough {label}',f'(() => {{const d=new StringDecoder({js(label)});d.write(Buffer.from([0xc3]));return [d.write("A+B/="),d.end("tail")];}})()')
    for source in ['null','undefined','{}','new ArrayBuffer(2)','new Uint8Array([65,66])','new Uint16Array([65,66])','new DataView(new Uint8Array([65,66]).buffer)','new Uint8Array(new SharedArrayBuffer(2))']:
        add(f'input {source}',f'new StringDecoder().write({source})')
    add('byte ranges', '''(() => {const a=new Uint8Array([65,66,67,68]),v=a.subarray(1,3);
        Object.defineProperties(v,{buffer:{value:new ArrayBuffer(1)},byteOffset:{value:0},byteLength:{value:1}});
        return new StringDecoder().write(v);})()''')
    add('detached input', '''(() => {const a=new ArrayBuffer(2),v=new Uint8Array(a);a.transfer();return new StringDecoder().end(v);})()''')
    add('identity','[builtin.StringDecoder===StringDecoder,new StringDecoder().constructor===StringDecoder]')
    source="import builtin,{StringDecoder} from 'node:string_decoder';\nfunction emit(label,fn){try{console.log(JSON.stringify([label,{value:fn()}]));}catch(e){console.log(JSON.stringify([label,{error:e.name,code:e.code}]));}}\n"+'\n'.join(f'emit({js(label)},()=>({expression}));' for label,expression in cases)
    with tempfile.TemporaryDirectory(prefix='mite-decoder-') as directory:
        fixture=Path(directory)/'decoder.mjs';fixture.write_text(source)
        results=[subprocess.run([exe,str(fixture)],capture_output=True,text=True,timeout=20) for exe in [args.node,args.mite]]
        for result in results:
            if result.returncode or result.stderr: print(result);return 1
        node,mite=[[json.loads(line) for line in result.stdout.splitlines()] for result in results]
        failures=0
        for expected,actual in zip(node,mite):
            if expected!=actual:
                failures+=1
                if failures<=20: print(f'FAIL Node={expected!r} Mite={actual!r}')
        if len(node)!=len(cases) or len(mite)!=len(cases): failures+=1;print('FAIL missing output rows')
        print(f'{len(cases)} StringDecoder comparisons, {failures} differences')
        return int(bool(failures))


if __name__=='__main__':
    raise SystemExit(main())
