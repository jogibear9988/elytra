#!/usr/bin/env python3
"""Exact reversal/default-off and bounded no-payload diagnostic controls."""

import _support
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

from code_oss import scanner_diagnostics as scanner
from code_oss.overlay import prepare

ROOT = Path(__file__).resolve().parents[2]


def rejects(callback):
    try:
        callback()
    except ValueError:
        return
    raise AssertionError("Expected scanner drift refusal")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--node", default="node")
    args = parser.parse_args()
    original = (args.source / scanner.SOURCE).read_bytes()
    patched = scanner.adapt_scanner(original)
    assert scanner.revert_scanner(patched) == original
    assert patched.count(b"await ") == original.count(b"await ")
    rejects(lambda: scanner.adapt_scanner(original + b"\n"))
    rejects(lambda: scanner.adapt_scanner(patched))
    rejects(lambda: scanner.revert_scanner(patched + b"\n"))
    rejects(lambda: scanner.revert_scanner(patched.replace(b"scanEnd('stat'", b"scanEnd('other'")))
    with tempfile.TemporaryDirectory(prefix="elytra-scanner-source-") as temporary:
        temporary = Path(temporary)
        default = prepare(args.source, temporary / "default")
        enabled = prepare(args.source, temporary / "enabled", extension_scan_diagnostics=True)
        assert not any(
            item["path"] in (scanner.SOURCE, scanner.HELPER_TARGET) for item in default["files"]
        )
        assert (temporary / "enabled" / scanner.SOURCE).read_bytes() == patched
        record = next(item for item in enabled["files"] if item["path"] == scanner.SOURCE)
        assert record["sourceSha256"] == scanner.SOURCE_SHA256
        assert record["overlaySha256"] == hashlib.sha256(patched).hexdigest()
        # Execute only the maintained logger helper, never the application.
        script = """
import fs from 'node:fs';
import ts from './tooling/code_oss/node_modules/typescript/lib/typescript.js';
const source = fs.readFileSync(process.argv[1], 'utf8');
const js = ts.transpileModule(source, {compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.ES2022}}).outputText;
const records = [];
console.error = value => records.push(value);
const helper = await import('data:text/javascript;base64,' + Buffer.from(js).toString('base64'));
const id = helper.scanBegin('manifest');
helper.scanEnd('manifest', id);
helper.scanBegin('/private/DO_NOT_LOG');
for(let n=0;n<2000;++n) helper.scanEnd('manifest', id);
if(records.length !== 1024 || records[0] !== '[ELYTRA_EXTENSION_SCAN] phase=manifest edge=before counter=1' || !records[1].includes('edge=after counter=1') || records.some(value => value.includes('DO_NOT_LOG'))) throw Error('diagnostic bounds');
console.error = () => {throw Error('logger failure');};
const fresh = await import('data:text/javascript;base64,' + Buffer.from(js).toString('base64') + '#fresh');
const next = fresh.scanBegin('stat');
fresh.scanEnd('stat', next);
console.log('PASS bounded scalar logger');
"""
        result = subprocess.run(
            [args.node, "--input-type=module", "-e", script, str(ROOT / scanner.HELPER_SOURCE)],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
        assert "PASS bounded scalar logger" in result.stdout
    assert (args.source / scanner.SOURCE).read_bytes() == original
    print(
        "PASS scanner exact pin/reversal/drift, default-off overlay, unchanged awaits, bounded scalar logger"
    )


if __name__ == "__main__":
    main()
