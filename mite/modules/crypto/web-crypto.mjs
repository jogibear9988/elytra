/** WebCrypto randomness uses native integer-view validation and fresh OS entropy, never Math.random. */
export default function createWebCrypto(host) {
  const native = host.native;
  const DOMException = host.DOMException;
  const fill = native.webRandomFill.bind(native);
  const uuid = native.uuid.bind(native);

  function invalid(code, message) {
    return Object.assign(new TypeError(message), { code });
  }

  class Crypto {
    constructor() {
      throw invalid('ERR_ILLEGAL_CONSTRUCTOR', 'Illegal constructor');
    }

    getRandomValues(view) {
      if (this !== webcrypto) {
        throw invalid('ERR_INVALID_THIS', 'Value must be a Crypto');
      }
      if (arguments.length === 0) {
        throw invalid('ERR_MISSING_ARGS', 'The typedArray argument must be specified');
      }
      try {
        return fill(view);
      } catch (failure) {
        if (failure.code === 'ERR_MITE_WEB_CRYPTO_TYPE') {
          throw new DOMException('Expected an integer TypedArray', 'TypeMismatchError');
        }
        if (failure.code === 'ERR_MITE_WEB_CRYPTO_QUOTA') {
          throw new DOMException('Random values exceed 65536 bytes', 'QuotaExceededError');
        }
        throw failure;
      }
    }

    randomUUID() {
      if (this !== webcrypto) {
        throw invalid('ERR_INVALID_THIS', 'Value must be a Crypto');
      }
      return uuid();
    }

    get subtle() {
      if (this !== webcrypto) {
        throw invalid('ERR_INVALID_THIS', 'Value must be a Crypto');
      }
      throw Object.assign(Error('WebCrypto subtle algorithms are not implemented'), {
        code: 'ERR_MITE_UNSUPPORTED_API'
      });
    }
  }

  for (const name of ['getRandomValues', 'randomUUID', 'subtle']) {
    const descriptor = Object.getOwnPropertyDescriptor(Crypto.prototype, name);
    Object.defineProperty(Crypto.prototype, name, { ...descriptor, enumerable: true });
  }
  Object.defineProperty(Crypto.prototype, Symbol.toStringTag, {
    value: 'Crypto',
    configurable: true
  });
  const webcrypto = Object.create(Crypto.prototype);
  const randomValues = Crypto.prototype.getRandomValues;
  // Node's top-level helper is callable unbound; the WebCrypto method retains its receiver check.
  function getRandomValues(view) {
    return arguments.length ? randomValues.call(webcrypto, view) : randomValues.call(webcrypto);
  }

  return { webcrypto, getRandomValues };
}
