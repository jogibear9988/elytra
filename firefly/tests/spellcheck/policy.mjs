/** Controlled provider-state admission, using the real shared window preference validator. */
import assert from 'node:assert/strict';
import paths from 'node:path';
import create from '../../core/window/preferences.mjs';

const error = (code, message) => Object.assign(new Error(message), { code });

const unsupported = (message) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', message);
};

const fails = (action, code = 'ERR_FIREFLY_UNSUPPORTED_API') => assert.throws(action, { code });

let calls = 0;
const read = create(
  paths,
  () => false,
  error,
  unsupported,
  () => {
    ++calls;
    return true;
  }
);
assert.deepEqual(read(), { preload: undefined, additionalArguments: [] });
assert.deepEqual(read({ contextIsolation: true, nodeIntegration: false, sandbox: true }), {
  preload: undefined,
  additionalArguments: []
});
assert.equal(calls, 0);
const disabled = Object.freeze({ spellcheck: false });
assert.deepEqual(read(disabled), { preload: undefined, additionalArguments: [] });
assert.equal(disabled.spellcheck, false);
assert.equal(calls, 1);

fails(() => read({ spellcheck: true }));

assert.equal(calls, 1);
console.log(
  'PASS spellcheck defaults avoid native query and explicit false requires confirmed disabled provider'
);

for (const value of [undefined, null, 0, 1, '', 'false', [], {}, NaN]) {
  fails(() => read({ spellcheck: value }), 'ERR_INVALID_ARG_TYPE');
}
for (const value of [null, [], false, 1, 'preferences']) {
  fails(() => read(value), 'ERR_INVALID_ARG_TYPE');
}
assert.equal(calls, 1);
for (const value of [undefined, null, false, 0, 1, 'true', [], {}, new Boolean(true)]) {
  let queries = 0;
  const query = create(
    paths,
    () => false,
    error,
    unsupported,
    () => {
      ++queries;
      return value;
    }
  );

  fails(() => query({ spellcheck: false }));

  assert.equal(queries, 1);
}
for (const missing of [undefined, null, true, 17, {}]) {
  const query = create(paths, () => false, error, unsupported, missing);

  fails(() => query({ spellcheck: false }));

  assert.deepEqual(query({}), { preload: undefined, additionalArguments: [] });
}
console.log(
  'PASS spellcheck invalid types unavailable query and nonboolean provider states fail closed'
);

let reads = 0;
let queries = 0;
const snapshot = create(
  paths,
  () => false,
  error,
  unsupported,
  () => {
    ++queries;
    return true;
  }
);
const preferences = {
  get spellcheck() {
    return ++reads === 1 ? false : true;
  }
};
assert.deepEqual(snapshot(preferences), { preload: undefined, additionalArguments: [] });
assert.equal(reads, 1);
assert.equal(queries, 1);
const marker = new Error('controlled spellcheck accessor failure');

assert.throws(
  () =>
    snapshot({
      get spellcheck() {
        throw marker;
      }
    }),
  (failure) => failure === marker
);

assert.equal(queries, 1);
const nativeFailure = create(
  paths,
  () => false,
  error,
  unsupported,
  () => {
    throw marker;
  }
);

assert.throws(
  () => nativeFailure({ spellcheck: false }),
  (failure) => failure === marker
);

console.log(
  'PASS spellcheck accessor snapshots once and genuine query errors preserve failure identity'
);

const preload = paths.resolve('owned-preload.js');
const argumentsValue = Object.freeze(['--owned-config=1', 'owned argument with spaces']);
const secure = Object.freeze({
  contextIsolation: true,
  nodeIntegration: false,
  sandbox: true,
  preload,
  additionalArguments: argumentsValue,
  spellcheck: false
});
assert.deepEqual(read(secure), { preload, additionalArguments: argumentsValue });
for (const preference of [
  { contextIsolation: false },
  { nodeIntegration: true },
  { sandbox: false },
  { v8CacheOptions: 'none' }
]) {
  fails(() => read({ spellcheck: false, ...preference }));
}
const enforced = create(
  paths,
  () => true,
  error,
  unsupported,
  () => true
);
const sandbox = Object.freeze({ spellcheck: false, sandbox: false });
assert.deepEqual(enforced(sandbox), { preload: undefined, additionalArguments: [] });
assert.equal(sandbox.sandbox, false);

fails(() => read({ spellcheck: false, preload: false }), 'ERR_INVALID_ARG_TYPE');

fails(() => read({ spellcheck: false, preload: 'relative.js' }), 'ERR_INVALID_ARG_VALUE');

fails(() => read({ spellcheck: false, preload: paths.resolve('owned-preload.mjs') }));

for (const value of [null, 'arguments', [1]]) {
  fails(() => read({ spellcheck: false, additionalArguments: value }), 'ERR_INVALID_ARG_TYPE');
}

fails(
  () => read({ spellcheck: false, additionalArguments: Array(65).fill('argument') }),
  'ERR_INVALID_ARG_VALUE'
);

console.log(
  'PASS spellcheck admission preserves existing isolation sandbox preload and argument validation'
);
