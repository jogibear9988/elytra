/**
 * URL and search-parameter facades backed by LibURL. Shared constructors keep global, ESM and
 * CommonJS URL identities consistent.
 *
 * Native dependencies are supplied by the embedding loader. One factory call
 * constructs the shared builtin instance for one runtime/realm.
 */
export default function createUrlModule(host) {
  const native = host.native;
  const urls = new WeakMap();
  const params = new WeakMap();

  function fail(code, message) {
    const error = new TypeError(message);
    error.code = code;
    throw error;
  }

  function usv(value) {
    return `${value}`.toWellFormed();
  }

  function state(map, object) {
    const value = map.get(object);
    if (!value) {
      if (map === urls) {
        throw new TypeError('Invalid URL receiver');
      }
      fail('ERR_INVALID_THIS', 'Invalid receiver');
    }
    return value;
  }

  function required(count, actual) {
    if (actual < count) {
      fail('ERR_MISSING_ARGS', 'Missing required argument');
    }
  }

  function invalid(input, base) {
    const error = new TypeError('Invalid URL');
    error.code = 'ERR_INVALID_URL';
    error.input = input;
    if (base !== undefined) {
      error.base = base;
    }
    throw error;
  }

  function updateParams(value) {
    if (value.url) {
      value.url.record = native.update(
        value.url.record.href,
        'search',
        native.encodeForm(value.list)
      );
    }
  }

  /** Keep parameters live with their associated URL while owning independent iterable state. */
  class URLSearchParams {
    constructor(init = '') {
      const value = { list: [], url: null };
      params.set(this, value);
      if (init == null) {
        return;
      }
      if (typeof init === 'object' || typeof init === 'function') {
        const iterator = init[Symbol.iterator];
        if (iterator !== undefined && iterator !== null) {
          if (typeof iterator !== 'function') {
            fail('ERR_ARG_NOT_ITERABLE', 'Query pairs must be iterable');
          }
          for (const pair of init) {
            if (
              pair === null ||
              (typeof pair !== 'object' && typeof pair !== 'function') ||
              typeof pair[Symbol.iterator] !== 'function'
            ) {
              fail('ERR_INVALID_TUPLE', 'Each query pair must be iterable with two items');
            }
            const items = Array.from(pair);
            if (items.length !== 2) {
              fail('ERR_INVALID_TUPLE', 'Each query pair must contain exactly two items');
            }
            value.list.push([usv(items[0]), usv(items[1])]);
          }
        } else {
          const names = new Map();
          for (const name of Reflect.ownKeys(init)) {
            if (!Object.getOwnPropertyDescriptor(init, name).enumerable) {
              continue;
            }
            const key = usv(name);
            const item = usv(init[name]);
            if (names.has(key)) {
              value.list[names.get(key)][1] = item;
            } else {
              names.set(key, value.list.length);
              value.list.push([key, item]);
            }
          }
        }
      } else {
        const text = usv(init);
        value.list = native.decodeForm(text.startsWith('?') ? text.slice(1) : text);
      }
    }

    get size() {
      return state(params, this).list.length;
    }

    append(name, value) {
      const s = state(params, this);
      required(2, arguments.length);
      s.list.push([usv(name), usv(value)]);
      updateParams(s);
    }

    delete(name, value) {
      const s = state(params, this);
      required(1, arguments.length);
      name = usv(name);
      if (value !== undefined) {
        value = usv(value);
      }
      s.list = s.list.filter(
        (pair) => pair[0] !== name || (value !== undefined && pair[1] !== value)
      );
      updateParams(s);
    }

    get(name) {
      const s = state(params, this);
      required(1, arguments.length);
      name = usv(name);
      return s.list.find((pair) => pair[0] === name)?.[1] ?? null;
    }

    getAll(name) {
      const s = state(params, this);
      required(1, arguments.length);
      name = usv(name);
      return s.list.filter((pair) => pair[0] === name).map((pair) => pair[1]);
    }

    has(name, value) {
      const s = state(params, this);
      required(1, arguments.length);
      name = usv(name);
      if (value !== undefined) {
        value = usv(value);
      }
      return s.list.some((pair) => pair[0] === name && (value === undefined || pair[1] === value));
    }

    set(name, value) {
      const s = state(params, this);
      required(2, arguments.length);
      name = usv(name);
      value = usv(value);
      let found = false;
      s.list = s.list.filter((pair) => {
        if (pair[0] !== name) {
          return true;
        }
        if (found) {
          return false;
        }
        pair[1] = value;
        found = true;
        return true;
      });
      if (!found) {
        s.list.push([name, value]);
      }
      updateParams(s);
    }

    sort() {
      const s = state(params, this);

      s.list.sort((a, b) => (a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0));

      updateParams(s);
    }

    entries() {
      return iterator(state(params, this), 'entries');
    }

    keys() {
      return iterator(state(params, this), 'keys');
    }

    values() {
      return iterator(state(params, this), 'values');
    }

    forEach(callback, thisArg) {
      const s = state(params, this);
      if (typeof callback !== 'function') {
        fail('ERR_INVALID_ARG_TYPE', 'callback must be a function');
      }
      for (let i = 0; i < s.list.length; ++i) {
        callback.call(thisArg, s.list[i][1], s.list[i][0], this);
      }
    }

    toString() {
      return native.encodeForm(state(params, this).list);
    }
  }
  const iterators = new WeakMap();
  const iteratorPrototype = Object.create(
    Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()))
  );
  Object.defineProperty(iteratorPrototype, Symbol.toStringTag, {
    value: 'URLSearchParams Iterator',
    configurable: true
  });
  Object.defineProperty(iteratorPrototype, 'next', {
    enumerable: true,
    configurable: true,
    writable: true,

    value: function next() {
      const s = state(iterators, this);
      if (s.index >= s.source.list.length) {
        return { value: undefined, done: true };
      }
      const pair = s.source.list[s.index++];
      return {
        value: s.kind === 'keys' ? pair[0] : s.kind === 'values' ? pair[1] : pair.slice(),
        done: false
      };
    }
  });

  function iterator(source, kind) {
    const value = Object.create(iteratorPrototype);
    iterators.set(value, { source, kind, index: 0 });
    return value;
  }

  URLSearchParams.prototype[Symbol.iterator] = URLSearchParams.prototype.entries;
  Object.defineProperty(URLSearchParams.prototype, Symbol.toStringTag, {
    value: 'URLSearchParams',
    configurable: true
  });
  /** Native parsing backs shared global/builtin constructors; setters update their live parameter view. */
  class URL {
    constructor(input, base) {
      required(1, arguments.length);
      input = usv(input);
      if (base !== undefined) {
        base = usv(base);
      }
      const record = native.parse(input, base);
      if (!record) {
        invalid(input, base);
      }
      const searchParams = new URLSearchParams(record.search);
      const value = { record, searchParams };
      urls.set(this, value);
      Object.assign(params.get(searchParams), { url: value, owner: this });
    }

    static canParse(input, base) {
      required(1, arguments.length);
      input = usv(input);
      if (base !== undefined) {
        base = usv(base);
      }
      return native.parse(input, base) !== null;
    }

    static parse(input, base) {
      required(1, arguments.length);
      input = usv(input);
      if (base !== undefined) {
        base = usv(base);
      }
      const result = native.parse(input, base);
      return result ? new URL(result.href) : null;
    }

    get searchParams() {
      return state(urls, this).searchParams;
    }

    get origin() {
      return state(urls, this).record.origin;
    }

    toString() {
      const s = urls.get(this);
      if (!s) {
        throw new TypeError('Invalid URL receiver');
      }
      return s.record.href;
    }

    toJSON() {
      return state(urls, this).record.href;
    }
  }
  for (const name of [
    'href',
    'protocol',
    'username',
    'password',
    'host',
    'hostname',
    'port',
    'pathname',
    'search',
    'hash'
  ]) {
    Object.defineProperty(URL.prototype, name, {
      enumerable: true,
      configurable: true,

      get() {
        return state(urls, this).record[name];
      },

      set(input) {
        const s = state(urls, this);
        input = usv(input);
        const value =
          name === 'href' ? native.parse(input) : native.update(s.record.href, name, input);
        if (!value) {
          invalid(input);
        }
        s.record = value;
        if (name === 'href' || name === 'search') {
          params.get(s.searchParams).list = native.decodeForm(value.search.slice(1));
        }
      }
    });
  }
  Object.defineProperty(URL.prototype, Symbol.toStringTag, { value: 'URL', configurable: true });
  for (const prototype of [URL.prototype, URLSearchParams.prototype]) {
    for (const name of Object.getOwnPropertyNames(prototype)) {
      if (name !== 'constructor') {
        Object.defineProperty(prototype, name, {
          ...Object.getOwnPropertyDescriptor(prototype, name),
          enumerable: true
        });
      }
    }
  }
  const Buffer = host.Buffer;

  function fileURL(input) {
    if (typeof input === 'string') {
      input = new URL(input);
    } else if (!urls.has(input)) {
      fail('ERR_INVALID_ARG_TYPE', 'path must be a string or URL');
    }
    if (input.protocol !== 'file:') {
      fail('ERR_INVALID_URL_SCHEME', 'URL must use the file scheme');
    }
    return input;
  }

  function windowsOption(options) {
    return options?.windows ?? host.platform === 'win32';
  }

  function validateFileURL(input, windows, raw = false) {
    const url = fileURL(input);
    if (!windows && url.hostname !== '') {
      fail('ERR_INVALID_FILE_URL_HOST', 'File URL host must be localhost or empty');
    }
    if ((windows || !raw) && (windows ? /%2f|%5c/i : /%2f/i).test(url.pathname)) {
      invalidFilePath(url);
    }
    return url;
  }

  function invalidFilePath(url) {
    const error = new TypeError('Invalid file URL path');
    error.code = 'ERR_INVALID_FILE_URL_PATH';
    error.input = url.href;
    throw error;
  }

  function fileURLToPath(input, options) {
    const windows = windowsOption(options);
    const url = validateFileURL(input, windows);
    let pathname = decodeURIComponent(url.pathname);
    if (!windows) {
      return pathname;
    }
    pathname = pathname.replaceAll('/', '\\');
    if (url.hostname) {
      return '\\\\' + native.unicodeHost(url.hostname) + pathname;
    }
    if (!/^\\[a-zA-Z]:/.test(pathname)) {
      invalidFilePath(url);
    }
    return pathname.slice(1);
  }

  function fileURLToPathBuffer(input, options) {
    const windows = windowsOption(options);
    const url = validateFileURL(input, windows, true);
    const raw = native.decodePath(url.pathname);
    const pathname = Buffer.from(raw.buffer, raw.byteOffset, raw.byteLength);
    if (!windows) {
      return pathname;
    }
    for (let i = 0; i < pathname.length; ++i) {
      if (pathname[i] === 47) {
        pathname[i] = 92;
      }
    }
    if (url.hostname) {
      return Buffer.concat([Buffer.from('\\\\' + native.unicodeHost(url.hostname)), pathname]);
    }
    if (
      pathname.length < 3 ||
      pathname[0] !== 92 ||
      !((pathname[1] >= 65 && pathname[1] <= 90) || (pathname[1] >= 97 && pathname[1] <= 122)) ||
      pathname[2] !== 58
    ) {
      invalidFilePath(url);
    }
    return pathname.subarray(1);
  }

  function pathToFileURL(path, options) {
    if (typeof path !== 'string') {
      fail('ERR_INVALID_ARG_TYPE', 'path must be a string');
    }
    const windows = windowsOption(options);

    function encode(value) {
      return encodeURI(value.toWellFormed()).replace(
        /[?#~]/g,
        (c) => '%' + c.charCodeAt(0).toString(16).toUpperCase()
      );
    }

    function unc(value) {
      const slash = value.indexOf('/', 2);
      if (slash < 0) {
        fail('ERR_INVALID_ARG_VALUE', 'UNC path must contain a resource path');
      }
      const hostname = native.asciiHost(value.slice(2, slash));
      if (!hostname) {
        fail('ERR_INVALID_ARG_VALUE', 'Invalid UNC hostname');
      }
      return new URL('file://' + hostname + encode(value.slice(slash)));
    }

    if (windows && path.startsWith('\\\\?\\')) {
      path = path.slice(4);
      if (path.startsWith('UNC\\')) {
        return unc('//' + path.slice(4).replaceAll('\\', '/'));
      }
      return new URL('file:///' + encode(path.replaceAll('\\', '/')));
    }
    if (windows && path.startsWith('\\\\')) {
      return unc(path.replaceAll('\\', '/'));
    }
    let resolved = (windows ? host.path.win32 : host.path.posix).resolve(path);
    if (windows && resolved.startsWith('\\\\')) {
      return unc(resolved.replaceAll('\\', '/'));
    }
    const trailing = path.endsWith('/') || (windows && path.endsWith('\\'));
    if (trailing && !resolved.endsWith(host.path.sep)) {
      resolved += '/';
    }
    if (windows) {
      resolved = resolved.replaceAll('\\', '/');
    }
    return new URL('file://' + (resolved.startsWith('/') ? '' : '/') + encode(resolved));
  }

  function domainToASCII(domain) {
    required(1, arguments.length);
    domain = usv(domain);
    const authority = domain
      .replaceAll(String.fromCharCode(9), '')
      .replaceAll(String.fromCharCode(10), '')
      .replaceAll(String.fromCharCode(13), '')
      .split(/[/?#\\]/, 1)[0];
    return native.asciiHost(authority) ?? '';
  }

  function domainToUnicode(domain) {
    required(1, arguments.length);
    const ascii = domainToASCII(domain);
    return ascii ? native.unicodeHost(ascii) : '';
  }

  function urlToHttpOptions(url) {
    if (url === null || typeof url !== 'object') {
      fail('ERR_INVALID_ARG_TYPE', 'url must be an object');
    }
    const options = {
      __proto__: null,
      ...url,
      protocol: url.protocol,
      hostname: url.hostname,
      hash: url.hash,
      search: url.search,
      pathname: url.pathname,
      path: (url.pathname || '') + (url.search || ''),
      href: url.href
    };
    if (options.hostname?.startsWith('[')) {
      options.hostname = options.hostname.slice(1, -1);
    }
    if (url.port !== '') {
      options.port = Number(url.port);
    }
    if (url.username || url.password) {
      options.auth = decodeURIComponent(url.username) + ':' + decodeURIComponent(url.password);
    }
    return options;
  }

  return {
    URL,
    URLSearchParams,
    fileURLToPath,
    fileURLToPathBuffer,
    pathToFileURL,
    domainToASCII,
    domainToUnicode,
    urlToHttpOptions
  };
}
