/** Genuine lazy application formatters receive captured LibJS/Node CallSite records. */
import assert from 'node:assert/strict';
import { stackCaptureTop } from './helpers.mjs';

const original = Object.getOwnPropertyDescriptor(Error, 'prepareStackTrace');
try {
  const cached = stackCaptureTop({ name: 'Cached', message: 'ready' });
  const cachedValue = cached.stack;
  const manual = stackCaptureTop({});
  const manualValue = { opaque: 'assigned before hook' };
  manual.stack = manualValue;
  let calls = 0;

  Error.prepareStackTrace = function (target, sites) {
    assert.equal(this, Error);
    ++calls;
    assert.ok(Array.isArray(sites));
    return { target, sites };
  };

  const target = stackCaptureTop({ name: 'Prepared', message: 'lazy' });
  assert.equal(calls, 0);
  assert.equal(cached.stack, cachedValue);
  assert.equal(manual.stack, manualValue);
  assert.equal(calls, 0);
  const prepared = target.stack;
  assert.equal(calls, 1);
  assert.equal(prepared.target, target);
  assert.equal(prepared.sites[0].getFunctionName(), 'stackCaptureLeaf');
  assert.equal(target.stack, prepared);
  assert.equal(calls, 1);
  console.log(
    'PASS captureStackTrace genuine lazy prepare hook receiver CallSites and arbitrary cached return'
  );

  const failure = new Error('controlled formatter failure');
  calls = 0;

  Error.prepareStackTrace = function (_target, sites) {
    assert.equal(this, Error);
    if (++calls === 1) {
      throw failure;
    }
    return sites;
  };

  const retry = stackCaptureTop({
    get name() {
      throw new Error('Custom formatter must not read the default header');
    },

    get message() {
      throw new Error('Custom formatter must not read the default message');
    }
  });

  assert.throws(
    () => retry.stack,
    (error) => error === failure
  );

  assert.equal(calls, 1);
  const retried = retry.stack;
  assert.ok(Array.isArray(retried));
  assert.equal(retried[0].getFunctionName(), 'stackCaptureLeaf');
  assert.equal(retry.stack, retried);
  assert.equal(calls, 2);
  console.log(
    'PASS captureStackTrace formatter failure retries without default header reads or partial caching'
  );

  Error.prepareStackTrace = 17;
  assert.equal(typeof stackCaptureTop({}).stack, 'string');
  let getterCalls = 0;
  const getterFailure = new Error('controlled prepare accessor failure');
  Object.defineProperty(Error, 'prepareStackTrace', {
    configurable: true,

    get() {
      ++getterCalls;
      throw getterFailure;
    }
  });
  const throwing = stackCaptureTop({ name: 'GetterRetry', message: 'after failure' });
  assert.equal(getterCalls, 0);
  assert.equal(cached.stack, cachedValue);
  assert.equal(manual.stack, manualValue);
  assert.equal(getterCalls, 0);

  assert.throws(
    () => throwing.stack,
    (error) => error === getterFailure
  );

  assert.equal(getterCalls, 1);
  delete Error.prepareStackTrace;
  assert.equal(throwing.stack.split('\n')[0], 'GetterRetry: after failure');
  getterCalls = 0;
  let functionCalls = 0;
  Object.defineProperty(Error, 'prepareStackTrace', {
    configurable: true,

    get() {
      ++getterCalls;
      return function (actual, sites) {
        assert.equal(this, Error);
        ++functionCalls;
        return { actual, sites };
      };
    }
  });
  const accessor = stackCaptureTop({});
  assert.equal(getterCalls, 0);
  const formatted = accessor.stack;
  assert.equal(formatted.actual, accessor);
  assert.ok(Array.isArray(formatted.sites));
  assert.equal(functionCalls, 1);
  assert.ok(getterCalls >= 1);
  assert.equal(accessor.stack, formatted);
  console.log(
    'PASS captureStackTrace noncallable and lazy accessor prepare hooks preserve errors and caching'
  );
} finally {
  if (original) {
    Object.defineProperty(Error, 'prepareStackTrace', original);
  } else {
    delete Error.prepareStackTrace;
  }
}
