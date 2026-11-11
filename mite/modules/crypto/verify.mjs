/** Public EC/P-256 JWK admission and genuine synchronous ES256 verification only. */
export default function createVerification(host) {
  const keys = new WeakMap();
  const brand = {};
  const Buffer = host.Buffer;
  const byteView = host.encoding?.byteView;
  const isView = ArrayBuffer.isView;

  function fail(Type, code, message) {
    throw Object.assign(new Type(message), { code });
  }

  function unsupported(message) {
    fail(Error, 'ERR_MITE_UNSUPPORTED_API', message);
  }

  function state(key) {
    const found = keys.get(key);
    if (!found) {
      fail(TypeError, 'ERR_INVALID_ARG_TYPE', 'Expected an owned public KeyObject');
    }
    return found;
  }

  function provider() {
    if (
      !host.native.verify ||
      typeof host.native.verify.create !== 'function' ||
      typeof host.native.verify.verify !== 'function'
    ) {
      unsupported('Native public-key verification is unavailable');
    }
    return host.native.verify;
  }

  class PublicKeyObject {
    constructor(token, der) {
      if (token !== brand) {
        fail(TypeError, 'ERR_ILLEGAL_CONSTRUCTOR', 'Illegal KeyObject constructor');
      }
      keys.set(this, der);
    }

    get type() {
      state(this);
      return 'public';
    }

    get asymmetricKeyType() {
      state(this);
      return 'ec';
    }

    get asymmetricKeyDetails() {
      state(this);
      return { namedCurve: 'prime256v1' };
    }

    export() {
      state(this);
      unsupported('KeyObject.export is not implemented');
    }

    equals() {
      state(this);
      unsupported('KeyObject.equals is not implemented');
    }
  }

  function coordinate(value) {
    if (typeof value !== 'string' || !/^[A-Za-z0-9_-]{43}$/.test(value)) {
      fail(TypeError, 'ERR_CRYPTO_INVALID_JWK', 'P-256 coordinates require canonical base64url');
    }
    const result = Buffer.from(value, 'base64url');
    if (result.length !== 32 || result.toString('base64url') !== value) {
      fail(TypeError, 'ERR_CRYPTO_INVALID_JWK', 'Invalid P-256 coordinate');
    }
    return result;
  }

  function createPublicKey(options) {
    if (!options || typeof options !== 'object' || Array.isArray(options)) {
      fail(TypeError, 'ERR_INVALID_ARG_TYPE', 'Public key options must be an object');
    }
    for (const name of Reflect.ownKeys(options)) {
      if (!['key', 'format'].includes(name)) {
        unsupported('Public key option ' + String(name));
      }
    }
    if (options.format !== 'jwk') {
      unsupported('Only public EC P-256 JWK keys are implemented');
    }
    const key = options.key;
    if (!key || typeof key !== 'object' || Array.isArray(key)) {
      fail(TypeError, 'ERR_INVALID_ARG_TYPE', 'JWK must be an object');
    }
    const kty = key.kty;
    const crv = key.crv;
    if (kty !== 'EC' || crv !== 'P-256') {
      unsupported('Only EC P-256 public keys are implemented');
    }
    if ('d' in key) {
      unsupported('Private JWK keys are not implemented');
    }
    const x = coordinate(key.x);
    const y = coordinate(key.y);
    return new PublicKeyObject(brand, provider().create(x, y));
  }

  function bytes(value, name, maximum) {
    if (typeof value === 'string') {
      if (Buffer.byteLength(value, 'utf8') > maximum) {
        fail(RangeError, 'ERR_OUT_OF_RANGE', name + ' exceeds budget');
      }
      return Buffer.from(value, 'utf8');
    }
    if (!isView(value)) {
      fail(TypeError, 'ERR_INVALID_ARG_TYPE', name + ' must be bytes');
    }
    const result = Reflect.apply(byteView, host.encoding, [value]);
    if (result.byteLength > maximum) {
      fail(RangeError, 'ERR_OUT_OF_RANGE', name + ' exceeds budget');
    }
    return result;
  }

  function verify(algorithm, data, options, signature, callback) {
    if (callback !== undefined) {
      unsupported('Asynchronous signature verification is not implemented');
    }
    if (algorithm !== 'sha256' && algorithm !== 'SHA256') {
      unsupported('Only SHA-256 P1363 verification is implemented');
    }
    if (!options || typeof options !== 'object' || Array.isArray(options)) {
      fail(TypeError, 'ERR_INVALID_ARG_TYPE', 'Verification options must be an object');
    }
    for (const name of Reflect.ownKeys(options)) {
      if (!['key', 'dsaEncoding'].includes(name)) {
        unsupported('Verification option ' + String(name));
      }
    }
    const key = state(options.key);
    if (options.dsaEncoding !== 'ieee-p1363') {
      unsupported('Only ieee-p1363 signatures are implemented');
    }
    const message = bytes(data, 'data', 1024 * 1024);
    if (!isView(signature)) {
      fail(TypeError, 'ERR_INVALID_ARG_TYPE', 'signature must be bytes');
    }
    const proof = bytes(signature, 'signature', 1024 * 1024);
    if (proof.byteLength !== 64) {
      return false;
    }
    return provider().verify(key, message, proof);
  }

  return { createPublicKey, verify };
}
