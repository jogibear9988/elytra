/** Public descriptors, lazy generic headers and setter cache behavior. */
import assert from 'node:assert/strict';
import { assertOrder, stackCaptureTop } from './helpers.mjs';

assert.equal(typeof Error.captureStackTrace, 'function');
assert.equal(Error.captureStackTrace.name, 'captureStackTrace');
assert.equal(Error.captureStackTrace.length, 2);
const api = Object.getOwnPropertyDescriptor(Error, 'captureStackTrace');
assert.equal(api.enumerable, false);
assert.equal(api.writable, true);
assert.equal(api.configurable, true);
const target = { name: 'Captured', message: 'original' };
const identity = target;
assert.equal(Error.captureStackTrace(target), undefined);
assert.equal(target, identity);
const descriptor = Object.getOwnPropertyDescriptor(target, 'stack');
assert.equal(descriptor.enumerable, false);
assert.equal(descriptor.configurable, true);
assert.equal(typeof descriptor.get, 'function');
assert.equal(typeof descriptor.set, 'function');
assert.equal(Object.hasOwn(descriptor, 'value'), false);
assert.deepEqual(Object.keys(target), ['name', 'message']);
console.log('PASS captureStackTrace public API and nonenumerable configurable stack accessor');

let names = 0;
let messages = 0;
let currentName = 'First';
let currentMessage = 'before';
const lazy = {
  get name() {
    ++names;
    return currentName;
  },

  get message() {
    ++messages;
    return currentMessage;
  }
};
stackCaptureTop(lazy);
const lazyDescriptor = Object.getOwnPropertyDescriptor(lazy, 'stack');
assert.equal(names, 0);
assert.equal(messages, 0);
currentName = 'Changed';
currentMessage = 'after';
const first = lazy.stack;
assert.equal(first.split('\n')[0], 'Changed: after');
assertOrder(first, ['stackCaptureLeaf', 'stackCaptureMiddle', 'stackCaptureTop']);
assert.equal(names, 1);
assert.equal(messages, 1);
currentName = 'Later';
currentMessage = 'later';
assert.equal(lazy.stack, first);
assert.equal(names, 1);
assert.equal(messages, 1);
const custom = { manual: 'cached value' };
lazy.stack = custom;
assert.equal(lazy.stack, custom);
assert.equal(Object.getOwnPropertyDescriptor(lazy, 'stack').get, lazyDescriptor.get);
assert.equal(Object.getOwnPropertyDescriptor(lazy, 'stack').set, lazyDescriptor.set);
console.log(
  'PASS captureStackTrace lazy changed header cached formatting and arbitrary setter identity'
);

for (const operation of ['freeze', 'seal', 'preventExtensions']) {
  const object = { name: 'Frozen', message: operation };
  stackCaptureTop(object);
  Object[operation](object);
  assert.equal(object.stack.split('\n')[0], `Frozen: ${operation}`);
  object.stack = custom;
  assert.equal(object.stack, custom);
}
let getterCalls = 0;
const failure = new Error('controlled-header-getter');
const retry = {
  get name() {
    if (++getterCalls === 1) {
      throw failure;
    }
    return 'Retry';
  },

  message: 'works'
};
Error.captureStackTrace(retry);
assert.equal(getterCalls, 0);

assert.throws(
  () => retry.stack,
  (error) => error === failure
);

assert.equal(retry.stack.split('\n')[0], 'Retry: works');
assert.equal(getterCalls, 2);
console.log('PASS captureStackTrace frozen accessor setters and failed lazy header retry');
