/** Generic accessor receivers resolve their own captured record, including prototype holders. */
import assert from 'node:assert/strict';
import { stackCaptureTop } from './helpers.mjs';

const first = stackCaptureTop({ name: 'First', message: 'one' });
const second = stackCaptureTop({ name: 'Second', message: 'two' });
const descriptor = Object.getOwnPropertyDescriptor(first, 'stack');
assert.equal(descriptor.get.call(second).split('\n')[0], 'Second: two');
assert.equal(first.stack.split('\n')[0], 'First: one');
const assigned = { marker: 'other record cache' };
assert.equal(descriptor.set.call(second, assigned), undefined);
assert.equal(second.stack, assigned);
assert.equal(first.stack.split('\n')[0], 'First: one');
const holder = stackCaptureTop({ name: 'Holder', message: 'prototype' });
const inherited = Object.create(holder);
inherited.name = 'Child';
inherited.message = 'own fields must not replace holder header';
assert.equal(descriptor.get.call(inherited).split('\n')[0], 'Holder: prototype');
assert.equal(descriptor.set.call(inherited, assigned), undefined);
assert.equal(holder.stack, assigned);
assert.equal(inherited.stack, assigned);
assert.equal(Object.hasOwn(inherited, 'stack'), false);
console.log(
  'PASS captureStackTrace borrowed accessors select receiver records and inherited capture holders'
);

let traps = 0;
const proxy = new Proxy(first, {
  get() {
    ++traps;
    throw new Error('Proxy get receiver trap');
  },

  getPrototypeOf() {
    ++traps;
    throw new Error('Proxy prototype receiver trap');
  }
});
const plain = { stack: 'ordinary data must remain unchanged' };
for (const receiver of [undefined, null, true, 1, 'text', Symbol('receiver'), plain, proxy]) {
  assert.equal(descriptor.get.call(receiver), undefined);
  assert.equal(descriptor.set.call(receiver, assigned), undefined);
}
assert.equal(plain.stack, 'ordinary data must remain unchanged');
assert.equal(traps, 0);
const nativeError = new Error('real engine error');
assert.equal(descriptor.get.call(nativeError), nativeError.stack);
assert.equal(descriptor.set.call(nativeError, 'assigned native stack'), undefined);
assert.equal(nativeError.stack, 'assigned native stack');
console.log(
  'PASS captureStackTrace primitive plain and proxy receiver noops and genuine native Error delegation'
);
