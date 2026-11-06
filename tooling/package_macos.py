#!/usr/bin/env python3
"""Stage relocatable experimental macOS apps, preserve notices, and sign ad hoc."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
from npm_payload import copy_dependencies
from package_size import bundle_size
import sys
import tarfile

ROOT=Path(__file__).resolve().parents[1]
HELPERS=['AudioServer','Compositor','ImageDecoder','MediaServer','RequestServer','VideoDecoderServer','WebContent','WebRTCClient','WebWorker','ProcessReaper']

def run(*args,**kwargs):
    return subprocess.run(list(map(str,args)),check=True,**kwargs)

def output(*args):return subprocess.check_output(list(map(str,args)),text=True)

def dependencies(binary):
    return [line.strip().split(' (compatibility version',1)[0] for line in output('otool','-L',binary).splitlines()[1:]]

def rpaths(binary):
    return re.findall(r'cmd LC_RPATH\n\s+cmdsize \d+\n\s+path (.*?) \(offset',output('otool','-l',binary))

def signing_state(binary):
    result=run('codesign','-d','--verbose=4','--entitlements',':-',binary,capture_output=True)
    entitlements=plistlib.loads(result.stdout) if result.stdout.strip() else {}
    flags=re.search(rb'flags=(0x[0-9a-f]+)',result.stderr)
    if not flags:raise RuntimeError('Cannot inspect signature flags: '+str(binary))
    return entitlements,flags.group(1)

def check_helpers(source,destination):
    for helper in HELPERS:
        if signing_state(source/helper)!=signing_state(destination/helper):
            raise RuntimeError('Packaged helper entitlements/flags changed: '+helper)

def cache_value(build,key,default=''):
    for line in (build/'CMakeCache.txt').read_text().splitlines():
        if line.startswith(key+':'):return line.split('=',1)[1]
    return default

def notices(destination,renderer,mite):
    destination.mkdir(parents=True)
    shutil.copy2(ROOT/'LICENSE',destination/'Mite-Firefly-MIT.txt')
    shutil.copy2(ROOT/'vendor/ladybird/LICENSE',destination/'Ladybird-BSD-2-Clause.txt')
    shutil.copy2(ROOT/'vendor/ladybird.json',destination/'ladybird-pin.json')
    for label,build in [('renderer',renderer),('mite',mite)]:
        installed=Path(cache_value(build,'VCPKG_INSTALLED_DIR',str(build/'vcpkg_installed')))
        for file in installed.glob('*/share/*/copyright'):
            target=destination/'vcpkg'/label/file.parent.name;target.mkdir(parents=True,exist_ok=True);shutil.copy2(file,target/'copyright')
    metadata=json.loads(output('cargo','metadata','--offline','--locked','--format-version','1','--manifest-path',ROOT/'vendor/ladybird/Cargo.toml'))
    inventory=[]
    for package in metadata['packages']:
        if not package.get('source'):continue
        directory=Path(package['manifest_path']).parent
        target=destination/'rust'/(package['name']+'-'+package['version']);target.mkdir(parents=True)
        files=[file for file in directory.iterdir() if file.is_file() and any(word in file.name.lower() for word in ['license','licence','copying','notice','copyright'])]
        if package.get('license_file'):
            file=directory/package['license_file']
            if file.is_file() and file not in files:files.append(file)
        for file in files:shutil.copy2(file,target/file.name)
        shutil.copy2(directory/'Cargo.toml',target/'Cargo.toml')
        if not files:
            # Preserve the exact distributed sources/headers when a crate supplies
            # no standalone notice file, rather than inventing an author notice.
            with tarfile.open(target/'crate-source.tar.gz','w:gz') as archive:archive.add(directory,arcname=directory.name)
        inventory.append({key:package.get(key) for key in ['name','version','license','source']})
    (destination/'rust-packages.json').write_text(json.dumps(inventory,indent=2)+'\n')
    return len(inventory)

def stage_bundle(build,application,destination,name,identifier,version,licenses,dependencies_mode,strip_symbols):
    contents=destination/'Contents';mac=contents/'MacOS';mac.mkdir(parents=True)
    resources=contents/'Resources';resources.mkdir()
    # Physical resources keep sandbox access inside the new bundle.
    upstream=build/'bin/Ladybird.app/Contents'
    shutil.copytree(upstream/'Resources',resources,dirs_exist_ok=True)
    dependency_lock=application/'designer-package-lock.json'
    dependency_root=application/'web/node_modules'
    metadata=json.loads((application/'BUILD.json').read_text()) if dependency_lock.is_file() else {}
    bundled=metadata.get('webAssets')=='bundled'
    if bundled and metadata.get('dependencies',{}).get('mode')!=dependencies_mode:raise RuntimeError('Rebuild bundled Designer with the requested dependency mode')
    def omit_dependencies(directory,names):
        return ['node_modules'] if dependency_lock.is_file() and not bundled and Path(directory)==application/'web' else []
    shutil.copytree(application,resources/'app',ignore=omit_dependencies)
    if dependency_lock.is_file() and not bundled:
        metadata=json.loads((application/'BUILD.json').read_text())
        if dependencies_mode=='all' and metadata.get('dependencies',{}).get('mode')=='production':
            raise RuntimeError('Rebuild Designer with --dependencies all before packaging all dependencies')
        info=copy_dependencies(dependency_root,resources/'app/web/node_modules',dependency_lock,dependencies_mode)
        metadata['dependencies']=info;(resources/'app/BUILD.json').write_text(json.dumps(metadata,indent=2)+'\n')
    shutil.copytree(licenses,resources/'Licenses')
    shutil.copy2(build/'bin/firefly',mac/'firefly')
    runtime_helpers=build/'FireflyRuntimeHelper.app/Contents/MacOS'
    if not (runtime_helpers/'WebContent').is_file():raise RuntimeError('Build the production Firefly renderer helper before packaging')
    for helper in HELPERS:shutil.copy2(runtime_helpers/helper,mac/helper)
    libraries=contents/'lib';libraries.mkdir()
    shared_openssl=cache_value(build,'FIREFLY_SHARED_OPENSSL','OFF') in ['ON','TRUE','1']
    if shared_openssl and not (build/'lib/libfirefly-openssl.dylib').is_file():
        raise RuntimeError('Build the configured shared OpenSSL provider before packaging')
    for file in (build/'lib').glob('*.dylib'):
        if file.name=='libfirefly-openssl.dylib' and not shared_openssl:continue
        target=libraries/file.name
        if file.is_symlink():target.symlink_to(file.resolve().name)
        else:shutil.copy2(file,target)
    run('xcrun','clang++','-std=c++17','-O2',ROOT/'firefly/packaging/Launcher.cpp','-o',mac/'Launcher')
    binaries=[file for file in [*mac.iterdir(),*libraries.iterdir()] if file.is_file() and not file.is_symlink()]
    for binary in binaries:
        if strip_symbols:run('xcrun','strip','-S','-x',binary,stderr=subprocess.DEVNULL)
        for item in rpaths(binary):
            if item.startswith('/'):run('install_name_tool','-delete_rpath',item,binary,stderr=subprocess.DEVNULL)
        wanted='@loader_path' if binary.parent==libraries else '@executable_path/../lib'
        if wanted not in rpaths(binary):run('install_name_tool','-add_rpath',wanted,binary,stderr=subprocess.DEVNULL)
        for library in dependencies(binary):
            if library.startswith(('/System/Library/','/usr/lib/')):continue
            if library.startswith('@rpath/'):
                if not (libraries/library.removeprefix('@rpath/')).is_file():raise RuntimeError('Missing bundled dependency: '+library)
            elif library.startswith('@loader_path/'):
                if not (binary.parent/library.removeprefix('@loader_path/')).is_file():raise RuntimeError('Missing loader dependency: '+library)
            else:raise RuntimeError('Unpackaged non-system dependency: '+library)
        # RPATH edits invalidate existing seals. Keep upstream helper entitlements
        # and hardened-runtime flags; never disable the sandbox for packaging.
        run('codesign','--force','--sign','-','--preserve-metadata=entitlements,requirements,flags',binary,stderr=subprocess.DEVNULL)
    plist={'CFBundleExecutable':'Launcher','CFBundleIdentifier':identifier,'CFBundleName':name,'CFBundleDisplayName':name,'CFBundlePackageType':'APPL','CFBundleShortVersionString':re.split(r'[-+]',version)[0],'CFBundleVersion':re.split(r'[-+]',version)[0],'NSHighResolutionCapable':True,'LSMinimumSystemVersion':output('sw_vers','-productVersion').strip()}
    (contents/'Info.plist').write_bytes(plistlib.dumps(plist))
    run('codesign','--force','--sign','-',destination,stderr=subprocess.DEVNULL)
    run('codesign','--verify','--deep','--strict',destination)
    check_helpers(runtime_helpers,mac)
    return len(binaries)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renderer-build',type=Path,default=ROOT/'build/renderer')
    parser.add_argument('--mite-build',type=Path,default=ROOT/'build/dev')
    parser.add_argument('--designer',type=Path,default=ROOT/'build/designer-app')
    parser.add_argument('--output',type=Path,default=ROOT/'build/release-stage')
    parser.add_argument('--version',default='0.1.0')
    parser.add_argument('--dependencies',choices=['production','all'],default='production',help='Designer npm payload mode')
    parser.add_argument('--symbols',choices=['stripped','keep'],default='stripped',help='Keep dynamic exports; omit nonessential local/debug symbols by default')
    args=parser.parse_args()
    if sys.platform!='darwin':parser.error('Native macOS packaging only; Windows/Linux packaging is not implemented')
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?',args.version):parser.error('Invalid version')
    stage=args.output.resolve();renderer=args.renderer_build.resolve();mite=args.mite_build.resolve();designer=args.designer.resolve()
    if stage.exists():parser.error('Output exists; use a fresh staging directory')
    for file in [renderer/'bin/firefly',mite/'bin/mite',designer/'BUILD.json']:
        if not file.is_file():parser.error('Required build output missing: '+str(file))
    helper_source=(ROOT/'vendor/ladybird/Meta/CMake/ladybird_helper_processes.cmake').read_text()
    names=set(re.search(r'set\(ladybird_helper_processes\s+(.*?)\)',helper_source,re.S)[1].split())
    names.update(re.findall(r'list\(APPEND ladybird_helper_processes ([A-Za-z0-9_]+)\)',helper_source))
    if names!=set(HELPERS):parser.error('Upstream helper inventory changed; review the owned packaging adapter')
    stage.mkdir(parents=True)
    license_count=notices(stage/'licenses',renderer,mite)
    (stage/'bin').mkdir();shutil.copy2(mite/'bin/mite',stage/'bin/mite')
    if args.symbols=='stripped':run('xcrun','strip','-S','-x',stage/'bin/mite',stderr=subprocess.DEVNULL)
    run('codesign','--force','--sign','-',stage/'bin/mite',stderr=subprocess.DEVNULL)
    hello=stage/'hello-source';hello.mkdir();shutil.copy2(ROOT/'examples/firefly-hello-world/main.cjs',hello/'main.cjs');shutil.copy2(ROOT/'examples/firefly-hello-world/index.html',hello/'index.html');shutil.copy2(ROOT/'examples/firefly-hello-world/preload.cjs',hello/'preload.cjs')
    (hello/'main.mjs').write_text("import './main.cjs';\n")
    apps=[('Firefly',hello,'org.elytra.Firefly'),('Web Component Designer',designer,'org.elytra.WebComponentDesigner')]
    counts={}
    for name,app,identifier in apps:
        bundle=stage/(name+'.app');counts[name]=stage_bundle(renderer,app,bundle,name,identifier,args.version,stage/'licenses',args.dependencies,args.symbols=='stripped')
    shutil.rmtree(hello)
    manifest={'dependencies':args.dependencies,'symbols':args.symbols,'version':args.version,'trackedSourceDirty':subprocess.run(['git','diff','--quiet','HEAD'],cwd=ROOT).returncode!=0,'buildTypes':{'mite':cache_value(mite,'CMAKE_BUILD_TYPE'),'firefly':cache_value(renderer,'CMAKE_BUILD_TYPE')},'commit':output('git','-C',ROOT,'rev-parse','HEAD').strip(),'ladybird':json.loads((ROOT/'vendor/ladybird.json').read_text()),'designer':json.loads((designer/'upstream.json').read_text()),'platform':'macos','architecture':output('uname','-m').strip(),'buildHostOS':output('sw_vers','-productVersion').strip(),'signing':'ad-hoc; not notarized','nativeBinaries':counts,'rustNoticePackages':license_count}
    manifest['cppLTO']={name:cache_value(build,'ENABLE_LTO_FOR_RELEASE') for name,build in [('mite',mite),('firefly',renderer)]}
    manifest['cpuBaseline']={name:cache_value(build,'ENABLE_CI_BASELINE_CPU') for name,build in [('mite',mite),('firefly',renderer)]}
    manifest['sharedOpenSSL']=cache_value(renderer,'FIREFLY_SHARED_OPENSSL','OFF')
    if manifest['sharedOpenSSL'] in ['ON','TRUE','1']:
        for name,_,_ in apps:
            run(sys.executable,ROOT/'tooling/audit_openssl.py','--app',stage/(name+'.app'),'--renderer-build',renderer,'--require-shared','--output',stage/('OPENSSL-'+name+'.json'))
    manifest['rendererHelper']='firefly-runtime; isolated top-level CommonJS preload support'
    manifest['cppSizeOptimization']={name:{'enabled':cache_value(build,'ELYTRA_OPTIMIZE_SIZE'),'flag':cache_value(build,'ELYTRA_CPP_SIZE_FLAG')} for name,build in [('mite',mite),('firefly',renderer)]}
    (stage/'BUILD.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (stage/'SIZE.json').write_text(json.dumps({name:bundle_size(stage/(name+'.app')) for name,_,_ in apps},indent=2)+'\n')
    hashes={str(file.relative_to(stage)):hashlib.sha256(file.read_bytes()).hexdigest() for file in stage.rglob('*') if file.is_file() and not file.is_symlink()}
    (stage/'SHA256SUMS.json').write_text(json.dumps(hashes,indent=2)+'\n')
    print('Staged experimental native macOS packages:',stage)

if __name__=='__main__':main()
