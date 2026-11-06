#!/usr/bin/env python3
"""Compare priority validation and native calls in isolated short-lived child processes."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

VALUES = [
    "undefined",
    "null",
    "false",
    "true",
    '"0"',
    '"1"',
    "{}",
    "[]",
    "()=>{}",
    "1n",
    'Symbol("pid")',
    "new Number(0)",
    "NaN",
    "Infinity",
    "-Infinity",
    "0.5",
    "-0.5",
    "2147483648",
    "-2147483649",
    "Number.MAX_SAFE_INTEGER",
]
PRIORITIES = VALUES + ["-21", "20"]
SOURCE_PREFIX = r"""
(async()=>{
const os=require('node:os');
const before=os.getPriority();
const rows=[];
function observe(operation){try{const value=operation();rows.push({value,undefined:value===undefined});}catch(error){rows.push({error:error.name,code:error.code,argument:error.message.includes('"pid"')?'pid':error.message.includes('"priority"')?'priority':undefined,errno:error.errno,syscall:error.syscall,info:error.info});}}
"""
SOURCE_SUFFIX = r"""
let traps=0;const hostile=new Proxy({},{get(){traps++;throw Error('Priority arguments must not be coerced')},ownKeys(){traps++;throw Error('No reflection')}});
const start=rows.length;observe(()=>os.getPriority(hostile));observe(()=>os.setPriority(hostile,before));observe(()=>os.setPriority(hostile));const hostileRows=rows.splice(start);
const sameByPid=os.getPriority(process.pid)===before && os.getPriority(undefined)===before;
const ownSet=os.setPriority(process.pid,before)===undefined && os.setPriority(before)===undefined;
const receiver=Reflect.apply(os.getPriority,hostile,[])===before;
const ns=await import('node:os');
const identity=ns.getPriority===os.getPriority && ns.setPriority===os.setPriority && os===require('os');
// Only lower scheduling priority of this disposable child, never another process.
const next=Math.min(19,before+1);
const setResult=os.setPriority(0,next),afterTwo=os.getPriority();
const twoArgs=setResult===undefined && (os.platform()==='win32'?Number.isInteger(afterTwo):afterTwo===next);
const oneArg=os.setPriority(19)===undefined && os.getPriority()===19;
console.log(JSON.stringify({rows,hostileRows,before,next,traps,sameByPid,ownSet,receiver,identity,twoArgs,oneArg,
 afterTwo,functionInfo:[os.getPriority.name,os.getPriority.length,os.setPriority.name,os.setPriority.length],final:os.getPriority()}));
})().catch(error=>{console.error(error);process.exitCode=1});
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required")
    cases = []
    for value in VALUES:
        cases.append(f"os.getPriority({value})")
    for value in PRIORITIES:
        cases.append(f"os.setPriority({value})")
        cases.extend(
            ["os.setPriority(before,undefined)"] * 2
            if value == "undefined"
            else [f"os.setPriority(0,{value})", f"os.setPriority(process.pid,{value})"]
        )
    for pid in ["undefined", "null", '"0"', "{}", "NaN", "0.1", "2147483648"]:
        cases.extend([f"os.setPriority({pid},null)", f"os.setPriority({pid},20)"])
    cases.extend(
        [
            "os.getPriority()",
            "os.getPriority(0)",
            "os.getPriority(process.pid)",
            "os.getPriority(2147483647)",
            "os.getPriority(-2147483648)",
            "os.getPriority(-1)",
        ]
    )
    script = (
        SOURCE_PREFIX + "\n".join("observe(()=>" + expr + ");" for expr in cases) + SOURCE_SUFFIX
    )
    with tempfile.TemporaryDirectory(prefix="mite-os-priority-") as directory:
        fixture = Path(directory) / "priority.cjs"
        fixture.write_text(script)
        results = [
            subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=15)
            for exe in [args.node, args.mite]
        ]
    for result in results:
        assert result.returncode == 0 and not result.stderr, result
    expected, actual = [json.loads(result.stdout) for result in results]
    node_hostile = expected.pop("hostileRows")
    mite_hostile = actual.pop("hostileRows")
    node_traps = expected.pop("traps")
    mite_traps = actual.pop("traps")
    assert mite_traps == 0 and mite_hostile == [
        {"error": "TypeError", "code": "ERR_INVALID_ARG_TYPE", "argument": name}
        for name in ["pid", "pid", "priority"]
    ], mite_hostile
    assert (node_traps == 3 and node_hostile == [{"error": "Error"}] * 3) or (
        node_traps == 0 and node_hostile == mite_hostile
    ), (node_traps, node_hostile)
    if expected != actual:
        for i, (node, mite) in enumerate(zip(expected["rows"], actual["rows"])):
            if node != mite:
                print("FAIL", cases[i] if i < len(cases) else "hostile argument", node, mite)
        raise AssertionError((expected, actual))
    assert len(actual["rows"]) == len(cases), actual
    assert all(
        actual[name]
        for name in ["sameByPid", "ownSet", "receiver", "identity", "twoArgs", "oneArg"]
    ), actual
    assert actual["final"] == 19 and actual["functionInfo"] == [
        "getPriority",
        1,
        "setPriority",
        2,
    ], actual
    with tempfile.TemporaryDirectory(prefix="mite-os-priority-zero-") as directory:
        fixture = Path(directory) / "negative-zero.cjs"
        fixture.write_text(
            "const os=require('node:os');console.log(JSON.stringify({value:os.getPriority(-0)}));"
        )
        reference = subprocess.run(
            [args.node, str(fixture)], capture_output=True, text=True, timeout=10
        )
        if reference.returncode:
            assert "Assertion failed:" in reference.stderr and "IsInt32()" in reference.stderr, (
                reference
            )
            zero_reference = "Node aborts; Mite normalizes PID -0"
        else:
            assert (
                not reference.stderr and json.loads(reference.stdout)["value"] == actual["before"]
            ), reference
            zero_reference = "Node and Mite normalize PID -0"
        fixture.write_text(
            "const os=require('os');const before=os.getPriority();const pid=os.getPriority(-0)===before && os.setPriority(-0,before)===undefined && os.getPriority()===before;const priority=before!==0 || (os.setPriority(-0)===undefined && os.getPriority()===0);console.log(JSON.stringify({pid,priority}));"
        )
        result = subprocess.run(
            [args.mite, str(fixture)], capture_output=True, text=True, timeout=10
        )
        assert (
            result.returncode == 0
            and not result.stderr
            and json.loads(result.stdout) == {"pid": True, "priority": True}
        ), result
    print(
        "INTENTIONAL DIFFERENCE:",
        zero_reference,
        "; error formatting does not read hostile argument properties",
    )
    print(
        f"PASS {len(actual['rows'])} priority validation/native observations, no coercion traps, CJS/ESM identity and own-child nice {actual['before']} -> {actual['next']} -> 19"
    )


if __name__ == "__main__":
    main()
