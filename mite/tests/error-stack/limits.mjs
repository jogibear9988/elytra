/** This initial Mite contract limits captured stacks; native Error instances remain engine-owned. */
import assert from 'node:assert/strict';
import { frames, stackCaptureTop } from './helpers.mjs';

const original = Object.getOwnPropertyDescriptor(Error, 'stackTraceLimit');
const originalPrototype = Object.getPrototypeOf(Error);
assert.equal(original.value, 10);
assert.equal(original.enumerable, true);
assert.equal(original.writable, true);
assert.equal(original.configurable, true);

function deepCapture(depth) {
  if (depth > 0) {
    return deepCapture(depth - 1);
  }
  const target = {};
  Error.captureStackTrace(target);
  return target;
}

try {
  assert.equal(frames(deepCapture(20).stack).length, 10);
  for (const [limit, expected] of [
    [0, 0],
    [1, 1],
    [2, 2],
    [-1, 0],
    [1.9, 1],
    [NaN, 0]
  ]) {
    Error.stackTraceLimit = limit;
    assert.equal(frames(stackCaptureTop({}).stack).length, expected);
  }
  Error.stackTraceLimit = Infinity;
  assert.ok(frames(deepCapture(20).stack).length >= 21);
  Error.stackTraceLimit = 1;
  const snapshot = deepCapture(20);
  Error.stackTraceLimit = 5;
  assert.equal(frames(snapshot.stack).length, 1);
  for (const limit of [undefined, '2', null]) {
    Error.stackTraceLimit = limit;
    const target = {};
    assert.equal(Error.captureStackTrace(target), undefined);
    assert.equal(target.stack, undefined);
    assert.equal(typeof Object.getOwnPropertyDescriptor(target, 'stack').get, 'function');
    target.stack = 'manual value with disabled frame capture';
    assert.equal(target.stack, 'manual value with disabled frame capture');
  }
  let getterCalls = 0;
  const forbidden = new Error('stackTraceLimit getter must not run');
  for (const throws of [false, true]) {
    Object.defineProperty(Error, 'stackTraceLimit', {
      configurable: true,

      get() {
        ++getterCalls;
        if (throws) {
          throw forbidden;
        }
        return 2;
      }
    });
    const target = deepCapture(20);
    assert.equal(target.stack, undefined);
    assert.equal(typeof Object.getOwnPropertyDescriptor(target, 'stack').get, 'function');
    assert.equal(getterCalls, 0);
  }
  const inherited = Object.create(originalPrototype);
  Object.defineProperty(inherited, 'stackTraceLimit', { value: 2, configurable: true });
  delete Error.stackTraceLimit;
  Object.setPrototypeOf(Error, inherited);
  assert.equal(frames(deepCapture(20).stack).length, 2);
  Object.defineProperty(inherited, 'stackTraceLimit', {
    configurable: true,

    get() {
      ++getterCalls;
      throw forbidden;
    }
  });
  assert.equal(deepCapture(20).stack, undefined);
  assert.equal(getterCalls, 0);
} finally {
  Object.setPrototypeOf(Error, originalPrototype);
  Object.defineProperty(Error, 'stackTraceLimit', original);
}
console.log(
  'PASS captureStackTrace default limit numeric coercion disabled capture and limit snapshot'
);
