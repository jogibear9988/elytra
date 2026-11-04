#!/usr/bin/env python3
"""Compare native URL parsing and live URLSearchParams behavior with Node."""
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
    urls = ['', '.', '../文?a=1&b=two+words', '//Other.EXAMPLE:80/a', '#x', '?', 'https://example.com',
            'http://u:p@bücher.de:80/a/../b?q=é#x', 'http://127.1/', 'http://0x7f.1/',
            'http://[::1]:8080/a', 'http://[invalid]/', 'http://x:99999/', 'file:///tmp/é %',
            'file://localhost/C:/a', 'file://server/share/a', 'mailto:user@example.org?x=1',
            'data:text/plain,abc  ?q=1#x', 'blob:https://example.org/id', 'custom://host/path',
            'custom:opaque', '  https://exa\nmple.org/a\tb  ', 'http://a\\b', 'https://x/\ud800',
            'http://%23/', 'http://foo/bar?x=%FF', 'file:///a/%2f/b']
    for url in urls:
        for base in [None, 'https://example.org/a/b', 'file:///tmp/dir/', 'bad', 'mailto:a@b']:
            arguments = js(url) + (',' + js(base) if base is not None else '')
            add(f'parse {url!r} {base!r}', f'snapshot(new URL({arguments}))')
            add(f'canParse {url!r} {base!r}', f'URL.canParse({arguments})')
            add(f'parse static {url!r} {base!r}', f'URL.parse({arguments})?.href ?? null')
    for url in ['https://u:p@example.org:8080/a?q=1#f', 'file:///C:/a?x#y', 'data:opaque  ?q#f', 'custom://host:9/a']:
        for key in ['href','protocol','username','password','host','hostname','port','pathname','search','hash']:
            for value in ['', 'x', 'http:', 'https', '[::1]:80', 'bücher.de:443', '/é ?#', '?a=hello world', '#é', '65536', '80abc']:
                add(f'set {url} {key} {value!r}', f'(() => {{const u=new URL({js(url)});u[{js(key)}]={js(value)};return snapshot(u);}})()')
    for expression in ['undefined', 'null', 'true', '123', '1n', 'Symbol("x")', '{}', '{a:[1,2],b:null,c:undefined}',
                       '[["x","1"],["x","2"],["a"," "]]', 'new Map([["a",1],["b",2]])', '["ab"]',
                       '[["one"]]', '[[1,2,3]]', '{[Symbol.iterator]:123}', '{[Symbol("x")]:1}',
                       '{"\\ud800":"first","\\ufffd":"last"}', 'function named(){}']:
        add(f'params constructor {expression}', f'(() => {{const p=new URLSearchParams({expression});return [String(p),[...p],p.size];}})()')
    for encoded in ['', '?', '??a=1', 'a=1&&b&=x', 'a=1&a=2&x=3', 'x=%FF%EF%BB%BF', 'x=%E2%82',
                    'x=+%2B%20&%61=1', 'x=~!*\'()/%', 'é=文😀', 'x=%00%0A', 'x=%z0%0z%']:
        add(f'params parse {encoded!r}', f'(() => {{const p=new URLSearchParams({js(encoded)});return [String(p),[...p],p.size];}})()')
        for method, arguments in [('append',['a','new value']),('set',['a','last']),('delete',['a']),('delete',['a','1']),
                                  ('has',['a']),('has',['a','2']),('get',['a']),('getAll',['a']),('sort',[])]:
            add(f'params {encoded!r} {method} {arguments}',
                f'(() => {{const p=new URLSearchParams({js(encoded)});const v=p.{method}(...{js(arguments)});return [v,String(p),[...p]];}})()')
    for method in ['append','set','get','getAll','has','delete','forEach']:
        add(f'missing argument {method}', f'new URLSearchParams().{method}()')
        add(f'invalid receiver {method}', f'URLSearchParams.prototype.{method}.call({{}},"a","b")')
    for expression in ['new URL()', 'URL.canParse()', 'URL.parse()', 'new URL(Symbol("x"))',
                       'URL.prototype.toString.call({})', 'new URLSearchParams({a:Symbol("x")})']:
        add(expression, expression)
    add('live association', '''(() => {
        const u=new URL('https://x/a?b=two%20words&a=~#h');const p=u.searchParams;
        const values=[];p.sort();values.push(u.href);u.search='?x=1&x=2';values.push([...p]);
        p.delete('x','1');values.push(u.href);u.href='http://other/?new=+%FF';values.push(p===u.searchParams,[...p]);
        p.delete('new');values.push(u.href);u.search='?';p.sort();values.push(u.href);
        return values;
    })()''')
    add('live iteration', '''(() => {
        const p=new URLSearchParams('a=1&b=2');const iterator=p.entries();const out=[iterator.next()];
        p.append('c','3');p.delete('a');out.push(iterator.next(),iterator.next());
        const seen=[];p.forEach(function(v,k,self){seen.push([k,v,self===p,this.tag]);if(k==='b')p.append('d','4');},{tag:7});
        return [out,seen,[...p.keys()],[...p.values()],p[Symbol.iterator]===p.entries];
    })()''')
    for key in ['href','origin','searchParams']:
        add(f'invalid URL {key}', f'Object.getOwnPropertyDescriptor(URL.prototype,{js(key)}).get.call({{}})')
    add('invalid URL setter', 'Object.getOwnPropertyDescriptor(URL.prototype,"href").set.call({},"http://x")')
    add('invalid URL JSON', 'URL.prototype.toJSON.call({})')
    add('exhausted iterator', '(() => {const p=new URLSearchParams();const i=p.entries();const a=i.next();p.append("a","b");return [a,i.next()];})()')
    add('iterator shape', '(() => {const i=new URLSearchParams().entries();return [Object.prototype.toString.call(i),Object.keys(Object.getPrototypeOf(i)),i[Symbol.iterator]()===i,i.next()];})()')
    add('invalid iterator receiver', 'Object.getPrototypeOf(new URLSearchParams().entries()).next.call({})')
    add('global builtin identity', '''[URL===ImportedURL,URLSearchParams===ImportedParams,
        builtin.URL===URL,Object.prototype.toString.call(new URL('http://x')),
        Object.prototype.toString.call(new URLSearchParams()),new URL('http://x').toJSON()]''')
    source = '''import builtin,{URL as ImportedURL,URLSearchParams as ImportedParams} from 'node:url';
function snapshot(u){return [u.href,u.protocol,u.username,u.password,u.host,u.hostname,u.port,u.pathname,u.search,u.hash,u.origin,[...u.searchParams]];}
function emit(label,fn){try{console.log(JSON.stringify([label,{value:fn()}]));}catch(e){console.log(JSON.stringify([label,{error:e.name,code:e.code,input:e.input,base:e.base}]));}}
''' + '\n'.join(f'emit({js(label)},()=>({expression}));' for label, expression in cases)
    with tempfile.TemporaryDirectory(prefix='mite-url-') as directory:
        fixture = Path(directory) / 'url.mjs'
        fixture.write_text(source)
        results = [subprocess.run([exe,str(fixture)],capture_output=True,text=True,timeout=30) for exe in [args.node,args.mite]]
        for result in results:
            if result.returncode or result.stderr:
                print(result)
                return 1
        expected, actual = [[json.loads(line) for line in result.stdout.splitlines()] for result in results]
        differences = 0
        for node, mite in zip(expected,actual):
            if node != mite:
                differences += 1
                if differences <= 25:
                    print(f'FAIL Node={node!r} Mite={mite!r}')
        if len(expected) != len(cases) or len(actual) != len(cases):
            print('FAIL missing result rows')
            differences += 1
        print(f'{len(cases)} URL comparisons, {differences} differences')
        return int(bool(differences))


if __name__ == '__main__':
    raise SystemExit(main())
