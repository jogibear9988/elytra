/** Controlled admission and snapshot checks; native Qt enforcement is a separate component gate. */
import assert from 'node:assert/strict';
import create from '../../core/window/constraints.mjs';

const error = (code, message) => Object.assign(new Error(message), { code });

const unsupported = () => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', 'Native constraints');
};

let wire = '[0,0]';
const writes = [];
const native = {
  minimumSize(id) {
    assert.equal(id, 7);
    return wire;
  },

  maximumSize(id) {
    assert.equal(id, 7);
    return wire;
  },

  setMinimumSize(...args) {
    writes.push(['minimum', ...args]);
  },

  setMaximumSize(...args) {
    writes.push(['maximum', ...args]);
  }
};
const limits = create(native, error, unsupported);
assert.deepEqual(limits.initial({}), { minWidth: 0, minHeight: 0, maxWidth: 0, maxHeight: 0 });
assert.deepEqual(limits.initial({ minWidth: 700, maxWidth: 200 }), {
  minWidth: 700,
  minHeight: 0,
  maxWidth: 200,
  maxHeight: 0
});
console.log(
  'PASS constraints constructor defaults and requested conflicts remain native decisions'
);

limits.set(7, 400, 270, true);
limits.set(7, 16384, 0, false);
assert.deepEqual(writes, [
  ['minimum', 7, 400, 270],
  ['maximum', 7, 16384, 0]
]);
for (const value of [-1, 16385, Infinity, NaN, 1.2, '200', null, undefined]) {
  const code =
    typeof value === 'number' && Number.isInteger(value)
      ? 'ERR_OUT_OF_RANGE'
      : 'ERR_INVALID_ARG_TYPE';

  assert.throws(() => limits.set(7, value, 200, true), { code });

  assert.throws(() => limits.set(7, 200, value, false), { code });

  if (value !== undefined) {
    assert.throws(() => limits.initial({ minWidth: value }), { code });
  }
}
assert.equal(writes.length, 2);
console.log('PASS constraints strict integer range checks before every native mutation');

wire = '[400,270]';
const first = limits.get(7, true);
first[0] = -1;
assert.deepEqual(limits.get(7, true), [400, 270]);
wire = '[2147483647,16390]';
assert.deepEqual(limits.get(7, false), [2147483647, 16390]);
console.log('PASS constraints fresh native snapshots content frame expansion and unbounded values');

for (wire of ['', 'false', '{}', '[1]', '[1,2,3]', '[-1,2]', '[1.5,2]', '[2147483648,2]']) {
  assert.throws(() => limits.get(7, true), { code: 'ERR_FIREFLY_GEOMETRY_DATA' });
}
const missing = create({}, error, unsupported);

assert.throws(() => missing.get(7, true), { code: 'ERR_FIREFLY_UNSUPPORTED_API' });

assert.throws(() => missing.set(7, 1, 1, true), { code: 'ERR_FIREFLY_UNSUPPORTED_API' });

console.log('PASS constraints invalid native data and unavailable hosts fail explicitly');
console.log('DONE constraints policy');
