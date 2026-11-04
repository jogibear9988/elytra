#!/usr/bin/env python3
"""Compare DNS argument validation and numeric fast paths with Node."""
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
    if not args.node:parser.error('Node is required')
    cases=[]
    for family in ['undefined','null','0','4','6','"IPv4"','"IPv6"','"4"','1','-1','true','{}','NaN','Infinity','-Infinity','1024.5','4294967296']:
        cases.append(f'dns.promises.lookup("127.0.0.1",{{family:{family}}})')
    for key in ['hints','all','verbatim','order']:
        for value in ['undefined','null','0','1','-1','1.5','true','false','"ipv4first"','"ipv6first"','"verbatim"','"bad"','{}','NaN','Infinity','-Infinity','1024.5','4294967296']:
            cases.append(f'dns.promises.lookup("::1",{{{key}:{value}}})')
    for value in ['undefined','null','0','4','6','1','true','"4"','[]','{}']:
        cases.append(f'dns.promises.lookup("127.0.0.1",{value})')
    for hostname in ['true','1','{}','[]','Symbol("x")']:
        cases.append(f'dns.promises.lookup({hostname})')
    for cb in ['undefined','null','0','{}','"x"']:
        cases.append(f'dns.lookup("localhost",{{}},{cb})')
    for value in ['undefined','null','0','"verbatim"','"ipv4first"','"ipv6first"','"bad"']:
        cases.append(f'dns.setDefaultResultOrder({value})')
    cases += ['new Promise((resolve,reject)=>dns.lookup(\"127.0.0.1\",{family:\"IPv4\"},(e,address,family)=>e?reject(e):resolve({address,family})))','dns.promises.lookup("::1%en0",{all:true})','dns.promises.lookup("127.0.0.1",{hints:dns.ADDRCONFIG|dns.V4MAPPED|dns.ALL})']
    source="import dns from 'node:dns';\n"+'\n'.join(f'''try{{console.log(JSON.stringify({{value:await ({expr})}}));}}catch(e){{console.log(JSON.stringify({{error:e.name,code:e.code}}));}}''' for expr in cases)
    with tempfile.TemporaryDirectory(prefix='mite-dns-') as directory:
        fixture=Path(directory)/'validation.mjs';fixture.write_text(source)
        results=[subprocess.run([exe,str(fixture)],capture_output=True,text=True,timeout=10) for exe in [args.node,args.mite]]
    for result in results:
        if result.returncode or result.stderr:print(result);return 1
    rows=[[json.loads(line) for line in result.stdout.splitlines()] for result in results]
    failures=0
    for expr,expected,actual in zip(cases,*rows):
        if expected!=actual:failures+=1;print(f'FAIL {expr}: Node={expected!r} Mite={actual!r}')
    if any(len(row)!=len(cases) for row in rows):failures+=1
    print(f'{len(cases)} DNS validation comparisons, {failures} differences')
    return int(bool(failures))


if __name__=='__main__':raise SystemExit(main())
