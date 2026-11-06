/** Hash/HMAC stream state, provider handles and exact-byte equality policy. */
import { fail, wrapBytes } from './bytes.mjs';

export default function createDigestApi(host) {
  const native = host.native;
  const Buffer = host.Buffer;

  function buffer(bytes) {
    return wrapBytes(Buffer, bytes);
  }

  const hashes = new WeakMap();

  const Transform = host.stream.Transform;

  function stateFor(hash) {
    const state = hashes.get(hash);
    if (!state) {
      throw new TypeError('Invalid Hash receiver');
    }
    return state;
  }

  function finalized() {
    fail('hash finalized', Error, 'ERR_CRYPTO_HASH_FINALIZED');
  }

  function outputLength(options) {
    const length =
      options !== null && typeof options === 'object' ? options.outputLength : undefined;
    if (length === undefined) {
      return undefined;
    }
    if (typeof length !== 'number') {
      fail('options.outputLength', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    if (!Number.isInteger(length) || length < 0 || length > 0xffffffff) {
      fail('options.outputLength', RangeError, 'ERR_OUT_OF_RANGE');
    }
    if (length > 0x7fffffff) {
      fail('output length above Mite Buffer limit', RangeError, 'ERR_MITE_UNSUPPORTED_API');
    }
    return length;
  }

  /** Keep callable construction and Transform behavior while a WeakMap owns each native handle. */
  function Hash(algorithm, options) {
    if (!new.target) {
      return new Hash(algorithm, options);
    }
    if (typeof algorithm !== 'string') {
      fail('algorithm', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    const handle = native.hash.create(algorithm, outputLength(options));
    hashes.set(this, { handle, finalized: false });
    Transform.call(this, options);
  }

  Object.setPrototypeOf(Hash.prototype, Transform.prototype);

  Object.setPrototypeOf(Hash, Transform);

  function encoding(value) {
    if (typeof value !== 'string') {
      return undefined;
    }
    const zero = value.indexOf('\0');
    if (zero !== -1) {
      value = value.slice(0, zero);
    }
    value = value.toLowerCase();
    return Buffer.isEncoding(value) ? value : undefined;
  }

  function inputBytes(data, inputEncoding) {
    if (typeof data === 'string') {
      if (
        typeof inputEncoding === 'string' &&
        inputEncoding.toLowerCase() === 'hex' &&
        data.length % 2
      ) {
        fail('encoding', TypeError, 'ERR_INVALID_ARG_VALUE');
      }
      return Buffer.from(data, encoding(inputEncoding) || 'utf8');
    }
    if (!ArrayBuffer.isView(data)) {
      fail('data', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    return host.encoding.byteView(data);
  }

  Hash.prototype.update = function update(data, inputEncoding) {
    const state = stateFor(this);
    if (state.finalized) {
      finalized();
    }
    if (!state.handle.update(inputBytes(data, inputEncoding))) {
      fail('hash update', Error, 'ERR_CRYPTO_HASH_UPDATE_FAILED');
    }
    return this;
  };

  Hash.prototype.digest = function digest(outputEncoding) {
    const state = stateFor(this);
    if (state.finalized) {
      finalized();
    }
    const label = outputEncoding ? `${outputEncoding}` : undefined;
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

  Hash.prototype._transform = function (chunk, codec, callback) {
    try {
      stateFor(this).handle.update(inputBytes(chunk, codec));
    } catch (error) {
      callback(error);
      return;
    }
    callback();
  };

  Hash.prototype._flush = function (callback) {
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

  const hmacs = new WeakMap();

  function hmacState(value) {
    const state = hmacs.get(value);
    if (!state) {
      throw new TypeError('Invalid Hmac receiver');
    }
    return state;
  }

  function binaryBytes(value) {
    if (value === undefined) {
      fail('key or buffer', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    return host.encoding.byteView(value);
  }

  function keyEncoding(options) {
    // Like Node, inspect encoding even for binary keys, before validating key.
    const label = options == null ? undefined : options.encoding;
    if (label != null && typeof label !== 'string') {
      fail('options.encoding', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    return label;
  }

  /** Defer stream initialization like Node; key consumption and native handle creation are synchronous. */
  function Hmac(algorithm, key, options) {
    if (!new.target) {
      return new Hmac(algorithm, key, options);
    }
    if (typeof algorithm !== 'string') {
      fail('algorithm', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    const label = keyEncoding(options);
    let bytes;
    if (typeof key === 'string') {
      if (label && label !== 'buffer' && !Buffer.isEncoding(label)) {
        fail('encoding', TypeError, 'ERR_UNKNOWN_ENCODING');
      }
      bytes = Buffer.from(key, !label || label === 'buffer' ? 'utf8' : label);
    } else {
      bytes = binaryBytes(key);
    }
    const handle = native.hmac.create(algorithm, bytes);
    hmacs.set(this, { handle, finalized: false, options });
    // Node initializes crypto stream state only when stream methods need it.
    // Manual digest must not eagerly validate a readable output encoding.
  }

  Object.setPrototypeOf(Hmac.prototype, Transform.prototype);

  Object.setPrototypeOf(Hmac, Transform);

  for (const name of ['_readableState', '_writableState']) {
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
      fail('HMAC update', Error, 'ERR_CRYPTO_HASH_UPDATE_FAILED');
    }
    return this;
  };

  Hmac.prototype.digest = function digest(outputEncoding) {
    const state = hmacState(this);
    // Node returns empty output on repeated HMAC digest (unlike Hash).
    if (state.finalized) {
      return outputEncoding && outputEncoding !== 'buffer' ? '' : Buffer.alloc(0);
    }
    const label = outputEncoding ? `${outputEncoding}` : undefined;
    const value = buffer(state.handle.digest());
    state.finalized = true;
    const codec = encoding(label);
    return codec ? value.toString(codec) : value;
  };

  Hmac.prototype._transform = function (chunk, codec, callback) {
    try {
      this.update(chunk, codec);
    } catch (error) {
      callback(error);
      return;
    }
    callback();
  };

  Hmac.prototype._flush = function (callback) {
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

  /** Compare native byte ranges only; caller/type/length checks are outside that timing property. */
  function timingSafeEqual(left, right) {
    return native.hmac.equal(binaryBytes(left), binaryBytes(right));
  }

  return { createHash, Hash, createHmac, Hmac, timingSafeEqual };
}
