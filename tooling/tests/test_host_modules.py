#!/usr/bin/env python3
"""Verify registered native modules through actual CJS/ESM and GC lifetimes."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def run(binary, script):
    return subprocess.run([str(binary), str(script)], text=True, capture_output=True, timeout=15)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, type=Path)
    parser.add_argument("--mite", required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mite-host-modules-") as directory:
        root = Path(directory)
        (root / "package.json").write_text(
            json.dumps({"type": "module", "imports": {"#host": "embedded-host"}})
        )
        package = root / "node_modules/embedded-host"
        package.mkdir(parents=True)
        (package / "package.json").write_text('{"type":"module","exports":"./index.mjs"}')
        (package / "index.mjs").write_text("export default {fromPackage:true}")
        (root / "bridge.cjs").write_text(
            "const host=require('embedded-host'); host.state.count++; module.exports=host"
        )
        entry = root / "entry.mjs"
        entry.write_text("""
import host, {state, onPulse, class as marker} from 'embedded-host';
import * as namespace from 'embedded-host';
import alias from '#host';
import defaultOnly from 'host-default-only';
import cjs from './bridge.cjs';
if (host !== cjs || host !== alias || host !== defaultOnly || state !== host.state || state.count !== 1 || marker !== 'reserved-export') throw Error('shared identity or named exports');
if (Object.keys(namespace).sort().join() !== 'class,default,onPulse,state') throw Error('fixed exports');
const again = await import('embedded-host');
if (again !== namespace || again.default !== host) throw Error('ESM cache identity');
host.class = 'mutated';
if (marker !== 'reserved-export' || host.class !== 'mutated') throw Error('named export snapshot');
state.count = 9;
const value = await new Promise(resolve => onPulse(() => resolve(state.count)));
collectHostGarbage();
if (value !== 9 || again.state.count !== 9) throw Error('callback identity');
console.log('HOST_ESM_CJS_ALIAS_IDENTITY_OK');
""")
        result = run(args.host, entry)
        assert (
            result.returncode == 0
            and result.stdout == "HOST_ESM_CJS_ALIAS_IDENTITY_OK\n"
            and result.stderr == "HOST_SHUTDOWN\n"
        ), result
        entry = root / "entry.cjs"
        entry.write_text("""
collectHostGarbage();
const rooted = require('host-rooted');
if (rooted.label !== 'registry-root') throw Error('module registry must root exports');
const host = require('embedded-host');
if (host !== require('#host') || host !== require('registration-retry') || require.resolve('embedded-host') !== 'embedded-host') throw Error('CJS host resolution');
let denied = false;
try { require('node:embedded-host'); } catch (e) { if (e.code !== 'ERR_UNKNOWN_BUILTIN_MODULE') throw e; denied = true; }
if (!denied) throw Error('unexpected CJS node alias');
require.cache['embedded-host'] = {exports:{impostor:true}};
if (require('embedded-host') !== host) throw Error('host cache poisoning');
collectHostGarbage();
Promise.all([
  import('host-rooted').then(module => {if (module.default !== rooted || module.label !== 'registry-root') throw Error('rooted ESM identity')}),
  import('node:embedded-host').then(()=>{throw Error('unexpected node alias')}, e=>{if(e.code !== 'ERR_UNKNOWN_BUILTIN_MODULE') throw e}),
  import('mite-host:embedded-host').then(()=>{throw Error('unexpected internal URL')}, e=>{if(e.code !== 'ERR_UNSUPPORTED_ESM_URL_SCHEME') throw e}),
  import('mite:internal/host').then(()=>{throw Error('bridge leaked')}, e=>{if(e.code !== 'ERR_UNSUPPORTED_ESM_URL_SCHEME') throw e}),
  import('embedded-host', {with:{type:'json'}}).then(()=>{throw Error('wrong attributes accepted')}, e=>{if(e.code !== 'ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE') throw e})
]).then(()=>console.log('HOST_ROOTS_AND_REJECTIONS_OK'));
""")
        result = run(args.host, entry)
        assert (
            result.returncode == 0
            and result.stdout == "HOST_ROOTS_AND_REJECTIONS_OK\n"
            and result.stderr == "HOST_SHUTDOWN\n"
        ), result
        absent = root / "absent.mjs"
        absent.write_text("import {missing} from 'embedded-host'; console.log('must not execute')")
        result = run(args.host, absent)
        assert (
            result.returncode == 1
            and result.stdout == ""
            and "SyntaxError" in result.stderr
            and "missing" in result.stderr
        ), result
        local = root / "local.mjs"
        local.write_text("import value from 'embedded-host'; console.log(value.fromPackage)")
        result = run(args.mite, local)
        assert result.returncode == 0 and result.stdout == "true\n", result
        local = root / "absent.cjs"
        local.write_text(
            "try {require('host-rooted'); throw Error('module leaked')} catch(e) {if(e.code !== 'MODULE_NOT_FOUND') throw e; console.log('NO_HOST_MODULE')}"
        )
        result = run(args.mite, local)
        assert result.returncode == 0 and result.stdout == "NO_HOST_MODULE\n", result
    print("PASS registered CJS/ESM identity, aliases, shadow resistance and export snapshots")
    print("PASS export roots across GC, private bridge/URL and Node alias rejection")
    print("PASS registration validation, missing exports and standalone host-module absence")


if __name__ == "__main__":
    main()
