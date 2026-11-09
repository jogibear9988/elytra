/** Real Node/embedded collector verifies lazy native records and assigned cache values. */
import assert from 'node:assert/strict';
import { assertOrder, stackCaptureTop } from './helpers.mjs';

const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
assert.equal(typeof collect, 'function', 'A genuine collector is required');
const marker = { name: 'NativeCaptured', message: 'survives GC' };
const target = {
  get name() {
    collect();
    return marker.name;
  },

  get message() {
    collect();
    return marker.message;
  }
};
stackCaptureTop(target);
collect();
assert.equal(target.stack.split('\n')[0], 'NativeCaptured: survives GC');
assertOrder(target.stack, ['stackCaptureLeaf', 'stackCaptureMiddle', 'stackCaptureTop']);

const cached = {};
Error.captureStackTrace(cached);

function assignEphemeralCache() {
  const value = { bytes: new Uint8Array(65536), marker: 'native cached value' };
  value.bytes[65000] = 91;
  cached.stack = value;
  return new WeakRef(value);
}

const reference = assignEphemeralCache();
await new Promise((resolve) => setTimeout(resolve, 10));
if (typeof collectHostGarbageAtIdle === 'function') {
  await collectHostGarbageAtIdle();
} else {
  collect();
}

function checkCache() {
  const value = cached.stack;
  assert.equal(value, reference.deref());
  assert.equal(value.marker, 'native cached value');
  assert.equal(value.bytes[65000], 91);
}

checkCache();
console.log(
  'PASS captureStackTrace real GC preserves lazy formatter frames and arbitrary cached object identity'
);
