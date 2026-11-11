/** Preserve callable/constructor behavior while issuing one real asynchronous warning. */
export default function createDeprecate(process) {
  const codes = new Set();
  const apply = Reflect.apply;
  const construct = Reflect.construct;

  function invalid(message) {
    throw Object.assign(new TypeError(message), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  return function deprecate(fn, message, code, options = {}) {
    if (typeof fn !== 'function') {
      invalid('fn must be a function');
    }
    if (code !== undefined && typeof code !== 'string') {
      invalid('code must be a string');
    }
    if (!options || typeof options !== 'object' || Array.isArray(options)) {
      invalid('options must be an object');
    }
    for (const key of Object.keys(options)) {
      if (key !== 'modifyPrototype') {
        throw Object.assign(new Error(`util.deprecate option ${key} is not implemented`), {
          code: 'ERR_MITE_UNSUPPORTED_API'
        });
      }
    }
    const modifyPrototype = options.modifyPrototype === undefined ? true : options.modifyPrototype;
    if (typeof modifyPrototype !== 'boolean') {
      invalid('modifyPrototype must be a boolean');
    }
    let warned = false;

    function deprecated(...args) {
      if (!process.noDeprecation && !warned) {
        warned = true;
        if (code === 'ExperimentalWarning') {
          process.emitWarning(message, code, deprecated);
        } else if (code === undefined) {
          process.emitWarning(message, 'DeprecationWarning', deprecated);
        } else if (!codes.has(code)) {
          process.emitWarning(message, 'DeprecationWarning', code, deprecated);
          codes.add(code);
        }
      }
      return new.target ? construct(fn, args, new.target) : apply(fn, this, args);
    }

    if (modifyPrototype) {
      Object.setPrototypeOf(deprecated, fn);
      if (fn.prototype) {
        deprecated.prototype = fn.prototype;
      }
      const length = Object.getOwnPropertyDescriptor(fn, 'length');
      if (length) {
        Object.defineProperty(deprecated, 'length', length);
      }
    }
    return deprecated;
  };
}
