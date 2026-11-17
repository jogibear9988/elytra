#!/usr/bin/env python3
"""Check the pinned Writable admission composition without writing provider or bundle files."""

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]

CONTROL = r"""
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {createRequire} from 'node:module';
import {pathToFileURL} from 'node:url';
import {composeWritableSource, verifyWritableSources} from './tooling/runtime-js/streams/writable-source.mjs';

const directory = path.resolve('tooling/runtime-js/node_modules/readable-stream');
const pin = verifyWritableSources(directory);
const file = path.join(directory, pin.entry);
const original = fs.readFileSync(file, 'utf8');
const composed = composeWritableSource(original);
assert.equal(composed.replace(/  \/\/ Node 24 decides admission[\s\S]*?if \(!ret\) state.needDrain = true\n\n/, '').replace('  if (state.writing ||',
  '  // stream._write resets state.length\n  const ret = state.length < state.highWaterMark\n  // We must ensure that previous needDrain will not be reset to false.\n  if (!ret) state.needDrain = true\n  if (state.writing ||'), original);
const before = '  const ret = state.length < state.highWaterMark\n';
assert.ok(composed.startsWith(original.slice(0, original.indexOf("'use strict'"))));
for (const bad of [original + '\n', original.replace(before, ''), original.replace(before, before + before), composed]) {
  assert.throws(() => composeWritableSource(bad), /drifted|seam/);
}
assert.equal(fs.readFileSync(file, 'utf8'), original);

const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'mite-writable-source-'));
try {
  for (const name of Object.keys(pin.sha256)) {
    const target = path.join(temporary, name);
    fs.mkdirSync(path.dirname(target), {recursive:true});
    fs.copyFileSync(path.join(directory, name), target);
  }
  verifyWritableSources(temporary);
  fs.appendFileSync(path.join(temporary, 'LICENSE'), '\n');
  assert.throws(() => verifyWritableSources(temporary), /license drifted/);
  fs.copyFileSync(path.join(directory, 'LICENSE'), path.join(temporary, 'LICENSE'));
  const builder = path.join(temporary, 'builder');
  fs.mkdirSync(path.join(builder, 'streams'), {recursive:true});
  for (const name of ['writable-source.mjs', 'writable-source-pin.json']) {
    fs.copyFileSync(path.resolve('tooling/runtime-js/streams', name), path.join(builder, 'streams', name));
  }
  const lock = JSON.parse(fs.readFileSync('tooling/runtime-js/package-lock.json'));
  lock.packages['node_modules/readable-stream'].integrity = 'sha512-drift';
  fs.writeFileSync(path.join(builder, 'package-lock.json'), JSON.stringify(lock));
  const copied = await import(pathToFileURL(path.join(builder, 'streams/writable-source.mjs')).href);
  assert.throws(() => copied.verifyWritableSources(temporary), /lock drifted/);
} finally {
  fs.rmSync(temporary, {recursive:true});
}

// Execute the composed portable provider on Node as a source control. This is
// neither generated runtime code nor evidence of a rebuilt Mite executable.
const require = createRequire(import.meta.url);
const Module = require('node:module');
const load = Module._extensions['.js'];
let transformed = 0;
Module._extensions['.js'] = (module, filename) => {
  if (filename === file) {
    ++transformed;
    module._compile(composeWritableSource(fs.readFileSync(filename, 'utf8')), filename);
  } else load(module, filename);
};
let Writable;
try {
  ({Writable} = require(path.join(directory, 'lib/stream.js')));
} finally {
  Module._extensions['.js'] = load;
}
assert.equal(transformed, 1);
const {default: checkWritable} = await import('./mite/tests/stream-writable/cases.mjs');
await checkWritable(Writable);
console.log('PASS Writable source pin, reversal, drift, lock and license controls');
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--host", type=Path)
    args = parser.parse_args()
    result = subprocess.run(
        [args.node, "--input-type=module", "-"],
        input=CONTROL,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=20,
    )
    assert result.returncode == 0 and not result.stderr, result
    assert result.stdout == (
        "PASS Writable sync/async admission, zero HWM, drain/FIFO and failure ordering\n"
        "PASS Writable source pin, reversal, drift, lock and license controls\n"
    ), result
    reference = subprocess.run(
        [args.node, str(ROOT / "mite/tests/stream-writable/main.mjs")],
        capture_output=True,
        text=True,
        timeout=20,
    )
    assert reference.returncode == 0 and not reference.stderr, reference
    assert reference.stdout == result.stdout.splitlines(keepends=True)[0], reference
    for label, binary in [("Mite", args.mite), ("embedded host", args.host)]:
        if binary is None:
            continue
        actual = subprocess.run(
            [str(binary.resolve()), str(ROOT / "mite/tests/stream-writable/main.mjs")],
            capture_output=True,
            text=True,
            timeout=20,
        )
        expected_stderr = "HOST_SHUTDOWN\n" if label == "embedded host" else ""
        assert actual.returncode == 0 and actual.stderr == expected_stderr, (label, actual)
        assert actual.stdout == reference.stdout, (label, actual)
        print("PASS actual " + label + " Writable admission controls")
    print(result.stdout, end="")


if __name__ == "__main__":
    main()
