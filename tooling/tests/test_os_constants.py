#!/usr/bin/env python3
"""Compare every available OS constant, descriptor, prototype and shared identity."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

SOURCE = r"""
(async()=>{
const os=require('node:os'),c=os.constants;
const ns=await import('node:os');
function inspect(object){
 return {keys:Reflect.ownKeys(object),nullPrototype:Object.getPrototypeOf(object)===null,extensible:Object.isExtensible(object),frozen:Object.isFrozen(object),
 descriptors:Object.fromEntries(Object.entries(Object.getOwnPropertyDescriptors(object)).map(([key,d])=>[key,{type:typeof d.value,writable:d.writable,enumerable:d.enumerable,configurable:d.configurable,value:typeof d.value==='object'?inspect(d.value):d.value}]))};
}
const snapshot=inspect(c),root=Object.getOwnPropertyDescriptor(os,'constants');
// Explicit strict functions keep this check independent of surrounding CJS mode.
for(const operation of [function(){'use strict';c.errno.EACCES=-1},function(){'use strict';os.constants={}},function(){'use strict';c.signals.extra=1}]){
 try{operation();throw Error('readonly operation succeeded')}catch(error){if(!(error instanceof TypeError))throw error;}
}
const immutable=Reflect.set(c.errno,'EACCES',-1)===false && Reflect.deleteProperty(c.errno,'EACCES')===false && Reflect.set(os,'constants',{})===false && Reflect.deleteProperty(os,'constants')===false && Reflect.set(c.signals,'extra',1)===false;
c.errno.extra=7;const extensible=c.errno.extra===7;delete c.errno.extra;
const saved=c.errno;c.errno={replacement:true};const groupReplace=ns.constants.errno.replacement===true;c.errno=saved;
console.log(JSON.stringify({exports:Object.keys(os).sort(),named:Object.keys(ns).sort(),snapshot,root:{writable:root.writable,enumerable:root.enumerable,configurable:root.configurable},
 identity:c===ns.constants&&c===ns.default.constants&&c===require('os').constants,immutable,extensible,groupReplace,
 udp:c.UV_UDP_REUSEADDR,normal:c.priority.PRIORITY_NORMAL}));
})().catch(error=>{console.error(error);process.exitCode=1});
"""
POISON = """Object.freeze=()=>{throw Error('public freeze must not be called')};const os=require('os');console.log(Object.isFrozen(os.constants.signals));"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required")
    with tempfile.TemporaryDirectory(prefix="mite-os-constants-") as directory:
        fixture = Path(directory) / "constants.cjs"
        fixture.write_text(SOURCE)
        results = [
            subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=15)
            for exe in [args.node, args.mite]
        ]
        for result in results:
            assert result.returncode == 0 and not result.stderr, result
        expected, actual = [json.loads(result.stdout) for result in results]
        assert expected == actual, (expected, actual)
        assert all(
            actual[key] for key in ["identity", "immutable", "extensible", "groupReplace"]
        ), actual
        assert actual["root"] == dict(writable=False, enumerable=True, configurable=False), actual[
            "root"
        ]
        fixture.write_text(POISON)
        for executable in [args.node, args.mite]:
            result = subprocess.run(
                [executable, str(fixture)], capture_output=True, text=True, timeout=10
            )
            assert (
                result.returncode == 0 and not result.stderr and result.stdout.strip() == "true"
            ), result
    groups = actual["snapshot"]["descriptors"]
    count = 1 + sum(
        len(groups[name]["value"]["keys"]) for name in ["dlopen", "errno", "signals", "priority"]
    )
    print(
        f"PASS {count} native OS constant values with descriptors/order/null prototypes/freeze/mutation, CJS/ESM identity and poisoned public freeze control"
    )


if __name__ == "__main__":
    main()
