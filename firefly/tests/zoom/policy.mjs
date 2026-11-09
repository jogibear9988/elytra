/** Real shared zoom validation; controlled tests do not claim native renderer zoom acceptance. */
import assert from 'node:assert/strict';
import paths from 'node:path';
import createZoomPolicy from '../../core/window/zoom.mjs';
import createPreferences from '../../core/window/preferences.mjs';

const zoom = createZoomPolicy();

const invalidType = (action) =>
  assert.throws(
    action,
    (failure) => failure instanceof TypeError && failure.code === 'ERR_INVALID_ARG_TYPE'
  );

const invalidRange = (action) =>
  assert.throws(
    action,
    (failure) => failure instanceof RangeError && failure.code === 'ERR_OUT_OF_RANGE'
  );

for (const value of [0.3, 0.5, 1, 1.25, 4.999999, 5]) {
  assert.equal(zoom.factor(value), value);
}
for (const value of [-0, 0, -1, 0.2999999, 5.000001, NaN, Infinity, -Infinity]) {
  invalidRange(() => zoom.factor(value));
}
let coercions = 0;
const hostile = {
  valueOf() {
    ++coercions;
    throw new Error('Zoom value coercion ran');
  },

  [Symbol.toPrimitive]() {
    ++coercions;
    throw new Error('Zoom primitive coercion ran');
  }
};
for (const value of [
  undefined,
  null,
  true,
  false,
  '1',
  [],
  {},
  new Number(1),
  1n,
  Symbol('zoom'),
  () => 1,
  hostile
]) {
  invalidType(() => zoom.factor(value));

  invalidType(() => zoom.level(value));
}
assert.equal(coercions, 0);
console.log(
  'PASS zoom factors enforce inclusive native range and primitive numeric types without coercion'
);

for (const level of [-6, -1.5, -1, 0, 0.5, 1, 2, 5]) {
  const factor = zoom.level(level);
  const expected = 1.2 ** level;
  assert.ok(Math.abs(factor - expected) <= expected * 1e-12);
  assert.equal(zoom.factor(factor), factor);
}
for (const level of [-10, 10, NaN, Infinity, -Infinity, Number.MAX_VALUE, -Number.MAX_VALUE]) {
  invalidRange(() => zoom.level(level));
}
console.log(
  'PASS zoom levels map genuine numeric exponents to validated factors and reject overflow bounds'
);

const error = (code, message) => Object.assign(new Error(message), { code });

const unsupported = (message) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', message);
};

let queries = 0;
const read = createPreferences(
  paths,
  () => false,
  error,
  unsupported,
  () => {
    ++queries;
    return true;
  }
);
assert.deepEqual(read(), { preload: undefined, additionalArguments: [], zoomFactor: 1 });
assert.equal(read({ zoomFactor: 0.3 }).zoomFactor, 0.3);
assert.equal(read({ zoomFactor: 5 }).zoomFactor, 5);
assert.equal(queries, 0);
let reads = 0;
const source = {
  get zoomFactor() {
    return ++reads === 1 ? 1.5 : NaN;
  }
};
assert.equal(read(source).zoomFactor, 1.5);
assert.equal(reads, 1);
assert.equal(queries, 0);
const marker = new Error('controlled zoom accessor failure');

assert.throws(
  () =>
    read({
      get zoomFactor() {
        throw marker;
      }
    }),
  (failure) => failure === marker
);

for (const value of [undefined, null, true, '1', [], {}, hostile]) {
  invalidType(() => read({ zoomFactor: value }));
}
for (const value of [0, 0.2, 6, NaN, Infinity]) {
  invalidRange(() => read({ zoomFactor: value }));
}
assert.equal(queries, 0);
assert.equal(coercions, 0);
console.log(
  'PASS zoom preferences default to one snapshot accessors once and never query native providers'
);

const preload = paths.resolve('owned-zoom-preload.js');
const additionalArguments = Object.freeze(['--owned-config=1']);
const preferences = Object.freeze({
  zoomFactor: 1.25,
  contextIsolation: true,
  nodeIntegration: false,
  sandbox: true,
  spellcheck: false,
  preload,
  additionalArguments
});
assert.deepEqual(read(preferences), { preload, additionalArguments, zoomFactor: 1.25 });
assert.equal(queries, 1);
assert.equal(preferences.zoomFactor, 1.25);
for (const security of [
  { contextIsolation: false },
  { nodeIntegration: true },
  { sandbox: false },
  { v8CacheOptions: 'none' }
]) {
  assert.throws(() => read({ zoomFactor: 1.25, ...security }), {
    code: 'ERR_FIREFLY_UNSUPPORTED_API'
  });
}

assert.throws(() => read({ zoomFactor: 1.25, preload: 'relative.js' }), {
  code: 'ERR_INVALID_ARG_VALUE'
});

assert.throws(() => read({ zoomFactor: 1.25, additionalArguments: [1] }), {
  code: 'ERR_INVALID_ARG_TYPE'
});

console.log(
  'PASS zoom admission preserves shared isolation spellcheck preload and argument policies'
);
