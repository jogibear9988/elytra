#!/usr/bin/env python3
"""Differential text encoding, streaming decoder and BufferSource checks."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite', required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    args = parser.parse_args()
    if not args.node:
        parser.error('Node is required for differential tests')
    cases = []
    def add(label, expression):
        cases.append((label, expression))
    def js(value):
        return json.dumps(value, ensure_ascii=True)
    labels = ['utf-8',' UTF8 ','utf-16','utf-16le','utf-16be','ascii','latin1','iso-8859-1','us-ascii',
              'windows-1250','windows-1251','windows-1252','windows-1253','windows-1254','windows-1255',
              'windows-1256','windows-1257','windows-1258','windows-874','ibm866','koi8-r','koi8-u',
              'iso-8859-2','iso-8859-3','iso-8859-4','iso-8859-5','iso-8859-6','iso-8859-7','iso-8859-8',
              'iso-8859-8-i','iso-8859-10','iso-8859-13','iso-8859-14','iso-8859-15','iso-8859-16',
              'macintosh','x-mac-cyrillic','big5','euc-jp','iso-2022-jp','shift_jis','euc-kr','gb18030','gbk',
              'x-user-defined','replacement','iso-2022-cn','hz-gb-2312','UTF-32','bad','','é']
    for label in labels:
        for opts in ['{}','{fatal:true}','{ignoreBOM:true}','{fatal:1,ignoreBOM:1}']:
            add(f'label {label!r} {opts}', f'(() => {{const d=new TextDecoder({js(label)},{opts});return [d.encoding,d.fatal,d.ignoreBOM,Object.prototype.toString.call(d)];}})()')
    raw = [[],[0,65,127,128,255],[239,187,191,97],[255,254,65,0],[254,255,0,65],
           [0xe2,0x82],[0xe2,65,0xa1],[0xf0,0x9f,0x98,0x80],[0xed,0xa0,0x80],[0xf4,0x90,0x80,0x80]]
    for label in labels[:45]:
        for data in raw:
            for fatal in [False, True]:
                add(f'decode {label} {data} fatal={fatal}', f'new TextDecoder({js(label)},{{fatal:{js(fatal)}}}).decode(new Uint8Array({js(data)}))')
    streams = [('utf8',[239,187,191,0xe2,0x82,0xac,0xf0,0x9f,0x98,0x80]),
               ('utf8',[0xe2,0x82]),('utf8',[0xed,0xa0,0x80,65]),
               ('utf-16le',[255,254,65,0,0x3d,0xd8,0,0xde]),('utf-16le',[65,0,0x3d,0xd8]),
               ('utf-16be',[254,255,0,65,0xd8,0x3d,0xde,0]),
               ('shift_jis',[0x93,0xfa,0x96,0x7b,0x8c,0xea]),('big5',[0xa4,0x40,0xa4,0x41]),
               ('euc-kr',[0xb0,0xa1,0xb0,0xa2]),('gb18030',[0x81,0x30,0x81,0x30,0x80]),
               ('iso-2022-jp',[27,36,66,0x24,0x22,27,40,66,65]),('iso-2022-jp',[27,36,66,0xff,0x24,0x22])]
    for label, data in streams:
        for opts in ['{}','{fatal:true}','{ignoreBOM:true}','{fatal:true,ignoreBOM:true}']:
            for cut in range(len(data)+1):
                add(f'stream {label} {data} cut={cut} {opts}', f'''(() => {{
                    const d=new TextDecoder({js(label)},{opts});const data=new Uint8Array({js(data)});
                    return [outcome(()=>d.decode(data.subarray(0,{cut}),{{stream:true}})),
                        outcome(()=>d.decode(data.subarray({cut}))),outcome(()=>d.decode(new Uint8Array([65,0])))];
                }})()''')
        add(f'byte stream {label} {data}', f'''(() => {{const d=new TextDecoder({js(label)});
            return [...{js(data)}.map(x=>d.decode(new Uint8Array([x]),{{stream:true}})),d.decode()];}})()''')
    for label in ['big5','euc-jp','iso-2022-jp','shift_jis','euc-kr']:
        add(f'large output {label}', f'(() => {{const d=new TextDecoder({js(label)});const s=d.decode(new Uint8Array(8193).fill(65));return [s.length,s==="A".repeat(8193)];}})()')
        for data in [[255],[226,130],[255,65]]:
            for stream in [False,True]:
                add(f'fatal recovery {label} {data} {stream}', f"""(() => {{const d=new TextDecoder({js(label)},{{fatal:true}});
                    return [outcome(()=>d.decode(new Uint8Array({js(data)}),{{stream:{js(stream)}}})),
                        outcome(()=>d.decode(new Uint8Array([65]),{{stream:true}})),outcome(()=>d.decode())];}})()""")
    for label in ['utf8','bad']:
        for opts in ['1','false']:
            add(f'validation order {label} {opts}', f'new TextDecoder({js(label)},{opts}).encoding')
    for text in ['', 'hello', 'é文😀z', '\ud800a\udfff', '\ufeff\x00z', '\ud83d\ude00', '\ud800\ud800\udc00']:
        add(f'encode {text!r}', f'Array.from(new TextEncoder().encode({js(text)}))')
        for length in range(13):
            add(f'encodeInto {text!r} length={length}', f'''(() => {{
                const destination=new Uint8Array({length}+4);destination.fill(0x2e);
                const result=new TextEncoder().encodeInto({js(text)},destination.subarray(2,2+{length}));
                return [result,Array.from(destination)];}})()''')
    for value in ['undefined','null','1','true','1n','Symbol("x")','new String("é")','{}']:
        add(f'encode conversion {value}', f'Array.from(new TextEncoder().encode({value}))')
        add(f'encodeInto conversion {value}', f'new TextEncoder().encodeInto({value},new Uint8Array(8))')
        add(f'decode conversion {value}', f'new TextDecoder().decode({value})')
        add(f'label conversion {value}', f'new TextDecoder({value}).encoding')
    for opts in ['null','1','false','"x"','[]','{}','{stream:true}']:
        add(f'constructor options {opts}', f'new TextDecoder("utf8",{opts}).fatal')
        add(f'decode options {opts}', f'new TextDecoder().decode(undefined,{opts})')
    for destination in ['undefined','null','[]','new Uint8ClampedArray(4)','new Int8Array(4)','Buffer.alloc(4)',
                        'new Uint8Array(new SharedArrayBuffer(4))']:
        add(f'destination {destination}', f'new TextEncoder().encodeInto("é",{destination})')
    for source in ['new Uint8Array([65,0,66,0])','new Uint16Array([65,66])','new DataView(new Uint8Array([65,0,66,0]).buffer,1,2)',
                   'new ArrayBuffer(3)','new SharedArrayBuffer(3)','Buffer.from([65,66])']:
        add(f'buffer source {source}', f'new TextDecoder().decode({source})')
    for constructor, methods in [('TextEncoder',['encoding','encode','encodeInto']),('TextDecoder',['encoding','fatal','ignoreBOM','decode'])]:
        for method in methods:
            add(f'brand {constructor}.{method}', f'''(() => {{const d=Object.getOwnPropertyDescriptor({constructor}.prototype,{js(method)});
                return d.get?d.get.call({{}}):d.value.call({{}});}})()''')
    add('detached buffers', '''(() => {
        const b=new ArrayBuffer(4),v=new Uint8Array(b),d=new DataView(b);b.transfer();
        return [new TextEncoder().encodeInto('x',v),new TextDecoder().decode(b),new TextDecoder().decode(v),new TextDecoder().decode(d)];})()''')
    add('resizable out of bounds', '''(() => {
        const b=new ArrayBuffer(8,{maxByteLength:16});const v=new Uint8Array(b,4,4);b.resize(2);
        return [new TextEncoder().encodeInto('x',v),new TextDecoder().decode(v)];})()''')
    add('internal byte ranges', '''(() => {
        const a=new Uint8Array([65,66,67,68]);const v=a.subarray(1,3);
        Object.defineProperties(v,{buffer:{value:new ArrayBuffer(1)},byteOffset:{value:0},byteLength:{value:1}});
        const before=new TextDecoder().decode(v);const result=new TextEncoder().encodeInto('é',v);
        return [before,result,Array.from(a)];})()''')
    add('label coercion order', '(() => {const events=[];const result=outcome(()=>new TextDecoder({toString(){events.push("label");return "bad"}},1));return [events,result];})()')
    add('unsupported label getter order', '(() => {const events=[];const result=outcome(()=>new TextDecoder("bad",{get fatal(){events.push("fatal");return false;}}));return [events,result];})()')
    add('builtin identity', '''[TextEncoder===Encoder,TextDecoder===Decoder,util.TextEncoder===TextEncoder,
        util.TextDecoder===TextDecoder,Object.prototype.toString.call(new TextEncoder()),
        new TextEncoder().encoding,TextEncoder.length,TextDecoder.length,
        new TextEncoder().encode.length,new TextEncoder().encodeInto.length,new TextDecoder().decode.length]''')
    source = '''import util,{TextEncoder as Encoder,TextDecoder as Decoder} from 'node:util';
function outcome(fn){try{return {value:fn()};}catch(e){return {error:e.name,code:e.code};}}
function emit(label,fn){console.log(JSON.stringify([label,outcome(fn)]));}
''' + '\n'.join(f'emit({js(label)},()=>({expression}));' for label, expression in cases)
    with tempfile.TemporaryDirectory(prefix='mite-encoding-') as directory:
        path = Path(directory) / 'encoding.mjs'
        path.write_text(source)
        results = [subprocess.run([exe,str(path)],capture_output=True,text=True,timeout=30) for exe in [args.node,args.mite]]
        for result in results:
            if result.returncode or result.stderr:
                print(result)
                return 1
        node,mite = [[json.loads(line) for line in result.stdout.splitlines()] for result in results]
        failures = 0
        for expected,actual in zip(node,mite):
            if expected != actual:
                failures += 1
                if failures <= 30:
                    print(f'FAIL Node={expected!r} Mite={actual!r}')
        if len(node)!=len(cases) or len(mite)!=len(cases):
            failures += 1
            print('FAIL missing result rows')
        print(f'{len(cases)} encoding comparisons, {failures} differences')
        return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
