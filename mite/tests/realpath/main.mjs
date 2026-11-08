/** Canonical path bytes, callback ordering and actual OS symlink resolution. */
import fs, { realpath, realpathSync } from 'node:fs';
import * as promises from 'node:fs/promises';
import { promisify } from 'node:util';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
import assert from 'node:assert/strict';

const target = realpathSync('target');
assert.equal(realpath, fs.realpath);
assert.equal(realpathSync, fs.realpathSync);
assert.equal(realpathSync.native('target'), target);
assert.equal(realpathSync('target/../target'), target);
assert.equal(realpathSync(pathToFileURL(path.resolve('target'))), target);
assert.equal(realpathSync(Buffer.from('target')), target);
assert.equal(realpathSync('target', 'buffer').toString(), target);
assert.equal(realpathSync('target', 'hex'), Buffer.from(target).toString('hex'));
assert.equal(realpathSync('target', { encoding: 'buffer' }).toString(), target);
assert.equal(await promises.realpath('target'), target);
assert.equal((await promises.realpath('target', { encoding: 'buffer' })).toString(), target);
assert.equal(await promisify(realpath)('target'), target);
assert.equal(await promisify(realpath.native)('target'), target);
let synchronous = true;
const pending = new Promise((resolve, reject) => {
  const returned = realpath('target', (error, result) => {
    try {
      assert.equal(synchronous, false);
      assert.equal(error, null);
      assert.equal(result, target);
      resolve();
    } catch (failure) {
      reject(failure);
    }
  });
  assert.equal(returned, undefined);
});
synchronous = false;
await pending;
for (const operation of [realpathSync, realpathSync.native]) {
  assert.throws(() => operation('missing'), { code: 'ENOENT' });

  assert.throws(() => operation('target\0bad'), { code: 'ERR_INVALID_ARG_VALUE' });

  assert.throws(() => operation('target', { encoding: 'not-an-encoding' }), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
}
await assert.rejects(promises.realpath('missing'), { code: 'ENOENT' });
await assert.rejects(promisify(realpath.native)('missing'), {
  code: 'ENOENT',
  syscall: 'realpath'
});

assert.throws(() => realpath('target'), { code: 'ERR_INVALID_ARG_TYPE' });

if (process.argv.includes('--symlinks')) {
  assert.equal(realpathSync('linked'), target);
  assert.equal(realpathSync.native('linked/child.txt'), path.join(target, 'child.txt'));
  assert.equal(await promises.realpath('linked/child.txt'), path.join(target, 'child.txt'));

  assert.throws(() => realpathSync('broken'), { code: 'ENOENT' });

  assert.throws(() => realpathSync.native('loop'), { code: 'ELOOP' });
}
if (process.argv.includes('--raw-bytes')) {
  const raw = Buffer.from([0x72, 0x61, 0x77, 0xff]);
  const expected = Buffer.concat([Buffer.from(realpathSync('.')), Buffer.from('/'), raw]);
  assert.deepEqual(realpathSync.native(raw, { encoding: 'buffer' }), expected);
  assert.deepEqual(await promises.realpath(raw, { encoding: 'buffer' }), expected);
}
// Caller mutation of the public wrapper must not replace the native entry point.
const native = fs.realpath.native;
const old = fs.realpath;

fs.realpath = () => {
  throw Error('Controlled replacement must stay isolated');
};

assert.equal(await promisify(native)('target'), target);
fs.realpath = old;
console.log(
  'PASS genuine canonical paths, encodings, callback ordering, errors and native entry points'
);
