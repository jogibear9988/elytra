/** Embedded-only: reported promises are weak bookkeeping, not permanent native roots. */
import assert from 'node:assert/strict';

assert.equal(typeof globalThis.collectHostGarbage, 'function');
assert.equal(typeof globalThis.collectHostGarbageAtIdle, 'function');

let finish;
const delivered = new Promise((resolve) => (finish = resolve));
let notices = 0;
const deadline = setTimeout(() => {
  throw new Error('Reported rejection reclamation timed out');
}, 4000);

process.once('unhandledRejection', (reason, promise) => {
  ++notices;
  collectHostGarbage();
  assert.equal(promise, installed.promise.deref());
  assert.equal(reason, installed.reason.deref());
  assert.equal(reason.marker, 'ephemeral rejection payload');
  finish();
});

function install() {
  const reason = { marker: 'ephemeral rejection payload', bytes: new Uint8Array(65536) };
  const promise = Promise.reject(reason);
  // No application strong references to either value escape this helper frame.
  return { promise: new WeakRef(promise), reason: new WeakRef(reason) };
}

const installed = install();
await delivered;
for (let attempt = 0; attempt < 12; ++attempt) {
  await new Promise((resolve) => setTimeout(resolve, 5));
  // Native collection occurs after callback/Promise frames and kept objects unwind.
  // Do not dereference in between: WeakRef inspection would itself extend lifetime.
  await collectHostGarbageAtIdle();
}
assert.equal(notices, 1);
assert.equal(installed.promise.deref(), undefined, 'Reported native promise record must be weak');
assert.equal(installed.reason.deref(), undefined, 'Reported reason retires with its promise');
clearTimeout(deadline);
console.log(
  'PASS embedded process reported promise and ephemeral reason reclaim after notification'
);
