/** The bindings package's filename-discovery pattern must work without leaving a poisoned hook. */
import assert from 'node:assert/strict';
import { discoverCallerFile } from './bindings-helper.mjs';
import { stackCaptureTop } from './helpers.mjs';

const original = Object.getOwnPropertyDescriptor(Error, 'prepareStackTrace');

function actualPackageCaller() {
  return discoverCallerFile();
}

const filename = actualPackageCaller();
assert.ok(
  filename.endsWith('/bindings.mjs'),
  `Expected actual package caller source, got ${filename}`
);
assert.deepEqual(Object.getOwnPropertyDescriptor(Error, 'prepareStackTrace'), original);
assert.equal(typeof stackCaptureTop({}).stack, 'string');
console.log(
  'PASS captureStackTrace bindings-style package discovery uses actual caller filename and restores formatter'
);
