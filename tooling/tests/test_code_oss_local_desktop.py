#!/usr/bin/env python3
"""Verify pinned opt-in adaptations and execute their leaf policy on Node/Mite."""

import _support

import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile

from code_oss.local_desktop import adapt_local, PATHS, OWNED, HELPER
from code_oss.overlay import prepare, FILES

ROOT = Path(__file__).resolve().parents[2]
PINNED = ROOT / "vendor/code-oss-tests"


def rejected(callback):
    try:
        callback()
    except ValueError:
        return
    raise AssertionError("Unvalidated overlay accepted")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    args = parser.parse_args()
    node = shutil.which("node")
    manifest = json.loads((PINNED / "manifest.json").read_text())
    with tempfile.TemporaryDirectory(prefix="elytra-local-policy-") as directory:
        work = Path(directory)
        source = work / "source"
        for name in (*FILES, "nodeCompileCache.ts", "mainImpl.ts", *PATHS, "LICENSE.txt"):
            data = (PINNED / name).read_bytes()
            record = manifest["files"][name]
            assert hashlib.sha256(data).hexdigest() == record["sha256"]
            target = source / record["upstream"]
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            if name == "mainImpl.ts" or name in PATHS:
                adapted = adapt_local(name, data)
                assert adapted != data
                # Reconstruct every original byte outside the explicitly owned insertions.
                restored = re.sub(
                    r"\nimport \{ [^\n]+elytraLocalDesktop.js';", "", adapted.decode()
                )
                restored = restored.replace("\nassertLocalDesktopCLI(args);", "")
                restored = restored.replace(
                    "const codeCachePath = localDesktopCodeCache(getCodeCachePath);",
                    "const codeCachePath = getCodeCachePath();",
                )
                if name == "mainImpl.ts":
                    restored = restored.replace(
                        "protocol.registerSchemesAsPrivileged(localDesktopSchemes([",
                        "protocol.registerSchemesAsPrivileged([",
                    )
                    start = restored.index("protocol.registerSchemesAsPrivileged([")
                    restored = restored[:start] + restored[start:].replace("\n]));", "\n]);", 1)
                restored = re.sub(
                    r"\n\t\tassertLocalDesktopFeature\('(webviews|remote resources)'\);",
                    "",
                    restored,
                )
                restored = restored.replace(
                    "\n\tassertLocalDesktopFeature('remote resources');", ""
                )
                restored = restored.replace(
                    "\n\t\tconst unavailable = rejectLocalDesktopFeature('remote');\n\t\tif (unavailable) { return unavailable; }",
                    "",
                )
                restored = restored.replace("\n\t\tif (isLocalDesktop()) { return; }", "")
                if name == "local-webview-main.ts":
                    seam = "\t\tthis._register(instantiationService.createInstance(WebviewProtocolProvider));"
                    restored = restored.replace(
                        "\t\tif (!isLocalDesktop()) {\n" + seam + "\n\t\t}", seam
                    )
                if name == "local-app-main.ts":
                    start = "\t\tconst remoteResourceChannel = new Lazy(() => mainProcessElectronServer.getChannel("
                    end = "\t\t\turl => remoteResourceChannel.value.call<NodeRemoteResourceResponse>(NODE_REMOTE_RESOURCE_IPC_METHOD_NAME, [url])\n\t\t);"
                    restored = restored.replace(
                        "\t\tif (!isLocalDesktop()) {\n" + start, start
                    ).replace(end + "\n\t\t}", end)
                assert restored.encode() == data, name
                rejected(lambda: adapt_local(name, adapted))
                # Missing/duplicate method entry seams and initialization seams fail closed.
                if name in ("mainImpl.ts", "local-remote-loader.ts"):
                    seam = (
                        b"const args = parseCLIArgs();"
                        if name == "mainImpl.ts"
                        else b"\t\tsuper();"
                    )
                    rejected(lambda: adapt_local(name, data.replace(seam, b"changed")))
                    rejected(lambda: adapt_local(name, data + b"\n" + seam))
        rejected(lambda: prepare(source, work / "invalid", local_desktop=True))
        assert not (work / "invalid").exists()
        report = prepare(
            source, work / "overlay", libjs_cache=True, libjs_engine=True, local_desktop=True
        )
        assert len(report["adaptations"]) == 4
        assert (work / "overlay" / HELPER).read_bytes() == (ROOT / OWNED).read_bytes()
        for name in PATHS:
            record = manifest["files"][name]
            assert (source / record["upstream"]).read_bytes() == (PINNED / name).read_bytes()
        drift = source / manifest["files"]["local-app-main.ts"]["upstream"]
        drift.write_bytes(drift.read_bytes() + b"\n")
        rejected(
            lambda: prepare(
                source, work / "drift", libjs_cache=True, libjs_engine=True, local_desktop=True
            )
        )
        assert not (work / "drift").exists()
        # Execute the real pinned registration literal, not an invented descriptor fixture.
        original = (PINNED / "mainImpl.ts").read_text()
        literal = (
            original.split("protocol.registerSchemesAsPrivileged(", 1)[1].split("\n]);", 1)[0]
            + "\n]"
        )
        harness = """
import * as policy from './policy.mjs';
const original = SCHEMES;
const enabled = policy.isLocalDesktop();
const before = JSON.stringify(original);
const selected = policy.localDesktopSchemes(original);
if (JSON.stringify(original) !== before) throw Error('Mutated original');
if (enabled) {
 if (selected.length !== 1 || selected[0].scheme !== 'vscode-file' || 'codeCache' in selected[0].privileges) throw Error('Schemes');
 for (const change of [[], [...original, original[0]], original.map((x,i)=>i?x:{...x, scheme:'changed'})]) {
  try {policy.localDesktopSchemes(change); throw Error('accepted drift');} catch(e) {if(e.code!=='ERR_CODE_OSS_LOCAL_DESKTOP_CONTRACT') throw e;}
 }
} else if (selected !== original) throw Error('Inactive identity');
let cacheCalls = 0;
const cache = policy.localDesktopCodeCache(()=>{cacheCalls++; return 'real-cache';});
if (cacheCalls !== (enabled?0:1) || cache !== (enabled?undefined:'real-cache')) throw Error('Cache policy');
policy.assertLocalDesktopCLI({_ : ['/tmp/local.txt']});
for (const cli of [{remote:'host'}, {'file-uri':['vscode-remote://host/a']}, {'folder-uri':'vscode-managed-remote-resource://host/a'}]) {
 let refused=false; try {policy.assertLocalDesktopCLI(cli);} catch(e){refused=e.code==='ERR_CODE_OSS_LOCAL_DESKTOP_UNSUPPORTED';}
 if(refused!==enabled) throw Error('CLI guard');
}
let refused=false;try{policy.assertLocalDesktopFeature('webviews');}catch(e){refused=e.code==='ERR_CODE_OSS_LOCAL_DESKTOP_UNSUPPORTED';}
if(refused!==enabled)throw Error('Feature guard');
const rejection=policy.rejectLocalDesktopFeature('remote');
if(enabled){try{await rejection;throw Error('resolved');}catch(e){if(e.code!=='ERR_CODE_OSS_LOCAL_DESKTOP_UNSUPPORTED')throw e;}}
else if(rejection!==undefined)throw Error('Inactive Promise change');
if(!Object.isFrozen(policy.localDesktopStatus())||!Object.isFrozen(policy.localDesktopStatus().disabled))throw Error('Mutable status');
process.env.ELYTRA_CODE_OSS_LOCAL_DESKTOP=enabled?'0':'1';
if(policy.isLocalDesktop()!==enabled)throw Error('Mutable mode');
console.log(JSON.stringify({enabled, scope:policy.localDesktopStatus().scope}));
""".replace("SCHEMES", literal)
        (work / "main.mjs").write_text(harness)
        transform = """import fs from 'node:fs'; import {transformSync} from './tooling/runtime-js/node_modules/esbuild/lib/main.js';
fs.writeFileSync(process.argv[1],transformSync(fs.readFileSync(process.argv[2],'utf8'),{loader:'ts',format:'esm',target:'es2024'}).code);"""
        subprocess.run(
            [
                node,
                "--input-type=module",
                "-e",
                transform,
                str(work / "policy.mjs"),
                str(ROOT / OWNED),
            ],
            cwd=ROOT,
            check=True,
        )
        for binary in (node, str(args.mite.resolve())):
            for flag in (None, "0", "1", "invalid"):
                env = dict(os.environ)
                env.pop("ELYTRA_CODE_OSS_LOCAL_DESKTOP", None)
                if flag is not None:
                    env["ELYTRA_CODE_OSS_LOCAL_DESKTOP"] = flag
                result = subprocess.run(
                    [binary, str(work / "main.mjs")],
                    env=env,
                    capture_output=True,
                    text=True,
                    timeout=20,
                )
                if binary != node and flag == "invalid":
                    assert (
                        result.returncode != 0
                        and "ELYTRA_CODE_OSS_LOCAL_DESKTOP must be 0 or 1" in result.stderr
                    ), result
                else:
                    assert result.returncode == 0, result
                    value = json.loads(result.stdout)
                    assert value["enabled"] == (binary != node and flag == "1"), value
    print(
        "Pinned local desktop overlay and actual Node/Mite policy checks passed (not workbench acceptance)"
    )


if __name__ == "__main__":
    main()
