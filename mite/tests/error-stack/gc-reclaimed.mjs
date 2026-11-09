/** Embedded-only deterministic idle collection verifies native capture records do not leak owners. */
import assert from 'node:assert/strict';

assert.equal(typeof collectHostGarbageAtIdle, 'function');

function install() {
  const target = { name: 'Ephemeral', message: 'capture record owner' };
  const cache = { bytes: new Uint8Array(65536), marker: 'capture cached payload' };
  Error.captureStackTrace(target);
  target.stack = cache;
  return { target: new WeakRef(target), cache: new WeakRef(cache) };
}

const installed = install();
for (let attempt = 0; attempt < 12; ++attempt) {
  await new Promise((resolve) => setTimeout(resolve, 5));
  await collectHostGarbageAtIdle();
}
assert.equal(
  installed.target.deref(),
  undefined,
  'Unreachable captured object is not a permanent native root'
);
assert.equal(
  installed.cache.deref(),
  undefined,
  'Unreachable capture cache retires with its owner'
);
console.log(
  'PASS embedded captureStackTrace unreachable native record target and cache reclaim after idle GC'
);
