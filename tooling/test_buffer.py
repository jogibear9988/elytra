#!/usr/bin/env python3
"""Differential checks for Buffer bytes, encodings and memory operations."""
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
    codecs = ['utf8', 'UTF-8', 'utf16le', 'ucs2', 'latin1', 'binary', 'ascii', 'hex', 'base64', 'base64url', 'bad']
    texts = ['', 'hello', 'é文😀', '\ud800x\udfff', '\ufeffa\x00z', '1affz3', 'a=', '\ud800\udc41\ud800\udc41', 'aaaa====', 'Y W\nJj-_', '0g12', 'f', 'YWJj']
    for codec in codecs:
        for text in texts:
            add(f'from {codec} {text!r}', f'Buffer.from({js(text)}, {js(codec)}).toJSON()')
            add(f'byteLength {codec} {text!r}', f'Buffer.byteLength({js(text)}, {js(codec)})')
    for codec in ['undefined', 'null', 'false', 'true', '1', '{}', '\'\'']:
        for method in ['toString', 'write', 'from', 'isEncoding', 'fill', 'byteLength']:
            expression = {
                'toString': f'Buffer.from("abc").toString({codec})',
                'write': f'Buffer.alloc(3).write("a",0,1,{codec})',
                'from': f'Buffer.from("abc",{codec}).toJSON()',
                'isEncoding': f'Buffer.isEncoding({codec})',
                'fill': f'Buffer.alloc(3).fill("a",0,3,{codec}).toJSON()',
                'byteLength': f'Buffer.byteLength("abc",{codec})',
            }[method]
            add(f'encoding argument {method} {codec}', expression)
    for offset in ['-1', '-0.1', 'NaN', 'Infinity', '4.1', '"a"', 'null']:
        add(f'ArrayBuffer offset {offset}', f'Buffer.from(new ArrayBuffer(4),{offset}).length')
        add(f'ArrayBuffer length {offset}', f'Buffer.from(new ArrayBuffer(4),0,{offset}).length')
    for first in range(256):
        for second in [0x41, 0x80, 0x9f, 0xbf, 0xff]:
            add(f'utf8 boundary {first} {second}', f'Buffer.from([{first},{second},128,128]).toString()')
    raw = [[], [0, 127, 128, 255], [0xe2, 0x82], [0xe2, 0x41, 0xa1], [0xe0, 0x80, 0x80],
           [0xed, 0xa0, 0x80], [0xf4, 0x90, 0x80, 0x80], [0xf0, 0x9f, 0x98, 0x80],
           [0xef, 0xbb, 0xbf, 97], [0xff, 0xfe, 65], [0xc0, 0xaf], [0xf0, 0x90, 0x80]]
    for data in raw:
        for codec in codecs:
            add(f'decode {data} {codec}', f'Buffer.from({js(data)}).toString({js(codec)})')
    for codec in ['utf8', 'utf16le', 'hex', 'base64', 'ascii']:
        for text in ['é文😀x', '\ud800z', 'aabbcc', 'Y WJj', 'a']:
            for length in range(9):
                add(f'write {codec} {text!r} {length}',
                    f'(() => {{ const b=Buffer.alloc(10, 0x2e); return [b.write({js(text)}, 1, {length}, {js(codec)}), b.toString("hex")]; }})()')
    add('array iteration is ignored', '(() => { const a=[1,2]; a[Symbol.iterator]=function*(){yield 9;}; return Buffer.from(a).toJSON(); })()')
    for value in ['undefined', 'null', '5', '{}', '[]', '[256,-1,1.5,NaN]', '{length:3,0:1,2:257}',
                  '{length:"3",0:1}', '{type:"Buffer",data:[1,255]}', 'new Uint16Array([256,257])',
                  'new DataView(new ArrayBuffer(3))', 'new String("abc")', '{valueOf(){return "abc"}}',
                  '{[Symbol.toPrimitive](){return "abc"}}', 'Object.create(null)']:
        add(f'from object {value}', f'Buffer.from({value}).toJSON()')
    for value in ['-1', '1.5', 'NaN', 'Infinity', '"2"', '0', '3']:
        add(f'alloc {value}', f'Buffer.alloc({value}).toJSON()')
    for value in ['"x"', '"ab"', '""', '0x101', 'null', 'false', 'Buffer.from([1,2])', 'Buffer.alloc(0)']:
        for start, end in [(0, 7), (2, 5), (6, 2), (9, 7), (-1, 2)]:
            add(f'fill {value} {start} {end}', f'Buffer.alloc(7).fill({value},{start},{end}).toJSON()')
    for start in [None, -10, -1, 0, 1, 3, 8]:
        for needle in ['"ab"', '""', '98', 'Buffer.from("ab")']:
            for method in ['indexOf', 'lastIndexOf', 'includes']:
                add(f'{method} {needle} {start}', f'Buffer.from("abcabc").{method}({needle},{js(start) if start is not None else "undefined"})')
    for codec in ['utf16le', 'utf8']:
        for start in [-3, 0, 1, 2, 3, 9, 20]:
            for method in ['indexOf', 'lastIndexOf']:
                add(f'wide {method} {start} {codec}', f'Buffer.from("abcabc","utf16le").{method}("bc",{start},{js(codec)})')
    for start, end, target in [(0, 4, 1), (1, 5, 0), (0, 10, 3), (9, 10, 0), (0, 5, 8), (-1, 3, 0)]:
        add(f'copy {start} {end} {target}', f'(() => {{ const b=Buffer.from([1,2,3,4,5]); return [b.copy(b,{target},{start},{end}),b.toJSON()]; }})()')
    for start, end in [(-5, 10), (1, 3), (5, 1), (-1, 0), (0, 0), (1.5, 4.5)]:
        for method in ['slice', 'subarray', 'toString']:
            arguments = f'"hex",{start},{end}' if method == 'toString' else f'{start},{end}'
            add(f'{method} {start} {end}', f'Buffer.from([1,2,3,4,5]).{method}({arguments})')
    for length in [None, 0, 3, 10]:
        add(f'concat {length}', f'Buffer.concat([Buffer.from([1,2]),new Uint8Array([3,4,5])]{"," + str(length) if length is not None else ""}).toJSON()')
    for width in [16, 32, 64]:
        for length in [3, 8, 16]:
            add(f'swap{width} {length}', f'Buffer.from(Array.from({{length:{length}}},(_,i)=>i)).swap{width}().toJSON()')
    def numeric_case(method, value, offset='1', length=12, extra=''):
        read = method.replace('write', 'read', 1)
        add(f'numeric {method} {value} @{offset} len={length} {extra}',
            f'(() => {{ const b=Buffer.alloc({length},0x2e); const end=b.{method}({value},{offset}{extra}); '
            f'const v=b.{read}({offset}{extra}); return [end,b.toString("hex"),typeof v==="bigint"?v.toString():Object.is(v,-0)?"-0":Number.isNaN(v)?"NaN":String(v)]; }})()')
    methods = []
    for signed in [False, True]:
        kind = 'Int' if signed else 'UInt'
        for width in [1, 2, 4]:
            for order in ([''] if width == 1 else ['LE', 'BE']):
                method = f'write{kind}{width*8}{order}'
                methods.append(method)
                maximum = 2**(width*8-(1 if signed else 0))-1
                minimum = -maximum-1 if signed else 0
                for value in [str(minimum), str(maximum), str(minimum-1), str(maximum+1), '-1.5', '1.5', 'NaN', 'Infinity', '"3"', 'null', 'undefined', '1n']:
                    numeric_case(method, value)
        for order in ['LE', 'BE']:
            for width in range(1,7):
                maximum = 2**(width*8-(1 if signed else 0))-1
                minimum = -maximum-1 if signed else 0
                for value in [minimum, maximum, minimum-1, maximum+1, -1.5, 1.5]:
                    numeric_case(f'write{kind}{order}', str(value), extra=f',{width}')
            for width in ['0', '7', '1.5', 'NaN', 'undefined', '"1"']:
                numeric_case(f'write{kind}{order}', '1', extra=f',{width}')
    for kind in ['Float', 'Double']:
        for order in ['LE', 'BE']:
            method = f'write{kind}{order}'
            methods.append(method)
            for value in ['0', '-0', '1.5', '-3.25', '1e-40', '1e40', 'Infinity', '-Infinity', 'NaN', '"3"', '1n']:
                numeric_case(method, value)
    for signed in [False, True]:
        for order in ['LE', 'BE']:
            method = f'writeBig{"Int" if signed else "UInt"}64{order}'
            methods.append(method)
            limit = 2**(63 if signed else 64)
            for value in ['0n', '1n', '-1n', f'{limit-1}n', f'{limit}n', f'{-limit}n', f'{-limit-1}n', '1', '"1"', '"-1"', 'undefined']:
                numeric_case(method, value)
    for method in methods:
        for length in [0, 1, 8]:
            for offset in ['-1', '0', '1', '9', '1.5', 'NaN', '"1"', 'null', 'undefined']:
                value = '1n' if 'Big' in method else '1'
                numeric_case(method, value, offset, length)
                read = method.replace('write', 'read', 1)
                add(f'numeric bounds {read} {offset} {length}', f'String(Buffer.alloc({length}).{read}({offset}))')
    for method in ['readUIntLE', 'readIntBE', 'writeUIntLE', 'writeIntBE']:
        for offset in ['undefined', 'null', '1.5', '-1']:
            add(f'variable offset {method} {offset}', f'Buffer.alloc(8).{method}({"1," if method.startswith("write") else ""}{offset},3)')
    add('numeric aliases and subview', '(() => {const b=Buffer.alloc(16);const v=b.subarray(3,11);v.writeDoubleBE(1.5);return [v.readDoubleBE(),b.toString("hex"),Buffer.prototype.readUint32LE===Buffer.prototype.readUInt32LE,Buffer.prototype.writeBigUint64BE===Buffer.prototype.writeBigUInt64BE,Buffer.prototype.readUInt16BE.call(new Uint8Array([1,2]))];})()')
    source = 'import builtin, { Buffer as Imported } from "node:buffer";\n' + '''
function emit(label, fn) {
    try { console.log(JSON.stringify([label, {value:fn()}])); }
    catch(e) { console.log(JSON.stringify([label,{error:e.name,code:e.code}])); }
}
''' + '\n'.join(f'emit({js(label)},()=>({expression}));' for label, expression in cases) + '''
emit('identity and memory', () => {
    const b=Buffer.from([1,2,3,4]); const s=b.slice(1,3); s[0]=9;
    const memory=new ArrayBuffer(6); const shared=Buffer.from(memory,1,3); shared[1]=7;
    const copied=Buffer.from(shared); shared[1]=8;
    const words=new Uint16Array([0x1234,0xabcd]); const raw=Buffer.copyBytesFrom(words,1,1); words[1]=0;
    return [Imported===Buffer,builtin.Buffer===Buffer,Buffer.isBuffer(b),Buffer.isBuffer(b.subarray()),
        Buffer.isBuffer(b.map(x=>x)),b instanceof Uint8Array,b.constructor===Buffer,b.parent===b.buffer,b.offset===b.byteOffset,b.toJSON(),
        Array.from(new Uint8Array(memory)),copied.toJSON(),raw.toString('hex'),
        b.equals(Buffer.from(b)),Buffer.compare(b,Buffer.from([1,9,4])),
        JSON.parse(JSON.stringify(b)), Buffer.byteLength(new Uint16Array(3))];
});
'''
    with tempfile.TemporaryDirectory(prefix='mite-buffer-') as directory:
        path = Path(directory) / 'buffer.mjs'
        path.write_text(source)
        results = [subprocess.run([exe, str(path)], capture_output=True, text=True, timeout=30) for exe in [args.node, args.mite]]
        for result in results:
            if result.returncode or result.stderr:
                print(result)
                return 1
        node, mite = [[json.loads(line) for line in result.stdout.splitlines()] for result in results]
        failures = 0
        for expected, actual in zip(node, mite):
            if expected != actual:
                failures += 1
                if failures <= 30:
                    print(f'FAIL Node={expected!r} Mite={actual!r}')
        if len(node) != len(mite) or len(node) != len(cases) + 1:
            print('FAIL missing output rows')
            failures += 1
        policy = Path(directory) / 'policy.mjs'
        policy.write_text("""
import {Buffer as Original} from 'node:buffer';
console.log(Buffer.allocUnsafe(16).every(x=>x===0), Buffer.allocUnsafeSlow(16).every(x=>x===0));
try { Buffer.alloc(2**31); } catch(e) { console.log(e.name, e.code); }
globalThis.Buffer = function Replacement() {};
console.log((await import('node:buffer')).Buffer === Original);
""")
        result = subprocess.run([args.mite, str(policy)], capture_output=True, text=True, timeout=15)
        passed = result.returncode == 0 and result.stdout == 'true true\nRangeError ERR_OUT_OF_RANGE\ntrue\n' and not result.stderr
        if not passed:
            print(f'FAIL explicit Buffer allocation policy: {result}')
            failures += 1
        print(f'{len(cases)+1} Buffer comparisons plus allocation policy, {failures} differences')
        return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
