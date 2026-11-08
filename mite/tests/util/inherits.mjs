/** Compare actual legacy inheritance, descriptors and invalid-target side effects with Node. */
import assert from 'node:assert/strict';
import util, { inherits } from 'node:util';
import { EventEmitter } from 'node:events';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
assert.equal(inherits, util.inherits);
assert.equal(inherits, require('util').inherits);

function Parent() {
  this.parent = true;
}

Parent.staticValue = 1;

Parent.prototype.method = function () {
  return 42;
};

function Child() {
  Parent.call(this);
}

const oldPrototype = Child.prototype;
Child.prototype.own = true;
assert.equal(inherits(Child, Parent), undefined);
const child = new Child();
assert.equal(child.method(), 42);
assert.equal(child.parent, true);
assert.equal(child.own, true);
assert.equal(child.constructor, Child);
assert.equal(child instanceof Parent, true);
assert.equal(Child.prototype, oldPrototype);
assert.equal(Child.staticValue, undefined);
assert.equal(Object.getPrototypeOf(Child), Function.prototype);
const descriptor = Object.getOwnPropertyDescriptor(Child, 'super_');
assert.equal(descriptor.value, Parent);
assert.equal(descriptor.writable, true);
assert.equal(descriptor.configurable, true);
assert.equal(descriptor.enumerable, false);

function LegacyEmitter() {
  EventEmitter.call(this);
}

inherits(LegacyEmitter, EventEmitter);
const emitter = new LegacyEmitter();
let delivered = 0;

emitter.on('message', (value) => (delivered += value));

emitter.emit('message', 7);
assert.equal(delivered, 7);
const objectCtor = { prototype: {} };
inherits(objectCtor, { prototype: null });
assert.equal(Object.getPrototypeOf(objectCtor.prototype), null);
console.log(
  'PASS inherits module identity, instance methods, preserved prototype/static parent and real EventEmitter'
);

for (const [ctor, parent] of [
  [undefined, Parent],
  [null, Parent],
  [Child, undefined],
  [Child, null],
  [Child, {}],
  [Child, () => {}]
]) {
  assert.throws(() => inherits(ctor, parent), { code: 'ERR_INVALID_ARG_TYPE' });
}
const cases = [
  ['primitive ctor', () => [1, Parent]],
  ['arrow ctor', () => [() => {}, Parent]],
  ['primitive parent prototype', () => [function C() {}, { prototype: 1 }]],
  ['nonextensible ctor', () => [Object.preventExtensions(function C() {}), Parent]],
  [
    'frozen prototype',
    () => {
      function C() {}

      Object.preventExtensions(C.prototype);
      return [C, Parent];
    }
  ]
];
for (const [label, prepare] of cases) {
  const [ctor, parent] = prepare();
  let failure;
  try {
    inherits(ctor, parent);
  } catch (error) {
    failure = error;
  }
  assert.equal(failure instanceof TypeError, true);
  console.log(
    'PASS inherits invalid target',
    label,
    failure.code ?? 'native-TypeError',
    Object.hasOwn(Object(ctor), 'super_')
  );
}
let reads = 0;
const exception = Error('original getter error');

assert.throws(
  () =>
    inherits(Child, {
      get prototype() {
        reads++;
        throw exception;
      }
    }),
  (error) => error === exception
);

assert.equal(reads, 1);

const originalParent = function A() {};

let sets = 0;
Object.defineProperty(originalParent, 'super_', {
  set() {
    sets++;
  },

  configurable: true
});
inherits(originalParent, Parent);
assert.equal(sets, 0);
assert.equal(originalParent.super_, Parent);
console.log('PASS inherits accessor propagation and own data descriptor without inherited setters');
console.log('DONE inherits');
