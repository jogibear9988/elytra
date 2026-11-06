#!/usr/bin/env python3
"""Compare OS interface snapshots and independently verify address/CIDR metadata."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import ipaddress
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

SOURCE = r"""
(async()=>{
const os=require('node:os');
const snapshot=os.networkInterfaces(),later=os.networkInterfaces();
const entries=Object.values(snapshot).flat();
let traps=0;const bomb=new Proxy({},{get(){traps++;throw Error('Unused arguments must not be read')}});
const argument=Reflect.apply(os.networkInterfaces,bomb,[bomb,Symbol('ignored')]);
const namespace=await import('node:os');
const valid=Object.getPrototypeOf(snapshot)===Object.prototype && Object.values(snapshot).every(list=>Array.isArray(list)&&Object.getPrototypeOf(list)===Array.prototype && list.every(entry=>{
 const fields=['address','netmask','family','mac','internal','cidr'];if(entry.family==='IPv6')fields.push('scopeid');
 return Object.getPrototypeOf(entry)===Object.prototype && Object.keys(entry).join()===fields.join() && Object.values(Object.getOwnPropertyDescriptors(entry)).every(d=>d.writable && d.enumerable && d.configurable);
}));
let fresh=snapshot!==later;
for(const [name,list] of Object.entries(snapshot)){
 if(!later[name] || !list.length || !later[name].length)continue;
 fresh=fresh && list!==later[name] && list[0]!==later[name][0];
 const saved=later[name][0].address;list[0].address='mutation';fresh=fresh && later[name][0].address===saved;
}
console.log(JSON.stringify({snapshot:later,valid,fresh,traps,argumentValid:argument!==later&&Object.getPrototypeOf(argument)===Object.prototype,
 identity:namespace.networkInterfaces===os.networkInterfaces&&require('os')===os}));
})().catch(error=>{console.error(error);process.exitCode=1});
"""


def normalized(snapshot):
    return {
        name: sorted(entries, key=lambda row: json.dumps(row, sort_keys=True))
        for name, entries in snapshot.items()
    }


def validate(snapshot):
    count = 0
    loopback4 = False
    loopback6 = False
    for name, entries in snapshot.items():
        assert isinstance(name, str) and entries, (name, entries)
        for entry in entries:
            count += 1
            address = ipaddress.ip_address(entry["address"])
            mask = ipaddress.ip_address(entry["netmask"])
            assert address.version == mask.version, entry
            assert entry["family"] == f"IPv{address.version}", entry
            assert type(entry["internal"]) is bool and re.fullmatch(
                r"(?:[0-9a-f]{2}:){5}[0-9a-f]{2}", entry["mac"]
            ), entry
            bits = address.max_prefixlen
            value = int(mask)
            ones = bin(value).count("1")
            contiguous = value == (((1 << ones) - 1) << (bits - ones))
            assert entry["cidr"] == (f"{entry['address']}/{ones}" if contiguous else None), entry
            if address.version == 6:
                assert type(entry["scopeid"]) is int and 0 <= entry["scopeid"] <= 0xFFFFFFFF, entry
            else:
                assert "scopeid" not in entry, entry
            if address.is_loopback:
                assert entry["internal"], entry
                if address.version == 4:
                    loopback4 = True
                else:
                    loopback6 = True
    assert count and loopback4, (count, loopback4, loopback6)
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required")
    with tempfile.TemporaryDirectory(prefix="mite-os-network-") as directory:
        fixture = Path(directory) / "interfaces.cjs"
        fixture.write_text(SOURCE)
        results = [
            subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=15)
            for exe in [args.node, args.mite]
        ]
    rows = []
    for result in results:
        assert result.returncode == 0 and not result.stderr, result
        row = json.loads(result.stdout)
        assert (
            all(row[key] for key in ["valid", "fresh", "argumentValid", "identity"])
            and row["traps"] == 0
        ), row
        validate(row["snapshot"])
        rows.append(row["snapshot"])
    assert normalized(rows[0]) == normalized(rows[1]), (rows[0], rows[1])
    print(
        f"PASS {len(rows[1])} native interfaces / {validate(rows[1])} IPv4/IPv6 records, CIDR oracle, scope/MAC/loopback, fresh graphs, ignored args and module identity"
    )


if __name__ == "__main__":
    main()
