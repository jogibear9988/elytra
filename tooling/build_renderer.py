#!/usr/bin/env python3
"""Build the separate LibWebView/WebContent embedding probe, leaving Ladybird untouched."""
import argparse
from pathlib import Path
import subprocess
import os
import json

from check_upstream import check

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',type=Path,default=ROOT/'build/renderer')
    parser.add_argument('--jobs',type=int,default=4)
    parser.add_argument('--configure-only',action='store_true')
    args=parser.parse_args()
    if args.jobs<1:parser.error('--jobs must be positive')
    check(ROOT)
    vcpkg=ROOT/'build/vcpkg'
    toolchain=vcpkg/'scripts/buildsystems/vcpkg.cmake'
    if not toolchain.is_file():parser.error('Run python3 tooling/bootstrap.py first')
    baseline=json.loads((ROOT/'vendor/ladybird/vcpkg.json').read_text())['builtin-baseline']
    revision=subprocess.check_output(['git','-C',str(vcpkg),'rev-parse','HEAD'],text=True).strip()
    if revision!=baseline:parser.error('vcpkg checkout does not match Ladybird baseline; run tooling/bootstrap.py')
    build=args.build_dir.resolve();build.mkdir(parents=True,exist_ok=True)
    command=['cmake','-S',str(ROOT/'vendor/ladybird'),'-B',str(build),'-G','Ninja',
        '-DCMAKE_BUILD_TYPE=Release',f'-DCMAKE_TOOLCHAIN_FILE={toolchain}',
        f'-DCMAKE_PROJECT_ladybird_INCLUDE={ROOT}/firefly/renderer/Inject.cmake',
        f'-DFIREFLY_SOURCE_DIR={ROOT}',f'-DLADYBIRD_CACHE_DIR={ROOT}/build/renderer-cache',
        '-DBUILD_TESTING=OFF','-DENABLE_JS_JIT=OFF','-DENABLE_LTO_FOR_RELEASE=OFF',
        '-DENABLE_LAGOM_CCACHE=OFF','-DENABLE_INSTALL_HEADERS=OFF','-DCMAKE_SKIP_INSTALL_RULES=ON']
    environment={**os.environ,'VCPKG_MAX_CONCURRENCY':str(args.jobs)}
    subprocess.run(command,cwd=ROOT,env=environment,check=True)
    if not args.configure_only:
        subprocess.run(['cmake','--build',str(build),'--target','firefly-renderer-probe','--parallel',str(args.jobs)],cwd=ROOT,check=True)
    check(ROOT)


if __name__=='__main__':main()
