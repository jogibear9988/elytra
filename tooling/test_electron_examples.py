#!/usr/bin/env python3
"""Validate ESM examples against real runtimes, keeping blocked coverage explicit."""
import argparse
import json
from pathlib import Path
import subprocess
from test_firefly import execute

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firefly',required=True,type=Path)
    parser.add_argument('--electron',required=True,type=Path)
    parser.add_argument('--firefly-apps',required=True,type=Path)
    parser.add_argument('--electron-apps',required=True,type=Path)
    args=parser.parse_args()
    catalog=json.loads((ROOT/'examples/electron/catalog.json').read_text())['examples']
    for entry in catalog:
        for file in (ROOT/'examples/electron'/entry['id']).glob('*.mjs'):
            subprocess.run(['node','--check',str(file)],check=True,capture_output=True)
    for entry in catalog:
        if not entry['smoke']:
            result=execute(args.firefly.resolve(),args.firefly_apps.resolve()/entry['id']/'main.mjs')
            assert result.returncode==1 and ('ERR_FIREFLY_UNSUPPORTED_API:' in result.stderr or 'ERR_EXAMPLE_PLATFORM:' in result.stderr),(entry['id'],result)
            print('BLOCKED Firefly:',entry['id'],'(explicit API/platform rejection; not feature acceptance)',flush=True)
            continue
        observations=[]
        for binary,apps,module in [(args.firefly,args.firefly_apps,'firefly'),(args.electron,args.electron_apps,'electron')]:
            result=execute(binary.resolve(),apps.resolve()/entry['id']/'main.mjs',module,extra_args=['--smoke'])
            assert result.returncode==0,(entry['id'],module,result)
            values=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
            assert len(values)==1,(entry['id'],result)
            observations.append(values[0])
        assert observations[0]==observations[1],(entry['id'],observations)
        print('PASS shared native startup/assertions:',entry['id'],flush=True)
    for binary,apps,module in [(args.firefly,args.firefly_apps,'firefly'),(args.electron,args.electron_apps,'electron')]:
        result=execute(binary.resolve(),apps.resolve()/'lifecycle/main.mjs',module,extra_args=['--exit-code=7'])
        assert result.returncode==7,(module,result)
        values=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
        assert values==[{'trace':['ready','when-ready','quit:7']}],(module,result)
    print('PASS shared forced app.exit(7), without cancellable quit events',flush=True)
    print('Manual/reference feature examples remain unverified; no OS permissions, purchases, registrations or notifications were triggered.')


if __name__=='__main__':
    main()
