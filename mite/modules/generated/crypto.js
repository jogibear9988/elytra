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

// mite/modules/crypto/index.mjs
function createCryptoModule(host) {
  return { ...createRandomApi(host), ...createDigestApi(host) };
}
return createCryptoModule;
})()
