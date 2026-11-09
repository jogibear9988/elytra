/** Retained source records outlive their capture target and remain valid through real collection. */
import assert from 'node:assert/strict';
import { stackCaptureTop } from './helpers.mjs';

const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
assert.equal(typeof collect, 'function');
const original = Object.getOwnPropertyDescriptor(Error, 'prepareStackTrace');
let installed;
try {
  Error.prepareStackTrace = function (_target, sites) {
    collect();
    assert.equal(this, Error);
    assert.equal(sites[0].getFunctionName(), 'stackCaptureLeaf');
    return sites;
  };

  function install() {
    const target = stackCaptureTop({});
    return { sites: target.stack, target: new WeakRef(target) };
  }

  installed = install();
} finally {
  if (original) {
    Object.defineProperty(Error, 'prepareStackTrace', original);
  } else {
    delete Error.prepareStackTrace;
  }
}
for (let attempt = 0; attempt < 12; ++attempt) {
  await new Promise((resolve) => setTimeout(resolve, 5));
  if (typeof collectHostGarbageAtIdle === 'function') {
    await collectHostGarbageAtIdle();
  } else {
    collect();
  }
}
assert.equal(installed.sites[0].getFunctionName(), 'stackCaptureLeaf');
assert.ok(installed.sites[0].getFileName().endsWith('/helpers.mjs'));
assert.ok(installed.sites[0].getLineNumber() > 0);
assert.equal(installed.sites[0].getFunctionName.call(installed.sites[1]), 'stackCaptureMiddle');
console.log('PASS captureStackTrace retained CallSite source records survive real GC');
if (typeof collectHostGarbageAtIdle === 'function') {
  assert.equal(
    installed.target.deref(),
    undefined,
    'Retained CallSites do not pin their capture owner'
  );
  console.log('PASS embedded retained CallSites release unreachable capture owner after idle GC');
}
