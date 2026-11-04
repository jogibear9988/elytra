#!/usr/bin/env python3
"""Compare Node URL/path, IDNA and HTTP-option helpers without network access."""
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
    paths = ['', '.', '..', 'a/b', 'a/../b/', '/', '//', '/foo\\/', '/a b/é文😀', '/\ud800',
             '/a[b]c^`{}<>"\'()*!~:@&+$,;=', 'a?b#c%', '/a\tb\nc\rd\x00', 'C:\\a\\b',
             'C:foo', 'C:', 'C:/', '\\\\server\\share\\a', '//server/share', '\\\\server',
             '\\\\bücher\\share\\x', '\\\\?\\C:\\a', '\\\\?\\UNC\\server\\share\\a']
    paths += [chr(92)*2+'server'+chr(92), 'C:'+chr(92), '//server/share/',
              chr(92)*2+'?'+chr(92)+'C:'+chr(92), chr(92)*2+'?'+chr(92)+'UNC'+chr(92)+'server'+chr(92)+'share']
    for path in paths:
        for options in ['undefined','null','1','{windows:false}','{windows:true}','{windows:null}','{windows:"yes"}']:
            add(f'pathToFileURL {path!r} {options}', f'u.pathToFileURL({js(path)},{options}).href')
    files = ['file:///tmp/a', 'file://localhost/a', 'file://server/share/a', 'file://xn--bcher-kva/share/a',
             'file:///C:/a', 'file:///c:foo', 'file:///C:', 'file:///a%2fb', 'file:///a%5Cb',
             'file:///a%FFb', 'file:///x%', 'file:///é%20文', 'file:///a%00b', 'https://example.org/a', 'relative']
    for file in files:
        for windows in ['false','true']:
            for method in ['fileURLToPath','fileURLToPathBuffer']:
                expression = f'u.{method}({js(file)},{{windows:{windows}}})'
                if method.endswith('Buffer'):
                    expression += '.toString("hex")'
                add(f'{method} {file} {windows}', expression)
    for expression in ['null','undefined','42','{}','{href:"file:///a"}','new URL("file:///tmp/a")']:
        for method in ['fileURLToPath','fileURLToPathBuffer','pathToFileURL']:
            add(f'type {method} {expression}', f'String(u.{method}({expression}))')
    for domain in ['', 'bücher.de', 'faß.de', '例え.テスト', 'xn--bcher-kva.de', 'foo/bar', 'foo?x', 'foo#x',
                   'foo:80', 'abc@def', '@foo', 'https://x', '127.1', '[::1]', '[::1]:80', 'invalid%ZZ',
                   'foo ', ' foo', 'foo\tbar', 'foo\nbar', 'a..b', 'a_b.com', '\ud800', '%FF', '.']:
        for method in ['domainToASCII','domainToUnicode']:
            add(f'{method} {domain!r}', f'u.{method}({js(domain)})')
    for method in ['fileURLToPath','fileURLToPathBuffer','pathToFileURL','domainToASCII','domainToUnicode','urlToHttpOptions']:
        add(f'missing {method}', f'u.{method}()')
    for url in ['https://u:p@bücher.de:8443/a?x=é#fragment','http://[::1]:8080/a','http://x/',
                'http://a%20b:c%40d@x/path','http://%FF@x/','file:///a','data:hello']:
        add(f'http options {url}', f'u.urlToHttpOptions(new URL({js(url)}))')
    for expression in ['null','42','"http://x"','{}']:
        add(f'HTTP options type {expression}', f'u.urlToHttpOptions({expression})')
    add('own HTTP options', '(() => {const url=new URL("http://x");url.custom=42;const result=u.urlToHttpOptions(url);return [result,Object.getPrototypeOf(result)===null];})()')
    source = '''import u from 'node:url';
function emit(label,fn){try{console.log(JSON.stringify([label,{value:fn()}]));}catch(e){console.log(JSON.stringify([label,{error:e.name,code:e.code,input:e.input,base:e.base}]));}}
''' + '\n'.join(f'emit({js(label)},()=>({expression}));' for label, expression in cases)
    with tempfile.TemporaryDirectory(prefix='mite-url-paths-') as directory:
        fixture = Path(directory) / 'paths.mjs'
        fixture.write_text(source)
        results = [subprocess.run([exe,str(fixture)],capture_output=True,text=True,timeout=30) for exe in [args.node,args.mite]]
        for result in results:
            if result.returncode or result.stderr:
                print(result)
                return 1
        node, mite = [[json.loads(line) for line in result.stdout.splitlines()] for result in results]
        failures = 0
        for expected, actual in zip(node,mite):
            if expected != actual:
                failures += 1
                if failures <= 30:
                    print(f'FAIL Node={expected!r} Mite={actual!r}')
        if len(node) != len(cases) or len(mite) != len(cases):
            failures += 1
            print('FAIL missing result rows')
        print(f'{len(cases)} URL helper comparisons, {failures} differences')
        return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
