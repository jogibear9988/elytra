/** Structured clone graph and native-brand behavior, with explicit supported-subset differences. */
import assert from 'node:assert/strict';
import process from 'node:process';
import { types } from 'node:util';
const mite = !!process.versions.mite;
const clone = globalThis.structuredClone;
assert.equal(typeof clone, 'function');
const descriptor = Object.getOwnPropertyDescriptor(globalThis, 'structuredClone');
assert.equal(descriptor.writable, true);
assert.equal(descriptor.enumerable, true);
assert.equal(descriptor.configurable, true);

assert.throws(() => clone(), TypeError);

assert.equal(clone(undefined), undefined);
for (const value of [null, true, false, 0, -0, NaN, Infinity, -Infinity, 123n, '\ud800x\udfff']) {
  assert.ok(Object.is(clone(value), value));
}
assert.equal(clone(7, null), 7);
assert.equal(clone(7, { transfer: [] }), 7);

assert.throws(() => clone(7, 42), TypeError);

assert.throws(() => clone(7, { transfer: null }), TypeError);

let transferReads = 0;
assert.equal(
  clone(7, {
    get transfer() {
      transferReads++;
      return [];
    }
  }),
  7
);
assert.equal(transferReads, 1);
console.log('PASS global descriptor, required argument, primitive identity and options validation');

const shared = { value: 4 };
const source = { shared, alias: shared, array: new Array(4) };
source.self = source;
source.array[2] = shared;
source.array.extra = 'kept';
const graph = clone(source);
assert.notEqual(graph, source);
assert.equal(graph.self, graph);
assert.equal(graph.shared, graph.alias);
assert.equal(graph.array[2], graph.shared);
assert.equal(graph.array.length, 4);
assert.equal(0 in graph.array, false);
assert.equal(1 in graph.array, false);
assert.equal(3 in graph.array, false);
assert.equal(graph.array.extra, 'kept');
graph.shared.value = 9;
assert.equal(source.shared.value, 4);
console.log('PASS cycles, repeated aliases, sparse arrays and independent mutable graph');

let gets = 0;
class Record {
  method() {
    return 'source-only';
  }
}
const record = new Record();
Object.defineProperty(record, 'value', {
  enumerable: true,

  get() {
    gets++;
    return shared;
  }
});
Object.defineProperty(record, 'hidden', { value: 42 });

record[Symbol('hidden')] = () => {};

const normalized = clone(record);
assert.equal(gets, 1);
assert.equal(Object.getPrototypeOf(normalized), Object.prototype);
assert.deepEqual(Object.keys(normalized), ['value']);
assert.equal(normalized.method, undefined);
assert.deepEqual(Object.getOwnPropertyDescriptor(normalized, 'value'), {
  value: normalized.value,
  writable: true,
  enumerable: true,
  configurable: true
});
let inheritedReads = 0;
const inherited = Object.create({
  get hidden() {
    inheritedReads++;
    throw Error('inherited getter');
  }
});
inherited.own = 5;
assert.deepEqual(clone(inherited), { own: 5 });
assert.equal(inheritedReads, 0);
const nullPrototype = Object.create(null);
nullPrototype.own = 3;
assert.equal(Object.getPrototypeOf(clone(nullPrototype)), Object.prototype);
const thrown = new Error('owned getter failure');

assert.throws(
  () =>
    clone({
      get value() {
        throw thrown;
      }
    }),
  (error) => error === thrown
);

console.log('PASS enumerable getters once, prototype normalization and original getter exceptions');

const date = new Date(123456789);
const regexp = /hello/gi;
regexp.lastIndex = 4;
const key = { owned: 1 };
const map = new Map([[key, { nested: key }]]);
const set = new Set([key, map]);
const builtin = clone({ date, invalid: new Date(NaN), regexp, key, map, set });
assert.equal(builtin.date.getTime(), date.getTime());
assert.ok(Number.isNaN(builtin.invalid.getTime()));
assert.equal(builtin.regexp.source, 'hello');
assert.equal(builtin.regexp.flags, 'gi');
assert.equal(builtin.regexp.lastIndex, 0);
assert.equal(builtin.map.get(builtin.key).nested, builtin.key);
assert.equal(builtin.set.has(builtin.key), true);
assert.equal(builtin.set.has(builtin.map), true);
assert.notEqual(builtin.key, key);
console.log('PASS genuine Date, RegExp, Map and Set brands with graph aliases');

