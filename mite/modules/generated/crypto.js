/* Generated from mite/modules/crypto/index.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/crypto/bytes.mjs
function fail(name, Type, code) {
  const error = new Type(`Invalid ${name} argument`);
  error.code = code;
  throw error;
}
function wrapBytes(Buffer, bytes) {
  return Buffer.from(bytes.buffer, bytes.byteOffset, bytes.byteLength);
}

// mite/modules/crypto/random.mjs
function createRandomApi(host) {
  const native = host.native;
  const Buffer = host.Buffer;
  function buffer(bytes) {
    return wrapBytes(Buffer, bytes);
  }
  function randomBytes(size, callback) {
    if (typeof size !== "number") {
      fail("size", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    if (Number.isNaN(size) || size < 0 || size > 2147483647) {
      fail("size", RangeError, "ERR_OUT_OF_RANGE");
    }
    size = size >>> 0;
    if (callback !== void 0 && typeof callback !== "function") {
      fail("callback", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    if (callback === void 0) {
      return buffer(native.random(size));
    }
    if (size === 0) {
      callback(null, buffer(native.random(0)));
      return;
    }
    native.random(size, (error, bytes) => {
      if (error) {
        callback(error);
      } else {
        callback(null, buffer(bytes));
      }
    });
  }
  function randomUUID(options) {
    if (options !== void 0 && (options === null || typeof options !== "object" || Array.isArray(options))) {
      fail("options", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    const disableEntropyCache = options?.disableEntropyCache;
    if (disableEntropyCache !== void 0 && typeof disableEntropyCache !== "boolean") {
      fail("options.disableEntropyCache", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    return native.uuid();
  }
  function randomInt(min, max, callback) {
    if (max === void 0 || typeof max === "function") {
      callback = max;
      max = min;
      min = 0;
    }
    if (callback !== void 0 && typeof callback !== "function") {
      fail("callback", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    if (!Number.isSafeInteger(min)) {
      fail("min", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    if (!Number.isSafeInteger(max)) {
      fail("max", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    const range = max - min;
    const space = 281474976710656;
    if (max <= min || range >= space) {
      fail("max - min", RangeError, "ERR_OUT_OF_RANGE");
    }
    const limit = space - space % range;
    function select(bytes) {
      let value = 0;
      for (let i = 0; i < 6; i++) {
        value = value * 256 + bytes[i];
      }
      return value < limit ? min + value % range : void 0;
    }
    if (callback === void 0) {
      for (; ; ) {
        const result = select(native.random(6));
        if (result !== void 0) {
          return result;
        }
      }
    }
    function attempt() {
      native.random(6, (error, bytes) => {
        if (error) {
          callback(error);
          return;
        }
        const result = select(bytes);
        if (result === void 0) {
          attempt();
          return;
        }
        callback(void 0, result);
      });
    }
    attempt();
  }
  return { randomBytes, randomUUID, randomInt };
}

// mite/modules/crypto/digest.mjs
function createDigestApi(host) {
  const native = host.native;
  const Buffer = host.Buffer;
  function buffer(bytes) {
    return wrapBytes(Buffer, bytes);
  }
  const hashes = /* @__PURE__ */ new WeakMap();
  const Transform = host.stream.Transform;
  function stateFor(hash) {
    const state = hashes.get(hash);
    if (!state) {
      throw new TypeError("Invalid Hash receiver");
    }
    return state;
  }
  function finalized() {
    fail("hash finalized", Error, "ERR_CRYPTO_HASH_FINALIZED");
  }
  function outputLength(options) {
    const length = options !== null && typeof options === "object" ? options.outputLength : void 0;
    if (length === void 0) {
      return void 0;
    }
    if (typeof length !== "number") {
      fail("options.outputLength", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    if (!Number.isInteger(length) || length < 0 || length > 4294967295) {
      fail("options.outputLength", RangeError, "ERR_OUT_OF_RANGE");
    }
    if (length > 2147483647) {
      fail("output length above Mite Buffer limit", RangeError, "ERR_MITE_UNSUPPORTED_API");
    }
    return length;
  }
  function Hash(algorithm, options) {
    if (!new.target) {
      return new Hash(algorithm, options);
    }
    if (typeof algorithm !== "string") {
      fail("algorithm", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    const handle = native.hash.create(algorithm, outputLength(options));
    hashes.set(this, { handle, finalized: false });
    Transform.call(this, options);
  }
  Object.setPrototypeOf(Hash.prototype, Transform.prototype);
  Object.setPrototypeOf(Hash, Transform);
  function encoding(value) {
    if (typeof value !== "string") {
      return void 0;
    }
    const zero = value.indexOf("\0");
    if (zero !== -1) {
      value = value.slice(0, zero);
    }
    value = value.toLowerCase();
    return Buffer.isEncoding(value) ? value : void 0;
  }
  function inputBytes(data, inputEncoding) {
    if (typeof data === "string") {
      if (typeof inputEncoding === "string" && inputEncoding.toLowerCase() === "hex" && data.length % 2) {
        fail("encoding", TypeError, "ERR_INVALID_ARG_VALUE");
      }
      return Buffer.from(data, encoding(inputEncoding) || "utf8");
    }
    if (!ArrayBuffer.isView(data)) {
      fail("data", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    return host.encoding.byteView(data);
  }
  Hash.prototype.update = function update(data, inputEncoding) {
    const state = stateFor(this);
    if (state.finalized) {
      finalized();
    }
    if (!state.handle.update(inputBytes(data, inputEncoding))) {
      fail("hash update", Error, "ERR_CRYPTO_HASH_UPDATE_FAILED");
    }
    return this;
  };
  Hash.prototype.digest = function digest(outputEncoding) {
    const state = stateFor(this);
    if (state.finalized) {
      finalized();
    }
    const label = outputEncoding ? `${outputEncoding}` : void 0;
    const value = buffer(state.handle.digest());
    state.finalized = true;
    const codec = encoding(label);
    return codec ? value.toString(codec) : value;
  };
  Hash.prototype.copy = function copy(options) {
    const state = stateFor(this);
    if (state.finalized) {
      finalized();
    }
    const handle = state.handle.copy(outputLength(options));
    const result = Object.create(Hash.prototype);
    hashes.set(result, { handle, finalized: false });
    Transform.call(result, options);
    return result;
  };
  Hash.prototype._transform = function(chunk, codec, callback) {
    try {
      stateFor(this).handle.update(inputBytes(chunk, codec));
    } catch (error) {
      callback(error);
      return;
    }
    callback();
  };
  Hash.prototype._flush = function(callback) {
    let value;
    try {
      value = buffer(stateFor(this).handle.digest());
    } catch (error) {
      callback(error);
      return;
    }
    this.push(value);
    callback();
  };
  function createHash(algorithm, options) {
    return new Hash(algorithm, options);
  }
  const hmacs = /* @__PURE__ */ new WeakMap();
  function hmacState(value) {
    const state = hmacs.get(value);
    if (!state) {
      throw new TypeError("Invalid Hmac receiver");
    }
    return state;
  }
  function binaryBytes(value) {
    if (value === void 0) {
      fail("key or buffer", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    return host.encoding.byteView(value);
  }
  function keyEncoding(options) {
    const label = options == null ? void 0 : options.encoding;
    if (label != null && typeof label !== "string") {
      fail("options.encoding", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    return label;
  }
  function Hmac(algorithm, key, options) {
    if (!new.target) {
      return new Hmac(algorithm, key, options);
    }
    if (typeof algorithm !== "string") {
      fail("algorithm", TypeError, "ERR_INVALID_ARG_TYPE");
    }
    const label = keyEncoding(options);
    let bytes;
    if (typeof key === "string") {
      if (label && label !== "buffer" && !Buffer.isEncoding(label)) {
        fail("encoding", TypeError, "ERR_UNKNOWN_ENCODING");
      }
      bytes = Buffer.from(key, !label || label === "buffer" ? "utf8" : label);
    } else {
      bytes = binaryBytes(key);
    }
    const handle = native.hmac.create(algorithm, bytes);
    hmacs.set(this, { handle, finalized: false, options });
  }
  Object.setPrototypeOf(Hmac.prototype, Transform.prototype);
  Object.setPrototypeOf(Hmac, Transform);
  for (const name of ["_readableState", "_writableState"]) {
    Object.defineProperty(Hmac.prototype, name, {
      configurable: true,
      get() {
        Transform.call(this, hmacState(this).options);
        return this[name];
      },
      set(value) {
        Object.defineProperty(this, name, {
          value,
          writable: true,
          enumerable: true,
          configurable: true
        });
      }
    });
  }
  Hmac.prototype.update = function update(data, inputEncoding) {
    const state = hmacState(this);
    if (state.finalized) {
      finalized();
    }
    if (!state.handle.update(inputBytes(data, inputEncoding))) {
      fail("HMAC update", Error, "ERR_CRYPTO_HASH_UPDATE_FAILED");
    }
    return this;
  };
  Hmac.prototype.digest = function digest(outputEncoding) {
    const state = hmacState(this);
    if (state.finalized) {
      return outputEncoding && outputEncoding !== "buffer" ? "" : Buffer.alloc(0);
    }
    const label = outputEncoding ? `${outputEncoding}` : void 0;
    const value = buffer(state.handle.digest());
    state.finalized = true;
    const codec = encoding(label);
    return codec ? value.toString(codec) : value;
  };
  Hmac.prototype._transform = function(chunk, codec, callback) {
    try {
      this.update(chunk, codec);
    } catch (error) {
      callback(error);
      return;
    }
    callback();
  };
  Hmac.prototype._flush = function(callback) {
    let value;
    try {
      value = this.digest();
    } catch (error) {
      callback(error);
      return;
    }
    this.push(value);
    callback();
  };
  function createHmac(algorithm, key, options) {
    return new Hmac(algorithm, key, options);
  }
  function timingSafeEqual(left, right) {
    return native.hmac.equal(binaryBytes(left), binaryBytes(right));
  }
  return { createHash, Hash, createHmac, Hmac, timingSafeEqual };
}

// mite/modules/crypto/web-crypto.mjs
function createWebCrypto(host) {
  const native = host.native;
  const DOMException = host.DOMException;
  const fill = native.webRandomFill.bind(native);
  const uuid = native.uuid.bind(native);
  function invalid(code, message) {
    return Object.assign(new TypeError(message), { code });
  }
  class Crypto {
    constructor() {
      throw invalid("ERR_ILLEGAL_CONSTRUCTOR", "Illegal constructor");
    }
    getRandomValues(view) {
      if (this !== webcrypto) {
        throw invalid("ERR_INVALID_THIS", "Value must be a Crypto");
      }
      if (arguments.length === 0) {
        throw invalid("ERR_MISSING_ARGS", "The typedArray argument must be specified");
      }
      try {
        return fill(view);
      } catch (failure) {
        if (failure.code === "ERR_MITE_WEB_CRYPTO_TYPE") {
          throw new DOMException("Expected an integer TypedArray", "TypeMismatchError");
        }
        if (failure.code === "ERR_MITE_WEB_CRYPTO_QUOTA") {
          throw new DOMException("Random values exceed 65536 bytes", "QuotaExceededError");
        }
        throw failure;
      }
    }
    randomUUID() {
      if (this !== webcrypto) {
        throw invalid("ERR_INVALID_THIS", "Value must be a Crypto");
      }
      return uuid();
    }
    get subtle() {
      if (this !== webcrypto) {
        throw invalid("ERR_INVALID_THIS", "Value must be a Crypto");
      }
      throw Object.assign(Error("WebCrypto subtle algorithms are not implemented"), {
        code: "ERR_MITE_UNSUPPORTED_API"
      });
    }
  }
  for (const name of ["getRandomValues", "randomUUID", "subtle"]) {
    const descriptor = Object.getOwnPropertyDescriptor(Crypto.prototype, name);
    Object.defineProperty(Crypto.prototype, name, { ...descriptor, enumerable: true });
  }
  Object.defineProperty(Crypto.prototype, Symbol.toStringTag, {
    value: "Crypto",
    configurable: true
  });
  const webcrypto = Object.create(Crypto.prototype);
  const randomValues = Crypto.prototype.getRandomValues;
  function getRandomValues(view) {
    return arguments.length ? randomValues.call(webcrypto, view) : randomValues.call(webcrypto);
  }
  return { webcrypto, getRandomValues };
}

// mite/modules/crypto/x509.mjs
function createX509(host) {
  const records = /* @__PURE__ */ new WeakMap();
  const Buffer = host.Buffer;
  const byteView = host.encoding?.byteView;
  const isView = ArrayBuffer.isView;
  const limit = 1024 * 1024;
  function state(value) {
    const record = records.get(value);
    if (!record) {
      fail("X509Certificate receiver", TypeError, "ERR_INVALID_THIS");
    }
    return record;
  }
  function unsupported(value, operation) {
    state(value);
    const error = new Error("X509Certificate." + operation + " is not implemented");
    error.code = "ERR_MITE_UNSUPPORTED_API";
    throw error;
  }
  class X509Certificate {
    constructor(input) {
      let bytes;
      if (typeof input === "string") {
        if (input.length > limit || Buffer.byteLength(input, "utf8") > limit) {
          fail("certificate size", RangeError, "ERR_OUT_OF_RANGE");
        }
        bytes = Buffer.from(input, "utf8");
      } else if (isView(input)) {
        bytes = Reflect.apply(byteView, host.encoding, [input]);
        if (bytes.byteLength > limit) {
          fail("certificate size", RangeError, "ERR_OUT_OF_RANGE");
        }
      } else {
        fail("certificate", TypeError, "ERR_INVALID_ARG_TYPE");
      }
      if (!host.native.x509 || typeof host.native.x509.parse !== "function") {
        const error = new Error("Native X509 certificate parser is unavailable");
        error.code = "ERR_MITE_UNSUPPORTED_API";
        throw error;
      }
      const parsed = host.native.x509.parse(bytes);
      records.set(this, { pem: parsed.pem, raw: wrapBytes(Buffer, parsed.raw) });
    }
    toString() {
      return state(this).pem;
    }
    toJSON() {
      return state(this).pem;
    }
    get raw() {
      return state(this).raw;
    }
  }
  for (const name of [
    "subject",
    "subjectAltName",
    "issuer",
    "issuerCertificate",
    "validFrom",
    "validTo",
    "validFromDate",
    "validToDate",
    "fingerprint",
    "fingerprint256",
    "fingerprint512",
    "keyUsage",
    "serialNumber",
    "publicKey",
    "ca",
    "infoAccess"
  ]) {
    Object.defineProperty(X509Certificate.prototype, name, {
      configurable: true,
      get() {
        return unsupported(this, name);
      }
    });
  }
  for (const name of [
    "checkHost",
    "checkEmail",
    "checkIP",
    "checkIssued",
    "checkPrivateKey",
    "verify",
    "toLegacyObject"
  ]) {
    Object.defineProperty(X509Certificate.prototype, name, {
      configurable: true,
      writable: true,
      value() {
        return unsupported(this, name);
      }
    });
  }
  return { X509Certificate };
}

// mite/modules/crypto/verify.mjs
function createVerification(host) {
  const keys = /* @__PURE__ */ new WeakMap();
  const brand = {};
  const Buffer = host.Buffer;
  const byteView = host.encoding?.byteView;
  const isView = ArrayBuffer.isView;
  function fail2(Type, code, message) {
    throw Object.assign(new Type(message), { code });
  }
  function unsupported(message) {
    fail2(Error, "ERR_MITE_UNSUPPORTED_API", message);
  }
  function state(key) {
    const found = keys.get(key);
    if (!found) {
      fail2(TypeError, "ERR_INVALID_ARG_TYPE", "Expected an owned public KeyObject");
    }
    return found;
  }
  function provider() {
    if (!host.native.verify || typeof host.native.verify.create !== "function" || typeof host.native.verify.verify !== "function") {
      unsupported("Native public-key verification is unavailable");
    }
    return host.native.verify;
  }
  class PublicKeyObject {
    constructor(token, der) {
      if (token !== brand) {
        fail2(TypeError, "ERR_ILLEGAL_CONSTRUCTOR", "Illegal KeyObject constructor");
      }
      keys.set(this, der);
    }
    get type() {
      state(this);
      return "public";
    }
    get asymmetricKeyType() {
      state(this);
      return "ec";
    }
    get asymmetricKeyDetails() {
      state(this);
      return { namedCurve: "prime256v1" };
    }
    export() {
      state(this);
      unsupported("KeyObject.export is not implemented");
    }
    equals() {
      state(this);
      unsupported("KeyObject.equals is not implemented");
    }
  }
  function coordinate(value) {
    if (typeof value !== "string" || !/^[A-Za-z0-9_-]{43}$/.test(value)) {
      fail2(TypeError, "ERR_CRYPTO_INVALID_JWK", "P-256 coordinates require canonical base64url");
    }
    const result = Buffer.from(value, "base64url");
    if (result.length !== 32 || result.toString("base64url") !== value) {
      fail2(TypeError, "ERR_CRYPTO_INVALID_JWK", "Invalid P-256 coordinate");
    }
    return result;
  }
  function createPublicKey(options) {
    if (!options || typeof options !== "object" || Array.isArray(options)) {
      fail2(TypeError, "ERR_INVALID_ARG_TYPE", "Public key options must be an object");
    }
    for (const name of Reflect.ownKeys(options)) {
      if (!["key", "format"].includes(name)) {
        unsupported("Public key option " + String(name));
      }
    }
    if (options.format !== "jwk") {
      unsupported("Only public EC P-256 JWK keys are implemented");
    }
    const key = options.key;
    if (!key || typeof key !== "object" || Array.isArray(key)) {
      fail2(TypeError, "ERR_INVALID_ARG_TYPE", "JWK must be an object");
    }
    const kty = key.kty;
    const crv = key.crv;
    if (kty !== "EC" || crv !== "P-256") {
      unsupported("Only EC P-256 public keys are implemented");
    }
    if ("d" in key) {
      unsupported("Private JWK keys are not implemented");
    }
    const x = coordinate(key.x);
    const y = coordinate(key.y);
    return new PublicKeyObject(brand, provider().create(x, y));
  }
  function bytes(value, name, maximum) {
    if (typeof value === "string") {
      if (Buffer.byteLength(value, "utf8") > maximum) {
        fail2(RangeError, "ERR_OUT_OF_RANGE", name + " exceeds budget");
      }
      return Buffer.from(value, "utf8");
    }
    if (!isView(value)) {
      fail2(TypeError, "ERR_INVALID_ARG_TYPE", name + " must be bytes");
    }
    const result = Reflect.apply(byteView, host.encoding, [value]);
    if (result.byteLength > maximum) {
      fail2(RangeError, "ERR_OUT_OF_RANGE", name + " exceeds budget");
    }
    return result;
  }
  function verify(algorithm, data, options, signature, callback) {
    if (callback !== void 0) {
      unsupported("Asynchronous signature verification is not implemented");
    }
    if (algorithm !== "sha256" && algorithm !== "SHA256") {
      unsupported("Only SHA-256 P1363 verification is implemented");
    }
    if (!options || typeof options !== "object" || Array.isArray(options)) {
      fail2(TypeError, "ERR_INVALID_ARG_TYPE", "Verification options must be an object");
    }
    for (const name of Reflect.ownKeys(options)) {
      if (!["key", "dsaEncoding"].includes(name)) {
        unsupported("Verification option " + String(name));
      }
    }
    const key = state(options.key);
    if (options.dsaEncoding !== "ieee-p1363") {
      unsupported("Only ieee-p1363 signatures are implemented");
    }
    const message = bytes(data, "data", 1024 * 1024);
    if (!isView(signature)) {
      fail2(TypeError, "ERR_INVALID_ARG_TYPE", "signature must be bytes");
    }
    const proof = bytes(signature, "signature", 1024 * 1024);
    if (proof.byteLength !== 64) {
      return false;
    }
    return provider().verify(key, message, proof);
  }
  return { createPublicKey, verify };
}

// mite/modules/crypto/index.mjs
function createCryptoModule(host) {
  return {
    ...createRandomApi(host),
    ...createDigestApi(host),
    ...createWebCrypto(host),
    ...createX509(host),
    ...createVerification(host)
  };
}
return createCryptoModule;
})()
