/** Controlled option admission; actual Cocoa first-click callbacks have a separate native gate. */
import assert from 'node:assert/strict';
import create from '../../core/window/input.mjs';

const error = (code, message) => Object.assign(new Error(message), { code });

const unsupported = () => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', 'Native input');
};

let supported = true;
let calls = 0;
const read = create(
  {
    firstMouseAvailable() {
      calls++;
      return supported;
    }
  },
  error,
  unsupported
);
assert.equal(read({}), undefined);
assert.equal(read({ experimentalDarkMode: true }), undefined);
assert.equal(read({ experimentalDarkMode: false }), undefined);
assert.equal(calls, 0);
assert.equal(read({ acceptFirstMouse: true }), true);
assert.equal(read({ acceptFirstMouse: false }), false);
assert.equal(calls, 2);
console.log('PASS input defaults legacy flag and both native first-click policies');
for (const value of [null, 0, 1, '', 'true', [], {}, NaN]) {
  assert.throws(() => read({ experimentalDarkMode: value }), { code: 'ERR_INVALID_ARG_TYPE' });

  assert.throws(() => read({ acceptFirstMouse: value }), { code: 'ERR_INVALID_ARG_TYPE' });
}
assert.equal(calls, 2);
let reads = 0;
assert.equal(
  read({
    get acceptFirstMouse() {
      reads++;
      return true;
    }
  }),
  true
);
assert.equal(reads, 1);
console.log('PASS input rejects invalid values before native admission and reads accessors once');
supported = false;
for (const value of [true, false]) {
  assert.throws(() => read({ acceptFirstMouse: value }), { code: 'ERR_FIREFLY_UNSUPPORTED_API' });

  assert.throws(() => create({}, error, unsupported)({ acceptFirstMouse: value }), {
    code: 'ERR_FIREFLY_UNSUPPORTED_API'
  });
}
assert.equal(read({ experimentalDarkMode: true }), undefined);
console.log('PASS input unavailable backends fail explicitly without inventing native support');
console.log('DONE input policy');
