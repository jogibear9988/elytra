#!/usr/bin/env python3
"""Exact undici metadata adaptation and real Node/Mite identity controls."""

import _support
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--dependencies", type=Path, required=True)
    args = parser.parse_args()
    node = shutil.which("node")
    with tempfile.TemporaryDirectory(prefix="elytra-undici-") as directory:
        work = Path(directory).resolve()
        script = r"""
import fs from 'node:fs';import path from 'node:path';import assert from 'node:assert/strict';
import {pathToFileURL}from 'node:url';
const root=process.argv[1],work=process.argv[2],deps=process.argv[3];
const {planUndici,transformUndici,transformUndiciFeatures,transformUndiciWebIDL,applyUndici}=await import(pathToFileURL(path.join(root,'tooling/code_oss/undici.mjs')));
const {verifyPackageAdditions}=await import(pathToFileURL(path.join(root,'tooling/code_oss/package-adaptations.mjs')));
const plan=planUndici(deps);
const original=fs.readFileSync(path.join(root,'vendor/undici/lib/core/util.js'));
assert.throws(()=>transformUndici(Buffer.concat([original,Buffer.from('\n')])));
assert.throws(()=>transformUndici(plan.bytes));
assert.throws(()=>transformUndiciFeatures(Buffer.concat([fs.readFileSync(path.join(root,'vendor/undici/lib/util/runtime-features.js')),Buffer.from('\n')])));
assert.throws(()=>transformUndiciFeatures(plan.featureBytes));
fs.writeFileSync(path.join(work,'features.cjs'),plan.featureBytes);
const webidlOriginal=fs.readFileSync(path.join(root,'vendor/undici/lib/web/webidl/index.js'));
assert.throws(()=>transformUndiciWebIDL(Buffer.concat([webidlOriginal,Buffer.from('\n')])));
assert.throws(()=>transformUndiciWebIDL(plan.webidlBytes));
const traitSource=plan.webidlBytes.toString();
const makeStart=traitSource.indexOf('webidl.util.MakeTypeAssertion = function');
const makeEnd=traitSource.indexOf('\n}',makeStart)+2;
const trait=traitSource.slice(traitSource.indexOf('const __elytraMiteNoMessagePort'),traitSource.indexOf('\nwebidl.is.BufferSource'));
fs.writeFileSync(path.join(work,'trait.cjs'),'const webidl={util:{},is:{}};const FunctionPrototypeSymbolHasInstance=Function.call.bind(Function.prototype[Symbol.hasInstance]);\n'+traitSource.slice(makeStart,makeEnd)+'\n'+trait+'\nmodule.exports=webidl.is.MessagePort;');

const stage=path.join(work,'stage');fs.mkdirSync(path.join(stage,'node_modules'),{recursive:true});
fs.cpSync(fs.realpathSync(path.join(deps,'node_modules/undici')),path.join(stage,'node_modules/undici'),{recursive:true});
const copied=planUndici(stage);verifyPackageAdditions(stage,[],[{plan:copied}]);applyUndici(stage,copied);
assert.deepEqual(verifyPackageAdditions(stage,[copied.record],[{plan:copied}]),[]);
const audit=copied.record.auditFiles[0];fs.appendFileSync(path.join(stage,audit.path),'\nconst maliciousVersionConsumer=1;');
assert.throws(()=>verifyPackageAdditions(stage,[copied.record],[{plan:copied}]));
assert.throws(()=>planUndici(stage));
const {prepareUndiciMigration,applyUndiciMigration}=await import(pathToFileURL(path.join(root,'tooling/code_oss/migrate-undici.mjs')));
const legacyStage=path.join(work,'legacy');fs.mkdirSync(path.join(legacyStage,'node_modules'),{recursive:true});
fs.cpSync(fs.realpathSync(path.join(deps,'node_modules/undici')),path.join(legacyStage,'node_modules/undici'),{recursive:true});
const legacy=structuredClone(plan.record);delete legacy.additionalFiles;
legacy.adaptation='undefined-unused-node-version-metadata';legacy.adapterSource.sha256='fbb6d56a91df533cb8c2e2d3a83aba66ec6ce33dbecdc22e263f02a75c41443a';
const pin=JSON.parse(fs.readFileSync(path.join(root,'vendor/undici/manifest.json')));
legacy.auditFiles=pin.runtimeFiles.filter(file=>file.path!=='lib/core/util.js').map(file=>({path:'node_modules/undici/'+file.path,sha256:file.sha256}));
fs.writeFileSync(path.join(legacyStage,legacy.path),plan.bytes);
const others=Array.from({length:4},(_,index)=>({name:'owned-sentinel-'+index,path:'sentinel-'+index,outputSha256:pin.sourceSha256}));
for(const record of others)fs.writeFileSync(path.join(legacyStage,record.path),original);
const manifest={schema:1,kind:'diagnostic-source-tree',compiler:'esbuild 0.25.12',inputs:[],sentinel:'must remain byte identical',packageAdaptations:[...others,legacy]};
const metadata=path.join(legacyStage,'ELYTRA-COMPILE.json');fs.writeFileSync(metadata,JSON.stringify(manifest,null,2)+'\n');
const featureTarget=path.join(legacyStage,'node_modules/undici/lib/util/runtime-features.js'),featureOriginal=fs.readFileSync(featureTarget);
fs.appendFileSync(featureTarget,'\n');assert.throws(()=>prepareUndiciMigration(legacyStage));fs.writeFileSync(featureTarget,featureOriginal);
const migration=prepareUndiciMigration(legacyStage);assert.equal(migration.remainingAdapters,4);
applyUndiciMigration(migration);
assert.deepEqual(fs.readFileSync(path.join(legacyStage,legacy.path)),original);
assert.deepEqual(JSON.parse(fs.readFileSync(metadata)).packageAdaptations,others);
assert.throws(()=>prepareUndiciMigration(legacyStage));
const expanded=planUndici(legacyStage);verifyPackageAdditions(legacyStage,[],[{plan:expanded}]);applyUndici(legacyStage,expanded);
assert.deepEqual(verifyPackageAdditions(legacyStage,[expanded.record],[{plan:expanded}]),[]);
const secondStage=path.join(work,'legacy-two');fs.mkdirSync(path.join(secondStage,'node_modules'),{recursive:true});
fs.cpSync(fs.realpathSync(path.join(deps,'node_modules/undici')),path.join(secondStage,'node_modules/undici'),{recursive:true});
const second=structuredClone(plan.record);second.additionalFiles=second.additionalFiles.slice(0,1);
second.adaptation='undefined-unused-node-version-metadata-and-native-optional-feature-query';second.adapterSource.sha256='c717eeb2234ea908169f01051a98213dfd8829043e910bb927ae9df6f85fb10a';
second.auditFiles=pin.runtimeFiles.filter(file=>!['lib/core/util.js','lib/util/runtime-features.js'].includes(file.path)).map(file=>({path:'node_modules/undici/'+file.path,sha256:file.sha256}));
fs.writeFileSync(path.join(secondStage,second.path),plan.bytes);fs.writeFileSync(path.join(secondStage,second.additionalFiles[0].path),plan.featureBytes);
for(const record of others)fs.writeFileSync(path.join(secondStage,record.path),original);
fs.writeFileSync(path.join(secondStage,'ELYTRA-COMPILE.json'),JSON.stringify({...manifest,packageAdaptations:[...others,second]},null,2)+'\n');
const secondMigration=prepareUndiciMigration(secondStage);assert.equal(secondMigration.restores.length,2);applyUndiciMigration(secondMigration);
assert.deepEqual(fs.readFileSync(path.join(secondStage,second.additionalFiles[0].path)),fs.readFileSync(path.join(root,'vendor/undici/lib/util/runtime-features.js')));
assert.deepEqual(JSON.parse(fs.readFileSync(path.join(secondStage,'ELYTRA-COMPILE.json'))).packageAdaptations,others);
const third=planUndici(secondStage);verifyPackageAdditions(secondStage,[],[{plan:third}]);applyUndici(secondStage,third);
assert.deepEqual(verifyPackageAdditions(secondStage,[third.record],[{plan:third}]),[]);

const transformed=plan.bytes.toString();
const prefix=transformed.slice(transformed.indexOf('const __elytraMiteUndici'),transformed.indexOf('\nclass BodyAsyncIterable'));
fs.writeFileSync(path.join(work,'probe.mjs'),prefix+'\nexport {nodeMajor,nodeMinor};');
"""
        subprocess.run(
            [
                node,
                "--input-type=module",
                "-e",
                script,
                str(ROOT),
                str(work),
                str(args.dependencies.resolve()),
            ],
            check=True,
            cwd=ROOT,
            timeout=20,
        )
        fixture = work / "run.mjs"
        fixture.write_text("""
import assert from 'node:assert/strict';
import {createRequire,isBuiltin}from 'node:module';
const require=createRequire(import.meta.url);
const node=typeof process.versions.node==='string';
if(process.argv[2]==='conflict')process._vscodeLibJSUndiciStatus={};
let result,error;try{result=await import('./probe.mjs')}catch(e){error=e}
if(node){
 assert.equal(error,undefined);
 assert.deepEqual([result.nodeMajor,result.nodeMinor],process.versions.node.split('.',2).map(Number));
 if(process.argv[2]!=='conflict')assert.equal(process._vscodeLibJSUndiciStatus,undefined);
}else if(process.argv[2]==='conflict')assert.equal(error.code,'ERR_CODE_OSS_UNDICI_METADATA');
else{
 assert.equal(error,undefined);assert.equal(process.versions.node,undefined);
 assert.deepEqual([result.nodeMajor,result.nodeMinor],[undefined,undefined]);
 const d=Object.getOwnPropertyDescriptor(process,'_vscodeLibJSUndiciStatus');
 assert.equal(d.writable,false);assert.equal(d.configurable,false);assert.equal(d.enumerable,false);
 assert.equal(Object.isFrozen(d.value),true);assert.equal(d.value.available,false);
 assert.equal(d.value.feature,'node-version-metadata');
}
if(process.argv[2]==='feature-conflict')process._vscodeLibJSUndiciFeaturesStatus={};
const features=require('./features.cjs').runtimeFeatures;
if(!node&&process.argv[2]==='feature-conflict')assert.throws(()=>features.has('markAsUncloneable'),{code:'ERR_CODE_OSS_UNDICI_FEATURE'});
else{
 const expected=node?typeof require('node:worker_threads').markAsUncloneable!=='undefined':false;
 assert.equal(features.has('markAsUncloneable'),expected);features.clear();assert.equal(features.has('markAsUncloneable'),expected);
 if(!node){assert.equal(isBuiltin('node:worker_threads'),false);const d=Object.getOwnPropertyDescriptor(process,'_vscodeLibJSUndiciFeaturesStatus');assert.equal(Object.isFrozen(d.value),true);assert.equal(d.writable,false);assert.equal(d.configurable,false);assert.equal(d.value.available,false);assert.equal(d.value.feature,'markAsUncloneable')}
 else assert.equal(process._vscodeLibJSUndiciFeaturesStatus===undefined,process.argv[2]!=='feature-conflict');
 assert.equal(features.has('zstd'),typeof require('node:zlib').createZstdDecompress!=='undefined');
}
if(process.argv[2]==='trait-conflict')process._vscodeLibJSUndiciWebIDLStatus={};
if(process.argv[2]==='absent-node'&&node)delete globalThis.MessagePort;
if(!node&&process.argv[2]==='trait-conflict')assert.throws(()=>require('./trait.cjs'),{code:'ERR_CODE_OSS_UNDICI_WEBIDL'});
else if(node&&process.argv[2]==='absent-node')assert.throws(()=>require('./trait.cjs'),{name:'ReferenceError'});
else{
 const isPort=require('./trait.cjs');assert.equal(isPort({}),false);
 if(node){const channel=new MessageChannel();assert.equal(isPort(channel.port1),true);channel.port1.close();channel.port2.close();assert.equal(process._vscodeLibJSUndiciWebIDLStatus===undefined,process.argv[2]!=='trait-conflict')}
 else{assert.equal(typeof MessagePort,'undefined');const d=Object.getOwnPropertyDescriptor(process,'_vscodeLibJSUndiciWebIDLStatus');assert.equal(Object.isFrozen(d.value),true);assert.equal(d.writable,false);assert.equal(d.configurable,false);assert.equal(d.value.available,false)}
}
console.log('PASS genuine runtime identity and undici metadata');
""")
        for binary in [node, str(args.mite.resolve())]:
            for mode in ["valid", "conflict", "feature-conflict", "trait-conflict", "absent-node"]:
                result = subprocess.run(
                    [binary, str(fixture), mode], capture_output=True, text=True, timeout=15
                )
                assert result.returncode == 0 and not result.stderr, result
    print("PASS pinned undici runtime inventory, drift/refreshed-stage refusal, Node/Mite metadata")


if __name__ == "__main__":
    main()
