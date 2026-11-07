#!/usr/bin/env python3
"""Test exact engine seams and run the real Code-OSS configuration function on Node/Mite."""

import _support

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from code_oss.engine import (
    adapt_engine,
    IMPORT,
    ADDED_IMPORT,
    CONFIG,
    CHECK,
    START,
    OPEN,
    END,
    CLOSE,
)
from code_oss.overlay import prepare, FILES

ROOT = Path(__file__).resolve().parents[2]
PINNED = ROOT / "vendor/code-oss-tests"


def rejected(callback):
    try:
        callback()
    except ValueError:
        return
    raise AssertionError("Engine overlay accepted drift")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    assert args.node
    manifest = json.loads((PINNED / "manifest.json").read_text())
    original = (PINNED / "mainImpl.ts").read_bytes()
    assert hashlib.sha256(original).hexdigest() == manifest["files"]["mainImpl.ts"]["sha256"]
    adapted = adapt_engine(original)
    restored = (
        adapted.decode()
        .replace(ADDED_IMPORT, "")
        .replace(CHECK, "")
        .replace(OPEN + START, START)
        .replace(CLOSE + END, END)
    )
    assert restored.encode() == original, "Original bytes changed outside declared insertions"
    for seam in (IMPORT, CONFIG, START, END):
        rejected(lambda: adapt_engine(original.replace(seam.encode(), b"changed")))
        rejected(lambda: adapt_engine(original + b"\n" + seam.encode()))
    with tempfile.TemporaryDirectory(prefix="elytra-code-oss-engine-") as directory:
        work = Path(directory).resolve()
        source = work / "source"
        for name in (*FILES, "mainImpl.ts", "nodeCompileCache.ts", "LICENSE.txt"):
            record = manifest["files"][name]
            target = source / record["upstream"]
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((PINNED / name).read_bytes())
        overlay = work / "overlay"
        report = prepare(source, overlay, libjs_cache=True, libjs_engine=True)
        assert len(report["adaptations"]) == 3
        assert (overlay / "src/mainImpl.ts").read_bytes() == adapted
        assert (overlay / "LICENSE.txt").read_bytes() == (PINNED / "LICENSE.txt").read_bytes()
        assert (source / "src/mainImpl.ts").read_bytes() == original
        target = source / "src/mainImpl.ts"
        target.write_bytes(original + b"\n")
        refused = work / "refused"
        rejected(lambda: prepare(source, refused, libjs_engine=True))
        assert not refused.exists(), "Output created before input validation"
        target.write_bytes(original)
        rejected(lambda: prepare(source, source / "overlap", libjs_engine=True))
        for kind, data in (("original", original), ("adapted", adapted)):
            tree = work / kind
            tree.mkdir()
            text = data.decode()
            configure = text[
                text.index("function configureCommandlineSwitchesSync(") : text.index(
                    "\ninterface IArgvConfig"
                )
            ]
            flags = original.decode()
            flags = flags[
                flags.index("function getJSFlags(") : flags.index("\nfunction parseCLIArgs(")
            ]
            imports = (
                "import { isLibJSHost, assertLibJSStartupOptions, recordLibJSDefaults } from './engine-policy.js';\n"
                if kind == "adapted"
                else ""
            )
            harness = (
                imports
                + """
const fixture = JSON.parse(process.argv[2]);
const before = process.argv.length;
const calls = [];
const app = {
  disableHardwareAcceleration() { calls.push(['disableHardwareAcceleration']); },
  commandLine: {
    hasSwitch(name) { return Object.hasOwn(fixture.native ?? {}, name); },
    getSwitchValue(name) { return fixture.native?.[name] ?? ''; },
    appendSwitch(...args) { calls.push(args); }
  }
};
function readArgvConfigSync() { return fixture.config; }
"""
                + configure
                + "\n"
                + flags
                + """
let failure;
try { configureCommandlineSwitchesSync(fixture.cli); } catch(error) { failure = {code: error.code, message: error.message}; }
console.log(JSON.stringify({calls, added: process.argv.slice(before), failure, status: process._vscodeLibJSEngineStatus,
 frozen: process._vscodeLibJSEngineStatus ? Object.isFrozen(process._vscodeLibJSEngineStatus) && Object.isFrozen(process._vscodeLibJSEngineStatus.omitted) && process._vscodeLibJSEngineStatus.omitted.every(Object.isFrozen) : undefined }));
"""
            )
            (tree / "main.ts").write_text(harness)
            shutil.copyfile(
                ROOT / "firefly/compat/code-oss/engine-policy.ts", tree / "engine-policy.ts"
            )
            transform = """import fs from 'node:fs'; import {pathToFileURL} from 'node:url';
const [directory,compiler]=process.argv.slice(1); const {transformSync,version}=await import(pathToFileURL(compiler));
if(version!=='0.25.12')throw Error('Wrong build compiler');
for(const name of ['main','engine-policy']) fs.writeFileSync(`${directory}/${name}.mjs`,transformSync(fs.readFileSync(`${directory}/${name}.ts`,'utf8'),{loader:'ts',format:'esm',target:'es2024'}).code.replace("'./engine-policy.js'", "'./engine-policy.mjs'").replace('"./engine-policy.js"','"./engine-policy.mjs"'));
"""
            subprocess.run(
                [
                    args.node,
                    "--input-type=module",
                    "-e",
                    transform,
                    str(tree),
                    str(ROOT / "tooling/runtime-js/node_modules/esbuild/lib/main.js"),
                ],
                check=True,
                timeout=20,
            )

        def execute(binary, kind, fixture):
            result = subprocess.run(
                [str(binary), str(work / kind / "main.mjs"), json.dumps(fixture)],
                capture_output=True,
                text=True,
                timeout=20,
            )
            assert result.returncode == 0, result
            return json.loads(result.stdout)

        normal = {
            "cli": {"sandbox": True, "no-sandbox": True},
            "config": {
                "log-level": "debug",
                "enable-proposed-api": ["publisher.test"],
                "use-inmemory-secretstorage": True,
            },
        }
        original_node = execute(args.node, "original", normal)
        adapted_node = execute(args.node, "adapted", normal)
        assert original_node == adapted_node and len(adapted_node["calls"]) == 5, (
            original_node,
            adapted_node,
        )
        mite = execute(args.mite, "adapted", normal)
        assert (
            not mite["calls"] and mite["added"] == original_node["added"] and "failure" not in mite
        ), mite
        assert mite["status"]["rendererCompatibility"] == "not-established" and mite["frozen"], mite
        assert len(mite["status"]["omitted"]) == (11 if sys.platform.startswith("linux") else 10), (
            mite
        )

        negatives = [
            {"cli": {"sandbox": False}, "config": {}},
            {"cli": {"sandbox": True}, "config": {"disable-chromium-sandbox": True}},
            {"cli": {"sandbox": True, "js-flags": "--expose-gc"}, "config": {}},
            {
                "cli": {"sandbox": True},
                "config": {"force-color-profile": "srgb", "log-level": "debug"},
            },
            {"cli": {"sandbox": True}, "config": {}, "native": {"enable-features": ""}},
            {"cli": {"sandbox": True}, "config": {}, "native": {"no-sandbox": "false"}},
            {"cli": {"sandbox": True}, "config": {"js-flags": "--max-old-space-size=128"}},
            {"cli": {"sandbox": True}, "config": {"enable-rdp-display-tracking": True}},
        ]
        for fixture in negatives:
            result = execute(args.mite, "adapted", fixture)
            assert (
                result["failure"]["code"] == "ERR_CODE_OSS_LIBJS_ENGINE_OPTION"
                and not result["calls"]
                and not result["added"]
                and "status" not in result
            ), result
        print(
            "PASS exact reversible Code-OSS engine seams/input/license preservation; unchanged Node tuning; actual Mite defaults and 8 rejected user/sandbox requests before mutations"
        )


if __name__ == "__main__":
    main()
