#!/usr/bin/env python3
"""Check the pinned iterator composition without writing provider or bundle files."""

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
import {composeReadableSource, verifyReadableSources} from './tooling/runtime-js/streams/readable-source.mjs';

const directory = path.resolve('tooling/runtime-js/node_modules/readable-stream');
const pin = verifyReadableSources(directory);
const file = path.join(directory, pin.entry);
const original = fs.readFileSync(file, 'utf8');
const composed = composeReadableSource(original);
const before = original.slice(original.indexOf('  } finally {', original.indexOf('async function* createAsyncIterator')),
                              original.indexOf('      destroyImpl.destroyer', original.indexOf('async function* createAsyncIterator')));
const after = composed.slice(composed.indexOf('  } finally {', composed.indexOf('async function* createAsyncIterator')),
                             composed.indexOf('      destroyImpl.destroyer', composed.indexOf('async function* createAsyncIterator')));
assert.equal(composed.replace(after, before), original);
assert.ok(composed.startsWith(original.slice(0, original.indexOf("'use strict'"))));
for (const bad of [original + '\n', original.replace(before, ''), original.replace(before, before + before), composed]) {
  assert.throws(() => composeReadableSource(bad), /drifted|seam/);
}
assert.equal(fs.readFileSync(file, 'utf8'), original);

const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'mite-iterator-source-'));
try {
  for (const name of Object.keys(pin.sha256)) {
    const target = path.join(temporary, name);
    fs.mkdirSync(path.dirname(target), {recursive:true});
    fs.copyFileSync(path.join(directory, name), target);
  }
  verifyReadableSources(temporary);
  fs.appendFileSync(path.join(temporary, 'LICENSE'), '\n');
  assert.throws(() => verifyReadableSources(temporary), /license drifted/);
  fs.copyFileSync(path.join(directory, 'LICENSE'), path.join(temporary, 'LICENSE'));
  const builder = path.join(temporary, 'builder');
  fs.mkdirSync(path.join(builder, 'streams'), {recursive:true});
  for (const name of ['readable-source.mjs', 'readable-source-pin.json']) {
    fs.copyFileSync(path.resolve('tooling/runtime-js/streams', name), path.join(builder, 'streams', name));
  }
  const lock = JSON.parse(fs.readFileSync('tooling/runtime-js/package-lock.json'));
  lock.packages['node_modules/readable-stream'].integrity = 'sha512-drift';
  fs.writeFileSync(path.join(builder, 'package-lock.json'), JSON.stringify(lock));
  const copied = await import(pathToFileURL(path.join(builder, 'streams/readable-source.mjs')).href);
  assert.throws(() => copied.verifyReadableSources(temporary), /lock drifted/);
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
    module._compile(composeReadableSource(fs.readFileSync(filename, 'utf8')), filename);
  } else load(module, filename);
};
let Duplex;
try {
  ({Duplex} = require(path.join(directory, 'lib/stream.js')));
} finally {
  Module._extensions['.js'] = load;
}
assert.equal(transformed, 1);
const writes = [];
const stream = new Duplex({objectMode:true, allowHalfOpen:true,
  read() { this.push('input'); this.push(null); },
  write(chunk, encoding, callback) { writes.push(chunk); callback(); }
});
const values = [];
for await (const chunk of stream) values.push(chunk);
assert.deepEqual(values, ['input']);
assert.equal(stream.destroyed, false);
assert.equal(stream.writable, true);
await new Promise((resolve, reject) => { stream.once('error', reject); stream.end('reply', resolve); });
assert.deepEqual(writes, ['reply']);
console.log('PASS iterator source pin, reversal, drift, license and portable half-open control');
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--node", default=shutil.which("node"))
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
        "PASS iterator source pin, reversal, drift, license and portable half-open control\n"
    ), result
    print(result.stdout, end="")


if __name__ == "__main__":
    main()
