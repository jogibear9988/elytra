/** Real OS entropy through global and node aliases; deterministic checks cover view boundaries and errors. */
import * as nodeCrypto from 'node:crypto';
import { createRequire } from 'node:module';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, name, code, dom = false) {
  let failure;
  try {
    action();
  } catch (error) {
    failure = error;
  }
  check(
    failure?.name === name && failure.code === code && failure instanceof DOMException === dom,
    'Expected ' + name + '/' + code + ', got ' + failure?.name + '/' + failure?.code
  );
}

const crypto = globalThis.crypto;
const common = createRequire(import.meta.url)('crypto');
check(
  crypto === nodeCrypto.webcrypto && crypto === common.webcrypto,
  'ESM/CommonJS/global webcrypto identity'
);
check(
  crypto instanceof Crypto &&
    crypto.constructor === Crypto &&
    Object.prototype.toString.call(crypto) === '[object Crypto]',
  'Genuine branded Crypto surface'
);
check(
  nodeCrypto.getRandomValues !== crypto.getRandomValues &&
    nodeCrypto.randomUUID !== crypto.randomUUID,
  'Node wrappers retain their separate contracts'
);

fails(() => new Crypto(), 'TypeError', 'ERR_ILLEGAL_CONSTRUCTOR');

for (const Type of [
  Int8Array,
  Uint8Array,
  Uint8ClampedArray,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  BigInt64Array,
  BigUint64Array
]) {
  const backing = new ArrayBuffer(96);
  const bytes = new Uint8Array(backing);
  bytes.fill(0x5a);
  const view = new Type(backing, 16, 32 / Type.BYTES_PER_ELEMENT);
  check(crypto.getRandomValues(view) === view, 'Same integer view returned');
  check(
    bytes.slice(0, 16).every((byte) => byte === 0x5a) &&
      bytes.slice(48).every((byte) => byte === 0x5a),
    'Entropy confined to exact byte offset/length'
  );
  check(
    bytes.slice(16, 48).some((byte) => byte !== 0x5a),
    'Native entropy replaced view bytes'
  );
  const zero = new Type(backing, 16, 0);
  check(crypto.getRandomValues(zero) === zero, 'Zero length integer view accepted');
}
const buffer = Buffer.alloc(64, 0x31);
check(
  nodeCrypto.getRandomValues(buffer.subarray(8, 40)).buffer === buffer.buffer,
  'Node Buffer integer view accepted'
);
check(
  buffer.subarray(0, 8).every((byte) => byte === 0x31) &&
    buffer.subarray(40).every((byte) => byte === 0x31),
  'Buffer subview offsets preserved'
);
console.log(
  'PASS branded global aliases integer views byte offsets buffers and intrinsic validation'
);

for (const value of [
  undefined,
  null,
  1,
  {},
  new ArrayBuffer(8),
  new DataView(new ArrayBuffer(8)),
  new Float32Array(1),
  new Float64Array(1),
  Object.create(Uint8Array.prototype),
  new Proxy(new Uint8Array(1), {})
]) {
  fails(() => crypto.getRandomValues(value), 'TypeMismatchError', 17, true);
}
const spoof = {
  [Symbol.toStringTag]: 'Uint8Array',

  get byteLength() {
    throw Error('Spoof accessed');
  }
};

fails(() => crypto.getRandomValues(spoof), 'TypeMismatchError', 17, true);

fails(() => crypto.getRandomValues(), 'TypeError', 'ERR_MISSING_ARGS');

fails(() => crypto.getRandomValues.call({}, new Uint8Array(1)), 'TypeError', 'ERR_INVALID_THIS');

fails(
  () => crypto.randomUUID.call(Object.create(Crypto.prototype)),
  'TypeError',
  'ERR_INVALID_THIS'
);

const alias = nodeCrypto.getRandomValues;
check(
  alias.call({}, new Uint8Array(1)) instanceof Uint8Array,
  'Node alias is callable without Crypto receiver'
);
const maximum = new Uint8Array(65536);
check(crypto.getRandomValues(maximum) === maximum, 'Exactly 65536 bytes accepted');
for (const value of [new Uint8Array(65537), new Uint32Array(16385), new BigUint64Array(8193)]) {
  fails(() => crypto.getRandomValues(value), 'QuotaExceededError', 22, true);
}
console.log('PASS WebCrypto DOMException types receiver checks quota and unbound Node helper');

const detachedBuffer = new ArrayBuffer(32);
const detached = new Uint8Array(detachedBuffer);
detachedBuffer.transfer();
check(
  crypto.getRandomValues(detached) === detached && detached.byteLength === 0,
  'Detached integer view is zero-length no-op'
);
const resizable = new ArrayBuffer(32, { maxByteLength: 128 });
const outOfBounds = new Uint32Array(resizable, 16, 4);
resizable.resize(4);
check(
  crypto.getRandomValues(outOfBounds) === outOfBounds && outOfBounds.byteLength === 0,
  'Resized out-of-bounds integer view is zero-length no-op'
);
const trackingBuffer = new ArrayBuffer(8, { maxByteLength: 65544 });
const tracking = new Uint8Array(trackingBuffer);
trackingBuffer.resize(65537);

fails(() => crypto.getRandomValues(tracking), 'QuotaExceededError', 22, true);

const shared = new Uint8Array(new SharedArrayBuffer(32));
check(
  crypto.getRandomValues(shared) === shared && shared.some((byte) => byte !== 0),
  'Shared integer backing receives native entropy'
);
console.log('PASS detached resizable out-of-bounds tracking and shared backing stores');

const seen = new Set();
for (let index = 0; index < 128; index++) {
  const value = crypto.randomUUID();
  check(
    /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(value),
    'RFC4122 random UUID version/variant'
  );
  seen.add(value);
}
check(seen.size === 128, 'Independent native UUID requests');
const originalMath = Math.random;

Math.random = () => {
  throw Error('Insecure entropy fallback');
};

try {
  crypto.getRandomValues(new Uint8Array(32));
  crypto.randomUUID();
} finally {
  Math.random = originalMath;
}
console.log('PASS fresh native random UUID formatting and no Math.random fallback');
console.log('DONE WebCrypto randomness');
