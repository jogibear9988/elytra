/** Invalid targets fail before mutation or Proxy hooks, while ordinary objects retain identity. */
import assert from 'node:assert/strict';

for (const target of [undefined, null, true, 1, 'text', Symbol('target'), 2n]) {
  assert.throws(() => Error.captureStackTrace(target), TypeError);
}
let traps = 0;
const proxied = {};
const proxy = new Proxy(proxied, {
  get() {
    ++traps;
    throw new Error('Proxy get trap ran');
  },

  defineProperty() {
    ++traps;
    throw new Error('Proxy define trap ran');
  },

  getOwnPropertyDescriptor() {
    ++traps;
    throw new Error('Proxy descriptor trap ran');
  },

  isExtensible() {
    ++traps;
    throw new Error('Proxy extensibility trap ran');
  }
});

assert.throws(() => Error.captureStackTrace(proxy), TypeError);

assert.equal(traps, 0);
assert.equal(Object.hasOwn(proxied, 'stack'), false);
for (const make of [Object.freeze, Object.seal, Object.preventExtensions]) {
  const target = make({});

  assert.throws(() => Error.captureStackTrace(target), TypeError);

  assert.equal(Object.hasOwn(target, 'stack'), false);
}
for (const writable of [true, false]) {
  const target = Object.defineProperty({}, 'stack', {
    value: 'untouched',
    writable,
    configurable: false
  });

  assert.throws(() => Error.captureStackTrace(target), TypeError);

  assert.equal(target.stack, 'untouched');
}
console.log(
  'PASS captureStackTrace strict targets immutable descriptors and Proxy refusal without traps'
);

const objects = [{}, [], Object.create(null), function CaptureTarget() {}];
for (const target of objects) {
  const before = Object.getPrototypeOf(target);
  assert.equal(Error.captureStackTrace(target), undefined);
  assert.equal(Object.getPrototypeOf(target), before);
  assert.equal(typeof target.stack, 'string');
}
let gets = 0;
const configurable = Object.defineProperty({}, 'stack', {
  get() {
    ++gets;
    throw new Error('Old stack getter invoked');
  },

  configurable: true,
  enumerable: true
});
Error.captureStackTrace(configurable);
assert.equal(gets, 0);
assert.equal(Object.getOwnPropertyDescriptor(configurable, 'stack').enumerable, false);
assert.equal(typeof configurable.stack, 'string');
assert.equal(gets, 0);
console.log(
  'PASS captureStackTrace ordinary target kinds and configurable replacement preserve object state'
);
