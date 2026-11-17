/** Compare byte contracts, never independently generated entropy values. */
import { randomFillSync } from 'node:crypto';
import { Buffer, isUtf8 } from 'node:buffer';
const mite = typeof process.versions.mite === 'string';

function check(value, label) {
  if (!value) {
    throw Error(label);
  }
}

function error(callback) {
  try {
    callback();
    return 'none';
  } catch (value) {
    return value.name + ':' + value.code;
  }
}

const rows = [];
for (const kind of ['array-buffer', 'uint8', 'uint16', 'float64', 'data-view', 'buffer']) {
  const backing = new ArrayBuffer(256);
  const all = new Uint8Array(backing);
  all.fill(90);
  const view =
    kind === 'array-buffer'
      ? backing
      : kind === 'uint8'
        ? new Uint8Array(backing, 16, 128)
        : kind === 'uint16'
          ? new Uint16Array(backing, 16, 64)
          : kind === 'float64'
            ? new Float64Array(backing, 16, 16)
            : kind === 'data-view'
              ? new DataView(backing, 16, 128)
              : Buffer.from(backing, 16, 128);
  const unit = kind === 'uint16' ? 2 : kind === 'float64' ? 8 : 1;
  const offset = kind === 'array-buffer' ? 0 : 16;
  const start = Math.trunc(0.5 * unit);
  const count = Math.trunc((64 / unit) * unit);
  check(randomFillSync(view, 0.5, 64 / unit) === view, 'random fill identity');
  check(
    all.every(
      (value, index) => (index >= offset + start && index < offset + start + count) || value === 90
    ),
    'random fill escaped view range'
  );
  check(
    all.slice(offset + start, offset + start + count).some((value) => value !== 90),
    'OS entropy absent'
  );
  const snapshot = all.slice();
  for (const args of [
    [-1, 0],
    [Infinity, 0],
    [NaN, 0],
    ['1', 1],
    [0, null],
    [0, Infinity],
    [0, 10000],
    [10000, 0]
  ]) {
    rows.push(error(() => randomFillSync(view, ...args)));
    check(
      all.every((value, index) => value === snapshot[index]),
      'invalid fill mutated storage'
    );
  }
  check(
    randomFillSync(view, 0, 0) === view && all.every((value, index) => value === snapshot[index]),
    'empty fill'
  );
}
for (const bytes of [
  [],
  [0, 0x7f],
  [0xc2, 0x80],
  [0xdf, 0xbf],
  [0xed, 0x9f, 0xbf],
  [0xee, 0x80, 0x80],
  [0xf0, 0x90, 0x80, 0x80],
  [0xf4, 0x8f, 0xbf, 0xbf],
  [0xef, 0xbb, 0xbf],
  [0xe2, 0x82, 0xac]
]) {
  check(isUtf8(Uint8Array.from(bytes)) === true, 'valid UTF-8 rejected');
}
for (const bytes of [
  [0x80],
  [0xbf],
  [0xc0, 0x80],
  [0xc1, 0xbf],
  [0xe0, 0x80, 0x80],
  [0xed, 0xa0, 0x80],
  [0xed, 0xbf, 0xbf],
  [0xf0, 0x80, 0x80, 0x80],
  [0xf4, 0x90, 0x80, 0x80],
  [0xf5, 0x80, 0x80, 0x80],
  [0xf8, 0x80, 0x80, 0x80, 0x80],
  [0xc2],
  [0xe2, 0x82],
  [0xf0, 0x90, 0x80]
]) {
  check(isUtf8(Uint8Array.from(bytes)) === false, 'invalid UTF-8 accepted');
}
const backing = Uint8Array.from([255, 0xc2, 0xa2, 255]);
check(isUtf8(backing.subarray(1, 3)), 'UTF-8 subview offset');
check(!isUtf8(backing.buffer), 'UTF-8 full backing');
const words = new Uint16Array(new ArrayBuffer(4));
new Uint8Array(words.buffer).set([0xc2, 0xa2, 0xc2, 0xa2]);
check(isUtf8(words), 'UTF-8 raw typed-array bytes');
let traps = 0;
const guarded = new Uint8Array(128);
for (const key of ['buffer', 'byteLength', 'byteOffset', 'length', 'BYTES_PER_ELEMENT']) {
  Object.defineProperty(guarded, key, {
    get() {
      ++traps;
      throw Error('author getter');
    }
  });
}
check(
  (!mite || randomFillSync(guarded, 0, 0) === guarded) && isUtf8(guarded),
  'native slots must ignore own getters'
);
const proxy = new Proxy(new Uint8Array(8), {
  get() {
    ++traps;
    throw Error('proxy get');
  },

  getPrototypeOf() {
    ++traps;
    throw Error('proxy proto');
  }
});
for (const value of [undefined, null, [], {}]) {
  rows.push(error(() => randomFillSync(value)));
  rows.push(error(() => isUtf8(value)));
}
// Node's JS argument-error formatting may inspect a Proxy, and its random
// wrapper reads public byteLength. Mite deliberately uses only native slots.
if (mite) {
  check(
    error(() => randomFillSync(proxy)) === 'TypeError:ERR_INVALID_ARG_TYPE',
    'proxy fill brand'
  );
  check(error(() => isUtf8(proxy)) === 'TypeError:ERR_INVALID_ARG_TYPE', 'proxy UTF-8 brand');
}
rows.push(error(() => isUtf8(new DataView(new ArrayBuffer(0)))));
check(traps === 0, 'byte primitive invoked author traps');
check(
  randomFillSync(new ArrayBuffer(0)).byteLength === 0 && isUtf8(new ArrayBuffer(0)),
  'empty ArrayBuffer'
);
if (mite && typeof SharedArrayBuffer === 'function') {
  for (const operation of [
    () => randomFillSync(new Uint8Array(new SharedArrayBuffer(8))),
    () => isUtf8(new Uint8Array(new SharedArrayBuffer(8)))
  ]) {
    check(
      error(operation).endsWith(':ERR_MITE_UNSUPPORTED_API'),
      'shared storage must be explicit unsupported'
    );
  }
}
if (mite && typeof ArrayBuffer.prototype.transfer === 'function') {
  const detached = new ArrayBuffer(8);
  const view = new Uint8Array(detached);
  detached.transfer();
  for (const value of [detached, view]) {
    check(
      error(() => randomFillSync(value)).endsWith(':ERR_MITE_UNSUPPORTED_API'),
      'detached fill explicit unsupported'
    );
    check(
      error(() => isUtf8(value)).endsWith(':ERR_MITE_UNSUPPORTED_API'),
      'detached UTF-8 explicit unsupported'
    );
  }
}
if (mite && typeof ArrayBuffer.prototype.resize === 'function') {
  const resizable = new ArrayBuffer(8, { maxByteLength: 16 });
  check(
    error(() => randomFillSync(resizable)).endsWith(':ERR_MITE_UNSUPPORTED_API'),
    'resizable fill explicit unsupported'
  );
  check(
    error(() => isUtf8(resizable)).endsWith(':ERR_MITE_UNSUPPORTED_API'),
    'resizable UTF-8 explicit unsupported'
  );
}
console.log(JSON.stringify({ rows, utf8: true, nativeBrands: true, fillBounds: true }));
