/* Generated from mite/modules/core/util.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/promisify.mjs
function createPromisify(state) {
  const customSymbol = Symbol.for("nodejs.util.promisify.custom");
  const argumentNamesSymbol = state.argumentNames = Symbol("customPromisifyArgs");
  function promisify(original) {
    if (typeof original !== "function") {
      throw invalid("original must be a function");
    }
    if (original[customSymbol]) {
      const custom = original[customSymbol];
      if (typeof custom !== "function") {
        throw invalid("promisify.custom must be a function");
      }
      Object.defineProperty(custom, customSymbol, {
        value: custom,
        configurable: true,
        enumerable: false,
        writable: false
      });
      return custom;
    }
    const names = original[argumentNamesSymbol];
    function wrapped(...args) {
      return new Promise((resolve, reject) => {
        args.push((error, ...values) => {
          if (error) {
            reject(error);
          } else {
            resolve(
              names && values.length > 1 ? Object.fromEntries(names.map((name, index) => [name, values[index]])) : values[0]
            );
          }
        });
        Reflect.apply(original, this, args);
      });
    }
    Object.setPrototypeOf(wrapped, Object.getPrototypeOf(original));
    Object.defineProperty(wrapped, customSymbol, {
      value: wrapped,
      configurable: true,
      enumerable: false,
      writable: false
    });
    return Object.defineProperties(wrapped, Object.getOwnPropertyDescriptors(original));
  }
  function invalid(message) {
    return Object.assign(new TypeError(message), { code: "ERR_INVALID_ARG_TYPE" });
  }
  promisify.custom = customSymbol;
  return promisify;
}

// mite/modules/core/inherits.mjs
function inherits(ctor, superCtor) {
  function invalid(name) {
    throw Object.assign(new TypeError(`${name} is not a supported inheritance target`), {
      code: "ERR_INVALID_ARG_TYPE"
    });
  }
  if (ctor === null || ctor === void 0) {
    invalid("ctor");
  }
  if (superCtor === null || superCtor === void 0) {
    invalid("superCtor");
  }
  if (superCtor.prototype === void 0) {
    invalid("superCtor.prototype");
  }
  Object.defineProperty(ctor, "super_", {
    value: superCtor,
    writable: true,
    configurable: true
  });
  Object.setPrototypeOf(ctor.prototype, superCtor.prototype);
}

// mite/modules/core/util.mjs
function createEncodingModule(host) {
  const native = host.native;
  const promisify = createPromisify(host.promisifyState);
  const encoders = /* @__PURE__ */ new WeakSet();
  const decoders = /* @__PURE__ */ new WeakMap();
  function fail(code, message, Type = TypeError) {
    const error = new Type(message);
    error.code = code;
    throw error;
  }
  function options(value) {
    if (value == null) {
      return {};
    }
    if (typeof value !== "object") {
      fail("ERR_INVALID_ARG_TYPE", "options must be an object");
    }
    return value;
  }
  function encoder(value) {
    if (!encoders.has(value)) {
      fail("ERR_INVALID_THIS", "Invalid TextEncoder receiver");
    }
  }
  function decoder(value) {
    const state = decoders.get(value);
    if (!state) {
      fail("ERR_INVALID_THIS", "Invalid TextDecoder receiver");
    }
    return state;
  }
  class TextEncoder {
    constructor() {
      encoders.add(this);
    }
    get encoding() {
      encoder(this);
      return "utf-8";
    }
    encode(input = "") {
      encoder(this);
      return native.encode(`${input}`.toWellFormed());
    }
    encodeInto(source, destination) {
      encoder(this);
      if (typeof source !== "string") {
        fail("ERR_INVALID_ARG_TYPE", "source must be a string");
      }
      return native.encodeInto(source, destination);
    }
  }
  class TextDecoder {
    constructor(label = "utf-8", opts = void 0) {
      label = `${label}`;
      opts = options(opts);
      const encoding = native.standardize(label)?.toLowerCase();
      if (!encoding || encoding === "replacement") {
        fail("ERR_ENCODING_NOT_SUPPORTED", `The encoding ${label} is not supported`, RangeError);
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
    decode(input = void 0, opts = void 0) {
      const state = decoder(this);
      return state.decode(input, !!options(opts).stream);
    }
  }
  for (const [constructor, name] of [
    [TextEncoder, "TextEncoder"],
    [TextDecoder, "TextDecoder"]
  ]) {
    Object.defineProperty(constructor.prototype, Symbol.toStringTag, {
      value: name,
      configurable: true
    });
    for (const property of Object.getOwnPropertyNames(constructor.prototype)) {
      if (property !== "constructor") {
        Object.defineProperty(constructor.prototype, property, {
          ...Object.getOwnPropertyDescriptor(constructor.prototype, property),
          enumerable: true
        });
      }
    }
  }
  for (const name of ["setTimeout", "setImmediate"]) {
    Object.defineProperty(host.timers[name], promisify.custom, {
      get: () => host.timerPromises[name],
      enumerable: true
    });
  }
  return { TextEncoder, TextDecoder, promisify, inherits };
}
return createEncodingModule;
})()
