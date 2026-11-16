/** Native process title coercion and independent worker-local metadata. */
import assert from 'node:assert/strict';
import process from 'node:process';
import { Worker } from 'node:worker_threads';
import { execFile } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const original = process.title;
assert.equal(typeof original, 'string');
assert.equal('title' in process, true);
const descriptor = Object.getOwnPropertyDescriptor(process, 'title');
assert.equal(descriptor.enumerable, true);
assert.equal(descriptor.configurable, true);
if (process.versions.mite) {
  assert.equal(typeof descriptor.get, 'function');
  assert.equal(typeof descriptor.set, 'function');
} else {
  assert.equal(descriptor.writable, true);
}
console.log('PASS process.title native property presence and descriptor');
try {
  for (const [input, expected] of [
    ['mt', 'mt'],
    [42, '42'],
    [null, 'null']
  ]) {
    process.title = input;
    assert.equal(process.title, expected);
  }
  let calls = 0;
  process.title = {
    toString() {
      calls++;
      return 'obj';
    }
  };
  assert.equal(calls, 1);
  assert.equal(process.title, 'obj');
  const failure = new Error('owned title conversion');

  assert.throws(
    () => {
      process.title = {
        toString() {
          throw failure;
        }
      };
    },
    (error) => error === failure
  );

  if (process.versions.mite) {
    assert.equal(process.title, 'obj');
  }
  process.title = 'obj';

  assert.throws(() => {
    process.title = Symbol('title');
  }, TypeError);

  if (process.versions.mite) {
    assert.equal(process.title, 'obj');
  }
  process.title = 'obj';
  console.log('PASS process.title string conversion, getter result and conversion errors');
  const worker = new Worker(new URL('./worker.mjs', import.meta.url));
  const received = new Promise((resolve, reject) => {
    worker.once('message', resolve);
    worker.once('error', reject);
  });
  const exited = new Promise((resolve) => worker.once('exit', resolve));
  const reply = await received;
  assert.equal(reply.before, 'obj');
  assert.deepEqual(reply.after, { local: 'worker-title' });
  assert.equal(reply.coercions, 0);
  assert.equal(reply.dataDescriptor, true);
  assert.equal(reply.pid, process.pid);
  assert.equal(await exited, 0);
  assert.equal(process.title, 'obj');
  const foreignOutput = await new Promise((resolve, reject) => {
    execFile(
      process.execPath,
      [fileURLToPath(new URL('./foreign.mjs', import.meta.url))],
      { encoding: 'utf8' },
      (error, stdout) => {
        if (error) {
          reject(error);
        } else {
          resolve(stdout);
        }
      }
    );
  });
  const foreign = JSON.parse(foreignOutput);
  assert.notEqual(foreign.pid, process.pid);
  assert.equal(foreign.title, 'kid');
  assert.equal(process.title, 'obj');
  console.log(
    'PASS worker-local and foreign process title assignments preserve owner native title'
  );
} finally {
  process.title = original;
}
assert.equal(process.title, original);
console.log('PASS original process title restored');
