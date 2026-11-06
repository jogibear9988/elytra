#!/usr/bin/env python3
"""Compare net numeric-address classification and port validation with Node."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for differential tests")
    values = [
        "",
        "127.0.0.1",
        "0.0.0.0",
        "255.255.255.255",
        "127.00.0.1",
        "127.1",
        "256.1.1.1",
        "1.2.3.4.5",
        "1.2.3.4\\n",
        " 1.2.3.4",
        "1.2.3.4 ",
        "0x7f.0.0.1",
        "01.2.3.4",
        "::",
        "::1",
        "2001:db8::1",
        "2001:DB8::abcd",
        "::ffff:192.0.2.1",
        "fe80::1%en0",
        "fe80::1%42",
        "::1%",
        "::1%a%b",
        "::1%abc-1",
        "::1%a_b",
        "::1%a.b",
        "::1%a b",
        "::1%a/b",
        "::1%a:b",
        "::1%a+b",
        "::1%a!b",
        "[::1]",
        "1:2:3:4:5:6:7:8",
        "1:2:3:4:5:6:7:8:9",
        "1:2:3:4:5:6:192.0.2.1",
        "1:2:3:4:5:6:7",
        "12345::1",
        "::ffff:192.00.2.1",
        "1::2::3",
        "::g",
        "::1\\n",
        "::1\x00",
        "127.0.0.1\x00",
        "localhost",
        None,
        False,
        123,
        {},
        [],
    ]
    source = (
        "import net from 'node:net';\n"
        + f"const values={json.dumps(values)};\n"
        + """
function emit(fn){try{console.log(JSON.stringify(fn()));}catch(e){console.log(JSON.stringify([e.name,e.code]));}}
for(const value of values)emit(()=>[net.isIP(value),net.isIPv4(value),net.isIPv6(value)]);
for(const value of [-1,65536,1.2,NaN,Infinity,'','  ','65536',null,false,{},[]]) {
 emit(()=>{try{new net.Socket().connect({port:value,host:'127.0.0.1'});}catch(e){return [e.name,e.code];}return 'unexpected valid port';});
}
for(const key of ['objectMode','readableObjectMode','writableObjectMode'])emit(()=>new net.Socket({[key]:true}));
"""
    )
    with tempfile.TemporaryDirectory(prefix="mite-net-") as directory:
        fixture = Path(directory) / "addresses.mjs"
        fixture.write_text(source)
        results = [
            subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=10)
            for exe in [args.node, args.mite]
        ]
    for result in results:
        if result.returncode or result.stderr:
            print(result)
            return 1
    rows = [[json.loads(line) for line in r.stdout.splitlines()] for r in results]
    failures = 0
    for index, (expected, actual) in enumerate(zip(*rows)):
        if expected != actual:
            print(
                f"FAIL {index}: {values[index] if index < len(values) else 'validation'!r} Node={expected!r} Mite={actual!r}"
            )
            failures += 1
    if len(rows[0]) != len(rows[1]):
        failures += 1
    print(f"{len(rows[0])} net address/validation comparisons, {failures} differences")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
