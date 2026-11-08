/** Known unsupported modern comparison state must not produce a false green assertion. */
import assert from 'node:assert';

function refuses(callback) {
  let error;
  try {
    callback();
  } catch (caught) {
    error = caught;
  }
  if (error?.code !== 'ERR_MITE_UNSUPPORTED_API') {
    throw Error('Expected explicit limitation: ' + error);
  }
}

for (const [left, right] of [
  [new WeakMap(), new WeakMap()],
  [Promise.resolve(1), Promise.resolve(2)],
  [new Date(NaN), new Date(NaN)],
  [Object(1n), Object(2n)],
  [new Error('same', { cause: 1 }), new Error('same', { cause: 2 })],
  [new Float64Array([1]), new Float64Array([2])],
  [
    {
      get value() {
        return 1;
      }
    },
    { value: 1 }
  ],
  [Object.assign(/a/g, { lastIndex: 1 }), /a/g]
]) {
  refuses(() => assert.deepStrictEqual(left, right));

  refuses(() => assert.notDeepStrictEqual(left, right));
}
for (const name of ['Assert', 'CallTracker', 'partialDeepStrictEqual']) {
  refuses(() => assert[name]());
}

refuses(() => assert.fail(1, 2));

refuses(() =>
  assert.throws(
    () => {
      throw { value: Promise.resolve(1) };
    },
    { value: Promise.resolve(2) }
  )
);

const deep = {};
let next = deep;
for (let i = 0; i < 260; i++) {
  next.value = {};
  next = next.value;
}

refuses(() => assert.deepStrictEqual(deep, { value: 1 }));

console.log('PASS explicit assert comparison/overload limitations');
let thenableError;
try {
  await assert.rejects({ then() {}, catch() {} });
} catch (error) {
  thenableError = error;
}
if (thenableError?.code !== 'ERR_MITE_UNSUPPORTED_API') {
  throw Error('Unknown thenable accepted');
}
