/** Observable callback adaptation, descriptor preservation and custom Promise contracts. */
import util, { promisify } from 'node:util';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

function check(value, label) {
  if (!value) {
    throw new Error(label);
  }
}

const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
check(util === require('util') && util.promisify === promisify, 'module identities');
check(promisify.custom === Symbol.for('nodejs.util.promisify.custom'), 'global symbol');
const symbol = Symbol('metadata');
let gets = 0;

function original(a, b, callback) {
  callback(null, this.offset + a + b, 'ignored');
}

Object.defineProperty(original, 'meta', {
  get() {
    gets++;
    return 42;
  },

  configurable: true
});
original[symbol] = 'symbol';
const wrapped = promisify(original);
check(gets === 0, 'do not read metadata accessors');
check(wrapped.name === original.name && wrapped.length === original.length, 'function metadata');
check(Object.getPrototypeOf(wrapped) === Object.getPrototypeOf(original), 'function prototype');
check(wrapped.prototype === original.prototype, 'constructor prototype descriptor');
check(wrapped.meta === 42 && gets === 1 && wrapped[symbol] === 'symbol', 'copied accessor/symbol');
check((await wrapped.call({ offset: 10 }, 2, 3)) === 15, 'receiver and first success value');
check(
  promisify(wrapped) === wrapped && promisify(original) !== wrapped,
  'self marker without caching original'
);
const descriptor = Object.getOwnPropertyDescriptor(wrapped, promisify.custom);
check(
  !descriptor.writable && !descriptor.enumerable && descriptor.configurable,
  'custom marker descriptor'
);
console.log(
  'PASS util identities, receiver, descriptors, symbols, self marker and uncached wrapper creation'
);

const reason = { label: 'exact reason' };
for (const error of [reason, 42, 'error', true]) {
  try {
    await promisify((cb) => cb(error))();
    throw new Error('resolved error');
  } catch (actual) {
    check(actual === error, 'exact callback error');
  }
}
for (const error of [undefined, null, false, 0, '']) {
  check((await promisify((cb) => cb(error, 'ok'))()) === 'ok', 'falsy error means success');
}
try {
  await promisify(() => {
    throw reason;
  })();
  throw new Error('throw lost');
} catch (actual) {
  check(actual === reason, 'synchronous exception becomes rejection');
}
check((await promisify((cb) => cb(null))()) === undefined, 'missing success value');
check(
  (await promisify((cb) => {
    cb(null, 'first');
    cb(reason);
    throw reason;
  })()) === 'first',
  'first settlement wins'
);
let callback;
const delayed = promisify((cb) => {
  callback = cb;
})();
if (typeof collect === 'function') {
  collect();
}
callback(null, Promise.resolve('adopted'));
check((await delayed) === 'adopted', 'callback and result Promise survive GC');
const scheduled = promisify(function (cb) {
  setImmediate(() => cb(null, this.value));
}).call({ value: 'native rooted' });
if (typeof collect === 'function') {
  collect();
}
check((await scheduled) === 'native rooted', 'native timer roots pending callback and receiver');
const order = [];
const pending = promisify((cb) => {
  order.push('inside');
  cb(null, 1);
  order.push('after');
})();

pending.then(() => order.push('then'));

order.push('outside');
await pending;
check(order.join() === 'inside,after,outside,then', 'Promise checkpoint ordering');
console.log(
  'PASS exact failures, falsy errors, synchronous throws, settlement, Promise adoption and ordering'
);

function source() {
  throw new Error('original should not run');
}

const custom = function (value) {
  check(this === source, 'custom receiver');
  return Promise.resolve(value);
};

source[promisify.custom] = custom;
check(promisify(source) === custom && promisify(custom) === custom, 'custom identity and marker');
check((await promisify(source).call(source, 'custom')) === 'custom', 'custom invocation');
const inherited = Object.create(Function.prototype, { [promisify.custom]: { value: custom } });
Object.setPrototypeOf(source, inherited);
check(promisify(source) === custom, 'inherited custom implementation');
let customGets = 0;

function accessorSource() {}

Object.defineProperty(accessorSource, promisify.custom, {
  get() {
    customGets++;
    return custom;
  }
});
check(promisify(accessorSource) === custom && customGets === 2, 'custom accessor read order');
for (const value of [undefined, null, false, 0, '']) {
  function fn(cb) {
    cb(null, 'fallback');
  }

  fn[promisify.custom] = value;
  const result = promisify(fn);
  check(
    (await result()) === 'fallback' && result[promisify.custom] === value,
    'falsy custom metadata'
  );
}
for (const value of [undefined, null, 0, 'x', {}, Symbol()]) {
  try {
    promisify(value);
    throw new Error('invalid original accepted');
  } catch (error) {
    check(
      error instanceof TypeError && error.code === 'ERR_INVALID_ARG_TYPE',
      'original validation'
    );
  }
}
for (const value of [1, 'x', {}, Symbol()]) {
  function fn() {}

  fn[promisify.custom] = value;
  try {
    promisify(fn);
    throw new Error('invalid custom accepted');
  } catch (error) {
    check(error instanceof TypeError && error.code === 'ERR_INVALID_ARG_TYPE', 'custom validation');
  }
}

const getter = () => {};

Object.defineProperty(getter, promisify.custom, {
  get() {
    throw reason;
  }
});
try {
  promisify(getter);
  throw new Error('getter error lost');
} catch (error) {
  check(error === reason, 'exact custom getter throw');
}
console.log(
  'PASS custom/inherited/falsy implementations, global symbol and exact validation/getter errors'
);
if (typeof collect === 'function') {
  console.log('PASS real GC with pending promisified callback and captured receiver');
}
