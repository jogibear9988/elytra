/** Adapt error-first callbacks without changing the caller's receiver or exception values. */
export default function createPromisify(state) {
  const customSymbol = Symbol.for('nodejs.util.promisify.custom');
  const argumentNamesSymbol = (state.argumentNames = Symbol('customPromisifyArgs'));

  function promisify(original) {
    if (typeof original !== 'function') {
      throw invalid('original must be a function');
    }
    if (original[customSymbol]) {
      const custom = original[customSymbol];
      if (typeof custom !== 'function') {
        throw invalid('promisify.custom must be a function');
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
              names && values.length > 1
                ? Object.fromEntries(names.map((name, index) => [name, values[index]]))
                : values[0]
            );
          }
        });
        // Promise construction captures synchronous throws; normal return values
        // are deliberately ignored because callback completion owns settlement.
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
    // Preserve name/length, symbols, accessors and descriptors without invoking getters.
    // As in Node, an existing falsy custom property is copied over the self marker.
    return Object.defineProperties(wrapped, Object.getOwnPropertyDescriptors(original));
  }

  function invalid(message) {
    return Object.assign(new TypeError(message), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  promisify.custom = customSymbol;
  return promisify;
}