const backing = new ArrayBuffer(24);
const bytes = new Uint8Array(backing, 3, 5);
bytes.set([0, 255, 7, 128, 9]);
const words = new Uint16Array(backing, 8, 3);
const data = new DataView(backing, 2, 12);
const buffer = Buffer.from([1, 255, 0]);
const binary = clone({ backing, bytes, words, data, again: bytes, buffer });
assert.notEqual(binary.backing, backing);
assert.equal(binary.bytes.buffer, binary.backing);
assert.equal(binary.words.buffer, binary.backing);
assert.equal(binary.data.buffer, binary.backing);
assert.equal(binary.bytes, binary.again);
assert.equal(binary.bytes.byteOffset, 3);
assert.equal(binary.bytes.length, 5);
assert.equal(binary.words.byteOffset, 8);
assert.equal(binary.words.length, 3);
assert.equal(binary.data.byteOffset, 2);
assert.equal(binary.data.byteLength, 12);
assert.deepEqual(Array.from(binary.bytes), [0, 255, 7, 128, 9]);
assert.equal(binary.buffer instanceof Uint8Array, true);
assert.equal(Buffer.isBuffer(binary.buffer), false);
assert.deepEqual(Array.from(binary.buffer), [1, 255, 0]);
binary.bytes[0] = 42;
assert.equal(bytes[0], 0);
bytes[1] = 11;
assert.equal(binary.bytes[1], 255);
// Internal slots, rather than user-overridable view properties, select the clone.
let viewProbes = 0;
const poisonedView = new Uint16Array(new ArrayBuffer(12), 2, 3);
poisonedView.set([7, 255, 1024]);
for (const key of ['buffer', 'constructor', Symbol.toStringTag]) {
  Object.defineProperty(poisonedView, key, {
    get() {
      viewProbes++;
      throw Error('view probe');
    },

    configurable: true
  });
}
const cleanView = clone(poisonedView);
assert.equal(cleanView instanceof Uint16Array, true);
assert.equal(cleanView.byteOffset, 2);
assert.equal(cleanView.length, 3);
assert.deepEqual(Array.from(cleanView), [7, 255, 1024]);
assert.equal(viewProbes, 0);
console.log('PASS cloned ArrayBuffer ownership, view offsets, aliases and Buffer normalization');

function uncloneable(value) {
  assert.throws(
    () => clone(value),
    (error) => error.name === 'DataCloneError'
  );
}

let traps = 0;
const proxy = new Proxy(
  {},
  {
    get() {
      traps++;
      throw Error('proxy get');
    },

    ownKeys() {
      traps++;
      throw Error('proxy keys');
    },

    getPrototypeOf() {
      traps++;
      throw Error('proxy prototype');
    }
  }
);
uncloneable(proxy);
assert.equal(traps, 0);
for (const value of [
  () => {},
  Symbol('value'),
  { value: () => {} },
  { value: Symbol('value') },
  new WeakMap(),
  new WeakSet(),
  Promise.resolve(1)
]) {
  uncloneable(value);
}
if (mite) {
  // Public util.types mutation cannot replace the private native admission checks.
  const originalIsProxy = types.isProxy;
  let predicateCalls = 0;
  try {
    types.isProxy = () => {
      predicateCalls++;
      return false;
    };

    uncloneable(proxy);
    assert.equal(predicateCalls, 0);
    assert.equal(traps, 0);
  } finally {
    types.isProxy = originalIsProxy;
  }

  const hosts = [
    new Blob(['owned']),
    new URL('https://owned.invalid/path'),
    new URLSearchParams('a=1'),
    new AbortController().signal,
    new Request('https://owned.invalid/'),
    new Response('owned'),
    new Headers({ 'x-owned': 'value' }),
    new FormData(),
    new ReadableStream(),
    new WritableStream(),
    new TransformStream()
  ];
  let hostProbes = 0;
  for (const value of hosts) {
    const prototype = Object.getPrototypeOf(value);
    Object.defineProperty(value, 'probe', {
      enumerable: true,
      configurable: true,

      get() {
        hostProbes++;
        throw Error('host probe');
      }
    });
    try {
      Object.setPrototypeOf(value, Object.prototype);
      uncloneable(value);
      assert.equal(hostProbes, 0);
    } finally {
      Object.setPrototypeOf(value, prototype);
      delete value.probe;
    }
  }
}
if (mite) {
  for (const value of [
    new SharedArrayBuffer(8),
    new Uint8Array(new SharedArrayBuffer(8)),
    new Error('native-error')
  ]) {
    assert.throws(
      () => clone(value),
      (error) => error.name === 'DataCloneError' || error.code === 'ERR_MITE_UNSUPPORTED_API'
    );
  }
  if (typeof Blob === 'function') {
    assert.throws(
      () => clone(new Blob(['owned'])),
      (error) => error.name === 'DataCloneError' || error.code === 'ERR_MITE_UNSUPPORTED_API'
    );
  }
}
console.log('PASS uncloneable capabilities refused and Proxy hooks remain uncalled');

const transferable = new ArrayBuffer(8);
new Uint8Array(transferable)[0] = 37;
if (mite) {
  const unchanged = { value: transferable };

  assert.throws(
    () => clone(unchanged, { transfer: [transferable] }),
    (error) => error.code === 'ERR_MITE_UNSUPPORTED_API'
  );

  let inputReads = 0;

  assert.throws(
    () =>
      clone(
        {
          get value() {
            inputReads++;
            return transferable;
          }
        },
        { transfer: [transferable] }
      ),
    (error) => error.code === 'ERR_MITE_UNSUPPORTED_API'
  );

  assert.equal(inputReads, 0);
  assert.equal(transferable.byteLength, 8);
  assert.equal(new Uint8Array(transferable)[0], 37);
} else {
  const moved = clone({ value: transferable }, { transfer: [transferable] });
  assert.equal(transferable.byteLength, 0);
  assert.equal(new Uint8Array(moved.value)[0], 37);
}
console.log('PASS transfer behavior explicit and unsupported Mite transfer leaves bytes attached');
