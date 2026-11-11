#!/usr/bin/env python3
"""Exact deviceid adapter and real UUID/storage roundtrip in isolated HOME directories."""

import _support
import argparse
import hashlib
import json
import os
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
    pin = ROOT / "vendor/deviceid"
    manifest = json.loads((pin / "manifest.json").read_text())
    for name, record in manifest["files"].items():
        assert hashlib.sha256((pin / name).read_bytes()).hexdigest() == record["sha256"]
    with tempfile.TemporaryDirectory(prefix="elytra-deviceid-") as temporary:
        work = Path(temporary).resolve()
        package = work / "node_modules/@vscode/deviceid"
        shutil.copytree(pin, package)
        dependencies = package / "node_modules"
        dependencies.mkdir()
        original = args.dependencies.resolve() / "node_modules/@vscode/deviceid"
        (dependencies / "uuid").symlink_to(
            (original / "node_modules/uuid").resolve(), target_is_directory=True
        )
        (dependencies / "fs-extra").symlink_to(
            (args.dependencies.resolve() / "node_modules/fs-extra").resolve(),
            target_is_directory=True,
        )
        plan_script = """
import fs from 'node:fs';import path from 'node:path';
import{transformDeviceId,transformDeviceIdStorage,planDeviceId,applyDeviceId}from './tooling/code_oss/deviceid.mjs';
import{verifyPackageAdditions}from './tooling/code_oss/package-adaptations.mjs';
const root=process.argv[1],file=path.join(root,'node_modules/@vscode/deviceid/dist/devdeviceid.js');
const original=fs.readFileSync(file),adapted=transformDeviceId(original);
const restored=adapted.toString().replace(`const uuid_1 = typeof process.versions.mite === 'string' && process.versions.node === undefined && process.versions.electron === undefined ? undefined : require("uuid");`,'const uuid_1 = require("uuid");').replace('const newDeviceId = (0, (uuid_1 ?? await import("uuid")).v4)().toLowerCase();','const newDeviceId = (0, uuid_1.v4)().toLowerCase();');
if(!Buffer.from(restored).equals(original))throw Error('Not exactly reversible');
for(const changed of [Buffer.concat([original,Buffer.from('\\n')]),adapted]){let refused=false;try{transformDeviceId(changed)}catch{refused=true}if(!refused)throw Error('Accepted drift');}
const storageFile=path.join(root,'node_modules/@vscode/deviceid/dist/storage.js');
const storage=fs.readFileSync(storageFile),adaptedStorage=transformDeviceIdStorage(storage);
for(const changed of [Buffer.concat([storage,Buffer.from('\\n')]),adaptedStorage]){let refused=false;try{transformDeviceIdStorage(changed)}catch{refused=true}if(!refused)throw Error('Accepted storage drift');}
const plan=planDeviceId(root),plans=[{plan,apply:applyDeviceId}];
if(verifyPackageAdditions(root,[],plans).length!==1)throw Error('Missing new package adapter');
applyDeviceId(root,plan);
if(verifyPackageAdditions(root,[plan.record],plans).length!==0||plan.record.additionalFiles.length!==1)throw Error('Refresh omitted extra file');
fs.appendFileSync(storageFile,'\\n');let drift=false;
try{verifyPackageAdditions(root,[plan.record],plans)}catch{drift=true}
if(!drift)throw Error('Refresh accepted edited storage sidecar');
fs.writeFileSync(storageFile,adaptedStorage);
if(!fs.readFileSync(file).equals(adapted)||plan.record.name!=='@vscode/deviceid'||plan.record.adaptation!=='async-genuine-uuid-import-and-native-fs-promises')throw Error('Wrong plan');
"""
        subprocess.run(
            [node, "--input-type=module", "-e", plan_script, str(work)], cwd=ROOT, check=True
        )
        (work / "main.mjs").write_text("""
import {getDeviceId} from '@vscode/deviceid';
import fs from 'node:fs';import path from 'node:path';
const first=await getDeviceId(),second=await getDeviceId();
if(!/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(first)||first!==second)throw Error('Real UUID/storage mismatch');
const file=path.join(process.env.HOME,process.platform==='darwin'?'Library/Application Support':'.cache','Microsoft/DeveloperTools/deviceid');
if(fs.readFileSync(file,'utf8')!==first)throw Error('Actual storage file mismatch');
console.log('PASS genuine deviceid UUID generation and isolated storage readback');
""")
        for index, binary in enumerate([node, str(args.mite.resolve())]):
            home = work / f"home-{index}"
            home.mkdir()
            env = dict(os.environ, HOME=str(home))
            env.pop("XDG_CACHE_HOME", None)
            result = subprocess.run(
                [binary, str(work / "main.mjs")],
                env=env,
                capture_output=True,
                text=True,
                timeout=20,
            )
            assert (
                result.returncode == 0
                and result.stdout.strip()
                == "PASS genuine deviceid UUID generation and isolated storage readback"
            ), result
    print(
        "PASS pinned deviceid reversal/drift and real Node/Mite UUID plus temporary-HOME persistence"
    )


if __name__ == "__main__":
    main()
