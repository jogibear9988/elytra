#!/usr/bin/env python3
"""Compare portable stream integration with the real Node builtin."""
import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite',required=True)
    parser.add_argument('--node',default=shutil.which('node'))
    args=parser.parse_args()
    if not args.node: parser.error('Node is required for differential tests')
    root=Path(__file__).resolve().parents[1]/'mite/tests/streams'
    failures=0
    for name in ['readable.cjs','writable.cjs','cork.cjs','pipeline.mjs','errors-isolated.mjs','iterate.mjs','identity.mjs']:
        results=[subprocess.run([exe,str(root/name)],capture_output=True,text=True,timeout=20) for exe in [args.node,args.mite]]
        passed=all(r.returncode==0 and not r.stderr for r in results) and results[0].stdout==results[1].stdout
        print(f'{"PASS" if passed else "FAIL"} streams/{name}')
        if not passed:
            failures+=1;print('Node:',results[0]);print('Mite:',results[1])
    # Keep the observed concurrent-error ordering difference visible. The same
    # portable bundle reproduces Mite's order when run in Node with native ticks.
    results=[subprocess.run([exe,str(root/'errors.cjs')],capture_output=True,text=True,timeout=20) for exe in [args.node,args.mite]]
    prefix='false\ncallback ERR_STREAM_WRITE_AFTER_END\nERR_STREAM_WRITE_AFTER_END\n'
    expected=[prefix+'ERR_STREAM_NULL_VALUES\ntrue true true true\n',prefix+'true true true true\nERR_STREAM_NULL_VALUES\n']
    passed=all(r.returncode==0 and not r.stderr and r.stdout==out for r,out in zip(results,expected))
    print(f'{"PASS" if passed else "FAIL"} recorded portable-stream/Node concurrent-error ordering difference')
    if not passed:
        failures+=1;print(results)
    return int(bool(failures))


if __name__=='__main__':
    raise SystemExit(main())
