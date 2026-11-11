import createPromisify from './promisify.mjs';
import inherits from './inherits.mjs';
import createDebugLog from './debuglog.mjs';
import createInspection from './inspect.mjs';
import createDeprecate from './deprecate.mjs';

/**
 * TextEncoder and stateful TextDecoder facades. Native conversion owns codec state and byte-
 * view handling; this layer validates public options.
 *
 * Native dependencies are supplied by the embedding loader. One factory call
 * constructs the shared builtin instance for one runtime/realm.
 */
export default function createEncodingModule(host) {
  const native = host.native;
  const debuglog = createDebugLog(host);
  const promisify = createPromisify(host.promisifyState);
  const encoders = new WeakSet();
  const decoders = new WeakMap();

  function fail(code, message, Type = TypeError) {
    const error = new Type(message);
    error.code = code;
    throw error;
  }

  function options(value) {
    if (value == null) {
      return {};
    }
    if (typeof value !== 'object') {
      fail('ERR_INVALID_ARG_TYPE', 'options must be an object');
    }
    return value;
  }

  function encoder(value) {
    if (!encoders.has(value)) {
      fail('ERR_INVALID_THIS', 'Invalid TextEncoder receiver');
    }
  }

  function decoder(value) {
    const state = decoders.get(value);
    if (!state) {
      fail('ERR_INVALID_THIS', 'Invalid TextDecoder receiver');
    }
    return state;
  }

  class TextEncoder {
    constructor() {
      encoders.add(this);
    }

    get encoding() {
      encoder(this);
      return 'utf-8';
    }

    encode(input = '') {
      encoder(this);
      return native.encode(`${input}`.toWellFormed());
    }

    encodeInto(source, destination) {
      encoder(this);
      if (typeof source !== 'string') {
        fail('ERR_INVALID_ARG_TYPE', 'source must be a string');
      }
      return native.encodeInto(source, destination);
    }
  }
  /** Own a native streaming decoder per instance, preserving BOM/fatal/flush behavior. */
  class TextDecoder {
    constructor(label = 'utf-8', opts = undefined) {
      label = `${label}`;
      opts = options(opts);
      const encoding = native.standardize(label)?.toLowerCase();
      if (!encoding || encoding === 'replacement') {
        fail('ERR_ENCODING_NOT_SUPPORTED', `The encoding ${label} is not supported`, RangeError);
      }
      const fatal = !!opts.fatal;
      const ignoreBOM = !!opts.ignoreBOM;
      decoders.set(this, {
        encoding,
        fatal,
        ignoreBOM,
        decode: native.createDecoder(encoding, fatal, ignoreBOM)
      });
    }

    get encoding() {
      return decoder(this).encoding;
    }

    get fatal() {
      return decoder(this).fatal;
    }

    get ignoreBOM() {
      return decoder(this).ignoreBOM;
    }

    decode(input = undefined, opts = undefined) {
      const state = decoder(this);
      return state.decode(input, !!options(opts).stream);
    }
  }
  for (const [constructor, name] of [
    [TextEncoder, 'TextEncoder'],
    [TextDecoder, 'TextDecoder']
  ]) {
    Object.defineProperty(constructor.prototype, Symbol.toStringTag, {
      value: name,
      configurable: true
    });
    for (const property of Object.getOwnPropertyNames(constructor.prototype)) {
      if (property !== 'constructor') {
        Object.defineProperty(constructor.prototype, property, {
          ...Object.getOwnPropertyDescriptor(constructor.prototype, property),
          enumerable: true
        });
      }
    }
  }
  // The existing Promise timer implementation owns cancellation/ref/unref semantics.
  for (const name of ['setTimeout', 'setImmediate']) {
    Object.defineProperty(host.timers[name], promisify.custom, {
      get: () => host.timerPromises[name],

      enumerable: true
    });
  }
  return {
    TextEncoder,
    TextDecoder,
    promisify,
    inherits,
    deprecate: createDeprecate(host.process),
    debuglog,
    debug: debuglog,
    ...createInspection(host),
    ...(host.types ? { types: host.types } : {})
  };
}
