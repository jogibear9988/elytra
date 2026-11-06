/** Fetch header lists preserve Set-Cookie entries and enforce response immutability. */
export default function createHeaders() {
  const states = new WeakMap();
  const token = /^[!#$%&'*+.^_`|~0-9A-Za-z-]+$/;

  function bytes(value) {
    if (typeof value === 'symbol') {
      throw new TypeError('Cannot convert Symbol to a header string');
    }
    const text = String(value);
    for (let index = 0; index < text.length; index++) {
      if (text.charCodeAt(index) > 255) {
        throw new TypeError('Header strings must contain bytes');
      }
    }
    return text;
  }

  function name(value) {
    const text = bytes(value);
    if (!token.test(text)) {
      throw new TypeError('Invalid header name');
    }
    return text.toLowerCase();
  }

  function value(input) {
    const text = bytes(input).replace(/^[\t\n\r ]+|[\t\n\r ]+$/g, '');
    if (/[\0\r\n]/.test(text)) {
      throw new TypeError('Invalid header value');
    }
    return text;
  }

  function state(headers, mutate = false) {
    const entry = states.get(headers);
    if (!entry) {
      throw new TypeError('Expected Headers receiver');
    }
    if (mutate && entry.immutable) {
      throw new TypeError('Headers are immutable');
    }
    return entry;
  }

  function pairs(headers) {
    const grouped = new Map();
    for (const [key, value] of state(headers).list) {
      const values = grouped.get(key) ?? [];
      values.push(value);
      grouped.set(key, values);
    }
    return [...grouped.keys()]
      .sort()
      .flatMap((key) =>
        key === 'set-cookie'
          ? grouped.get(key).map((value) => [key, value])
          : [[key, grouped.get(key).join(key === 'cookie' ? '; ' : ', ')]]
      );
  }

  class Headers {
    constructor(init = undefined) {
      states.set(this, { list: [], immutable: false });
      if (init === undefined) {
        return;
      }
      if (init === null || (typeof init !== 'object' && typeof init !== 'function')) {
        throw new TypeError('Header initializer must be a record or sequence');
      }
      if (init[Symbol.iterator] !== undefined) {
        for (const pair of init) {
          if (
            !pair ||
            (typeof pair !== 'object' && typeof pair !== 'function') ||
            typeof pair[Symbol.iterator] !== 'function'
          ) {
            throw new TypeError('Invalid header pair');
          }
          const values = [...pair];
          if (values.length !== 2) {
            throw new TypeError('Header pair must have two entries');
          }
          this.append(values[0], values[1]);
        }
      } else {
        for (const key of Object.keys(init)) {
          this.append(key, init[key]);
        }
      }
    }

    append(key, input) {
      if (arguments.length < 2) {
        throw new TypeError('append requires name and value');
      }
      const field = name(key);
      const text = value(input);
      state(this, true).list.push([field, text]);
    }

    set(key, input) {
      if (arguments.length < 2) {
        throw new TypeError('set requires name and value');
      }
      const field = name(key);
      const text = value(input);
      const entry = state(this, true);
      const index = entry.list.findIndex(([key]) => key === field);
      entry.list = entry.list.filter(([key]) => key !== field);
      entry.list.splice(index < 0 ? entry.list.length : index, 0, [field, text]);
    }

    delete(key) {
      if (!arguments.length) {
        throw new TypeError('delete requires a name');
      }
      const field = name(key);
      const entry = state(this, true);
      entry.list = entry.list.filter(([key]) => key !== field);
    }

    get(key) {
      if (!arguments.length) {
        throw new TypeError('get requires a name');
      }
      const field = name(key);
      const values = state(this)
        .list.filter(([key]) => key === field)
        .map(([, value]) => value);
      return values.length ? values.join(field === 'cookie' ? '; ' : ', ') : null;
    }

    has(key) {
      if (!arguments.length) {
        throw new TypeError('has requires a name');
      }
      const field = name(key);
      return state(this).list.some(([key]) => key === field);
    }

    getSetCookie() {
      return state(this)
        .list.filter(([key]) => key === 'set-cookie')
        .map(([, value]) => value);
    }

    *entries() {
      state(this);
      // Recompute after each yield: changes during iteration remain observable.
      for (let index = 0; ; index++) {
        const list = pairs(this);
        if (index >= list.length) {
          return;
        }
        yield list[index];
      }
    }

    *keys() {
      for (const [key] of this.entries()) {
        yield key;
      }
    }

    *values() {
      for (const [, value] of this.entries()) {
        yield value;
      }
    }

    forEach(callback, thisArg = undefined) {
      state(this);
      if (typeof callback !== 'function') {
        throw new TypeError('callback must be a function');
      }
      for (const [key, value] of this.entries()) {
        callback.call(thisArg, value, key, this);
      }
    }
  }
  Headers.prototype[Symbol.iterator] = Headers.prototype.entries;
  Object.defineProperty(Headers.prototype, Symbol.toStringTag, {
    value: 'Headers',
    configurable: true
  });
  return {
    Headers,

    isImmutable: (headers) => state(headers).immutable,

    immutable(headers) {
      state(headers).immutable = true;
      return headers;
    }
  };
}
