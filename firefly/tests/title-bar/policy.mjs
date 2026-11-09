/** Controlled constructor admission; native window style/buttons/layout need the separate Cocoa gate. */
import assert from 'node:assert/strict';
import create from '../../core/window/title-bar.mjs';

const error = (code, message) => Object.assign(new Error(message), { code });

const unsupported = () => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', 'Native title bar');
};

let supported = true;
let calls = 0;
const read = create(
  {
    titleBarAvailable() {
      calls++;
      return supported;
    }
  },
  error,
  unsupported
);
assert.equal(read(undefined), false);
assert.equal(read('default'), false);
assert.equal(calls, 0);
assert.equal(read('hidden'), true);
assert.equal(calls, 1);
console.log('PASS title bar defaults and hidden style require actual backend admission');
for (const value of [null, 0, 1, false, {}, [], NaN]) {
  assert.throws(() => read(value), { code: 'ERR_INVALID_ARG_TYPE' });
}
for (const value of ['', 'hiddenInset', 'customButtonsOnHover', 'Hidden', 'unsupported']) {
  assert.throws(() => read(value), { code: 'ERR_FIREFLY_UNSUPPORTED_API' });
}
assert.equal(calls, 1);
console.log('PASS title bar rejects invalid types and unsupported styles before native admission');
for (const value of [false, 0, 1, 'true', null, undefined]) {
  supported = value;

  assert.throws(() => read('hidden'), { code: 'ERR_FIREFLY_UNSUPPORTED_API' });

  assert.equal(read('default'), false);
}

assert.throws(() => create({}, error, unsupported)('hidden'), {
  code: 'ERR_FIREFLY_UNSUPPORTED_API'
});

console.log('PASS title bar missing unsupported and malformed backend capability fails explicitly');
console.log('DONE title bar policy');
