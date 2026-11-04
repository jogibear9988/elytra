#!/usr/bin/env python3
"""Compare tick/microtask checkpoint behavior in CJS, ESM and native callbacks."""
import argparse
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
    bodies = {
        'ordering': '''const out=[];
process.nextTick(()=>{out.push('tick');process.nextTick(()=>out.push('nested tick'));queueMicrotask(()=>out.push('tick microtask'));});
queueMicrotask(()=>{out.push('microtask');process.nextTick(()=>{out.push('microtask tick');queueMicrotask(()=>out.push('second microtask'));});});
Promise.resolve().then(()=>out.push('promise'));out.push('sync');
setTimeout(()=>console.log(out.join(',')),10);
''',
        'await': '''const out=[];
(async()=>{await 0;out.push('await one');process.nextTick(()=>out.push('tick inside await'));await 0;out.push('await two');})();
process.nextTick(()=>out.push('initial tick'));setTimeout(()=>console.log(out.join(',')),10);
''',
        'arguments': '''const result=process.nextTick(function(a,b){'use strict';console.log(this===undefined,a,b.value);},'value',{value:42});
console.log(result);
for(const value of [undefined,null,1,{},'x'])try{process.nextTick(value)}catch(e){console.log(e.name,e.code)}
''',
        'rejection': '''const p=Promise.reject(new Error('handled in tick'));process.nextTick(()=>p.catch(()=>console.log('handled')));
queueMicrotask(()=>process.nextTick(()=>console.log('last tick')));
''',
        'timer': '''setTimeout(()=>{const out=[];queueMicrotask(()=>out.push('microtask'));process.nextTick(()=>out.push('tick'));
setTimeout(()=>console.log(out.join(',')),5);},1);
''',
        'failure': '''process.nextTick(()=>{throw new Error('tick failure fixture')});process.nextTick(()=>console.log('unexpected late callback'));
''',
        'identity': '''import('node:process').then(({nextTick,default:p})=>console.log(nextTick===p.nextTick,nextTick===process.nextTick));
''',
    }
    failures=0
    with tempfile.TemporaryDirectory(prefix='mite-ticks-') as directory:
        root=Path(directory)
        for name,body in bodies.items():
            for suffix in ['cjs','mjs']:
                path=root/f'{name}.{suffix}';path.write_text(body)
                results=[subprocess.run([exe,str(path)],capture_output=True,text=True,timeout=15) for exe in [args.node,args.mite]]
                status=1 if name=='failure' else 0
                passed=(all(r.returncode==status for r in results) and results[0].stdout==results[1].stdout)
                if name=='failure': passed=passed and all('tick failure fixture' in r.stderr and 'unexpected late callback' not in r.stdout for r in results)
                else: passed=passed and all(not r.stderr for r in results)
                print(f'{"PASS" if passed else "FAIL"} nextTick/{name}.{suffix}')
                if not passed:
                    failures+=1;print('Node:',results[0]);print('Mite:',results[1])
        io=root/'io.mjs'
        io.write_text('''import fs from 'node:fs';
fs.readFile(import.meta.filename,()=>{const out=[];queueMicrotask(()=>out.push('microtask'));process.nextTick(()=>out.push('tick'));
setTimeout(()=>console.log(out.join(',')),5);});''')
        results=[subprocess.run([exe,str(io)],capture_output=True,text=True,timeout=15) for exe in [args.node,args.mite]]
        passed=all(r.returncode==0 and r.stdout=='tick,microtask\n' and not r.stderr for r in results)
        print(f'{"PASS" if passed else "FAIL"} nextTick/native filesystem callback')
        failures+=int(not passed)
        if not passed: print(results)
    return int(bool(failures))


if __name__=='__main__':
    raise SystemExit(main())
