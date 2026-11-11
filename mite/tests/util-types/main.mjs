/** Engine brands must not consult author properties or Proxy traps. */
import types from 'node:util/types';
import { types as alias } from 'node:util';
import { Buffer } from 'node:buffer';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

check(types === alias, 'Canonical util.types/builtin identity');
let traps = 0;
const hostile = new Proxy(
  {},
  {
    get() {
      traps++;
      throw Error('get');
    },

    getPrototypeOf() {
      traps++;
      throw Error('prototype');
    }
  }
);
const revoked = Proxy.revocable({}, {});
revoked.revoke();
const spoof = {
  get [Symbol.toStringTag]() {
    traps++;
    throw Error('tag');
  },

  get then() {
    traps++;
    throw Error('then');
  }
};
const rows = [
  undefined,
  null,
  1,
  {},
  spoof,
  Object.create(Uint8Array.prototype),
  hostile,
  revoked.proxy,
  new ArrayBuffer(8),
  new SharedArrayBuffer(8),
  new DataView(new ArrayBuffer(8)),
  new Uint8Array(8),
  new Uint8ClampedArray(8),
  new Uint16Array(8),
  Buffer.alloc(2),
  Promise.resolve(1),
  new Proxy(new Uint8Array(1), {})
];
const names = [
  'isArrayBuffer',
  'isSharedArrayBuffer',
  'isAnyArrayBuffer',
  'isArrayBufferView',
  'isTypedArray',
  'isUint8Array',
  'isDataView',
  'isProxy',
  'isPromise'
];
const actual = rows.map((value) => names.map((name) => types[name](value)));
check(traps === 0, 'Native predicates invoke no Proxy/tag/then hooks');
const buffer = new ArrayBuffer(4);
const view = new Uint8Array(buffer);
const dataView = new DataView(buffer);
buffer.transfer();
check(
  types.isArrayBuffer(buffer) &&
    types.isTypedArray(view) &&
    types.isUint8Array(view) &&
    types.isDataView(dataView),
  'Detached views retain genuine brands'
);
console.log('PASS util/types ' + JSON.stringify(actual));
