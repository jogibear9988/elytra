/** Real deprecation delivery, forwarding and construction: no warning suppression/no-op adapter. */
import assert from 'node:assert/strict';
import { deprecate } from 'node:util';
import process from 'node:process';
const warnings = [];

process.on('warning', (warning) => warnings.push(warning));

let calls = 0;
const receiver = { value: 4 };

function sum(a, b) {
  calls++;
  return this.value + a + b;
}

const first = deprecate(sum, 'owned first', 'OWNED_FIRST');
assert.equal(first.call(receiver, 1, 2), 7);
assert.equal(first.apply(receiver, [2, 3]), 9);
assert.equal(calls, 2);
assert.equal(warnings.length, 0);
const sameCode = deprecate(() => 11, 'owned duplicate', 'OWNED_FIRST');
assert.equal(sameCode(), 11);
const a = deprecate(() => 1, 'owned uncoded a');
a();
a();
deprecate(() => 2, 'owned uncoded b')();
assert.equal(Object.getPrototypeOf(first), sum);
assert.equal(first.length, sum.length);
assert.equal(first.name, 'deprecated');

function Constructor(value) {
  this.value = value;
  this.target = new.target;
}

Constructor.staticValue = 'inherited';
const Deprecated = deprecate(Constructor, 'owned constructor', 'OWNED_CTOR');
assert.equal(Deprecated.prototype, Constructor.prototype);
assert.equal(Deprecated.staticValue, 'inherited');
const constructed = new Deprecated(23);
assert.equal(constructed.value, 23);
assert.equal(constructed.target, Deprecated);
assert.ok(constructed instanceof Constructor);
assert.ok(new Constructor(2) instanceof Deprecated);
class Derived extends Deprecated {}
assert.equal(new Derived(5).target, Derived);
const plain = deprecate(sum, 'owned no prototype', 'OWNED_NOPROTO', { modifyPrototype: false });
assert.equal(Object.getPrototypeOf(plain), Function.prototype);
assert.equal(plain.length, 0);
assert.equal(plain.call(receiver, 1, 1), 6);
process.noDeprecation = true;
const delayed = deprecate(() => 17, 'owned delayed', 'OWNED_DELAYED');
assert.equal(delayed(), 17);
process.noDeprecation = false;
assert.equal(delayed(), 17);
const exact = new Error('owned exact');
exact.name = 'OwnedErrorWarning';
process.emitWarning(exact);
process.emitWarning('owned explicit', {
  type: 'OwnedWarning',
  code: 'OWNED_OPTIONS',
  detail: 'owned detail'
});
let calledBeforeThrow = false;
process.throwDeprecation = true;
const thrown = new Promise((resolve) => process.once('uncaughtException', resolve));
assert.equal(
  deprecate(
    () => {
      calledBeforeThrow = true;
      return 31;
    },
    'owned thrown',
    'OWNED_THROW'
  )(),
  31
);
assert.equal(calledBeforeThrow, true);
assert.equal(warnings.length, 0);
const error = await thrown;
process.throwDeprecation = false;
assert.equal(error.name, 'DeprecationWarning');
assert.equal(error.code, 'OWNED_THROW');
await new Promise((resolve) => setImmediate(resolve));
assert.deepEqual(
  warnings.map((warning) => [warning.name, warning.code ?? null, warning.message]),
  [
    ['DeprecationWarning', 'OWNED_FIRST', 'owned first'],
    ['DeprecationWarning', null, 'owned uncoded a'],
    ['DeprecationWarning', null, 'owned uncoded b'],
    ['DeprecationWarning', 'OWNED_CTOR', 'owned constructor'],
    ['DeprecationWarning', 'OWNED_NOPROTO', 'owned no prototype'],
    ['DeprecationWarning', 'OWNED_DELAYED', 'owned delayed'],
    ['OwnedErrorWarning', null, 'owned exact'],
    ['OwnedWarning', 'OWNED_OPTIONS', 'owned explicit']
  ]
);
assert.equal(warnings[6], exact);
assert.equal(warnings[7].detail, 'owned detail');

assert.throws(() => deprecate(() => {}, 'msg', 1), { code: 'ERR_INVALID_ARG_TYPE' });

assert.throws(() => process.emitWarning(1), { code: 'ERR_INVALID_ARG_TYPE' });

assert.throws(() => process.emitWarning('msg', { code: 1 }), { code: 'ERR_INVALID_ARG_TYPE' });

console.log(
  'PASS deprecate once-per-code/wrapper, real async warnings, receiver/args/return and constructor identity'
);
console.log(
  'PASS noDeprecation dynamic suppression and asynchronous throwDeprecation preserve wrapped calls'
);
