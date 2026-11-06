#!/usr/bin/env python3
"""Compile both projects and the pinned sample, test, package, and write release archives."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def run(*args,**kwargs):subprocess.run(list(map(str,args)),cwd=ROOT,check=True,**kwargs)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    identity=parser.add_mutually_exclusive_group(required=True)
    identity.add_argument('--tag',help='Exact existing release tag')
    identity.add_argument('--snapshot',help='Version label for a non-release CI package, e.g. 0.1.0-dev.123')
    parser.add_argument('--jobs',type=int,default=3)
    parser.add_argument('--lto',action='store_true',help='Enable C++ LTO for Mite and Firefly; run all normal release gates')
    parser.add_argument('--shared-openssl',action='store_true',help='Experimental macOS shared OpenSSL provider; run all native/package gates')
    parser.add_argument('--optimize-size',action='store_true',help='Use the C++ size optimization profile for both projects; keep all release gates')
    args=parser.parse_args()
    if args.shared_openssl and (sys.platform!='darwin' or platform.machine()!='arm64'):parser.error('--shared-openssl currently requires native macOS ARM64')
    version_tag=args.tag or ('v'+args.snapshot)
    if not re.fullmatch(r'v\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?',version_tag):parser.error('Tag must be a version such as v0.1.0-dev.1')
    if args.jobs<1:parser.error('jobs must be positive')
    if shutil.which('npm') is None:parser.error('Node/npm are required build tools')
    if subprocess.run(['git','diff','--quiet','HEAD'],cwd=ROOT).returncode:parser.error('Commit tracked changes before building a release')
    if args.tag:
        tagged=subprocess.check_output(['git','rev-parse','refs/tags/'+args.tag+'^{commit}'],cwd=ROOT,text=True).strip()
        head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
        if tagged!=head:parser.error('Release tag must match the checked-out commit')
    run(sys.executable,'tooling/test_npm_payload.py')
    run(sys.executable,'tooling/bootstrap.py')
    build=ROOT/'build/release-mite'
    command=['cmake','-S',ROOT,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=ON',f'-DCMAKE_TOOLCHAIN_FILE={ROOT}/build/vcpkg/scripts/buildsystems/vcpkg.cmake']
    if os.name=='nt':command.extend(['-DCMAKE_C_COMPILER=clang-cl','-DCMAKE_CXX_COMPILER=clang-cl'])
    command.append(f'-DENABLE_LTO_FOR_RELEASE={"ON" if args.lto else "OFF"}')
    command.append(f'-DELYTRA_OPTIMIZE_SIZE={"ON" if args.optimize_size else "OFF"}')
    baseline_cpu = sys.platform=='darwin' and platform.machine()=='arm64'
    command.append(f'-DENABLE_CI_BASELINE_CPU={"ON" if baseline_cpu else "OFF"}')
    run(*command)
    if baseline_cpu:run(sys.executable,'tooling/check_macos_cpu.py','--build',build)
    if args.optimize_size:run(sys.executable,'tooling/check_size_optimization.py','--build',build)
    run('cmake','--build',build,'--target','mite','--parallel',args.jobs)
    npm=shutil.which('npm.cmd' if os.name=='nt' else 'npm')
    run(npm,'ci','--ignore-scripts','--no-audit','--no-fund','--prefix','tooling/npm-fixtures')
    run('ctest','--test-dir',build,'--output-on-failure')
    # This deliberately fails on unsupported native desktop platforms. A Mite
    # binary alone must never be relabeled as a successful complete release.
    renderer_options = (['--lto'] if args.lto else []) + (['--optimize-size'] if args.optimize_size else [])
    if args.shared_openssl:renderer_options.append('--shared-openssl')
    if baseline_cpu:renderer_options.append('--baseline-cpu')
    run(sys.executable,'tooling/build_renderer.py','--target','firefly','--jobs',args.jobs,*renderer_options)
    run(sys.executable,'tooling/bootstrap_electron.py')
    run('cmake','--build','build/renderer','--target','check-firefly','check-firefly-renderer','check-firefly-isolation','check-firefly-renderer-helper','--parallel',args.jobs)
    designer=ROOT/'build/designer-app'
    run(sys.executable,'tooling/build_designer.py','--output',designer,'--dependencies','production')
    mite=build/'bin'/('mite.exe' if os.name=='nt' else 'mite')
    run(sys.executable,'tooling/test_designer.py','--mite',mite,'--app',designer,'--firefly',ROOT/'build/renderer/bin/firefly')
    stage=ROOT/'build/release-stage'
    if sys.platform!='darwin':raise RuntimeError('Native desktop packaging not implemented for '+sys.platform)
    run('cargo','fetch','--locked','--manifest-path',ROOT/'vendor/ladybird/Cargo.toml')
    run(sys.executable,'tooling/package_macos.py','--mite-build',build,'--output',stage,'--version',version_tag[1:],'--dependencies','production','--symbols','stripped')
    run(sys.executable,'tooling/test_public_preload.py','--firefly',stage/'Firefly.app/Contents/MacOS/firefly','--electron',ROOT/'build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron')
    packaged=stage/'Web Component Designer.app/Contents'
    run(sys.executable,'tooling/test_designer.py','--mite',stage/'bin/mite','--app',packaged/'Resources/app','--launcher',packaged/'MacOS/Launcher')
    # Separate runtime/framework/sample archives; all include relevant notices.
    destination=ROOT/'build/release-assets';destination.mkdir()
    suffix='macos-'+platform.machine().lower()
    items=[('mite',stage/'bin'),('firefly',stage/'Firefly.app'),('web-component-designer',stage/'Web Component Designer.app')]
    for name,item in items:
        directory=ROOT/'build'/('archive-'+name);directory.mkdir()
        if item.is_dir():shutil.copytree(item,directory/item.name,symlinks=True)
        else:shutil.copy2(item,directory/item.name)
        shutil.copytree(stage/'licenses',directory/'licenses')
        shutil.copy2(stage/'BUILD.json',directory/'BUILD.json')
        provider_report=stage/('OPENSSL-'+item.stem+'.json')
        if provider_report.is_file():shutil.copy2(provider_report,directory/'OPENSSL.json')
        base=destination/f'{name}-{version_tag}-{suffix}'
        if sys.platform=='darwin':
            run('ditto','-c','-k','--sequesterRsrc','--keepParent',directory,str(base)+'.zip')
        else:raise RuntimeError('No archive implementation for this platform')
    sizes=json.loads((stage/'SIZE.json').read_text())
    sizes['archives']={file.name:file.stat().st_size for file in destination.iterdir() if file.is_file()}
    (destination/f'SIZE-{suffix}.json').write_text(json.dumps(sizes,indent=2)+'\n')
    manifest={file.name:hashlib.sha256(file.read_bytes()).hexdigest() for file in destination.iterdir() if file.is_file()}
    (destination/f'SHA256SUMS-{suffix}.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Verified release assets:',destination)

if __name__=='__main__':main()
