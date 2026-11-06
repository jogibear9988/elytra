/** Fetch Request/Response dictionaries and realm-local brands; no transport lives here. */
export default function createObjects(host, headers, bodies) {
  const { Headers } = headers;
  const requests = new WeakMap();
  const responses = new WeakMap();
  const token = /^[!#$%&'*+.^_`|~0-9A-Za-z-]+$/;
  const emptyStatuses = new Set([101, 103, 204, 205, 304]);
  const redirects = new Set([301, 302, 303, 307, 308]);

  function dictionary(value) {
    if (value === null || value === undefined) {
      return {};
    }
    if (typeof value !== 'object' && typeof value !== 'function') {
      throw new TypeError('Expected an options dictionary');
    }
    return value;
  }

  function state(map, value) {
    const result = map.get(value);
    if (!result) {
      throw new TypeError('Invalid Fetch object receiver');
    }
    return result;
  }

  function choice(value, fallback, values, name) {
    const text = value === undefined ? fallback : String(value);
    if (!values.includes(text)) {
      throw new TypeError(`Invalid ${name}`);
    }
    return text;
  }

  function statusText(value) {
    const text = value === undefined ? '' : String(value);
    if (/[^\t\x20-\x7E\x80-\xFF]/.test(text)) {
      throw new TypeError('Invalid statusText');
    }
    return text;
  }

  function initializeBody(body, target) {
    if (body.type !== null && !target.has('content-type')) {
      target.set('content-type', body.type);
    }
    return body;
  }

  function properties(Type, map, names) {
    for (const name of names) {
      Object.defineProperty(Type.prototype, name, {
        configurable: true,
        enumerable: true,

        get() {
          return state(map, this)[name];
        }
      });
    }
    Object.defineProperty(Type.prototype, Symbol.toStringTag, {
      value: Type.name,
      configurable: true
    });

    bodies.install(Type, (value) => state(map, value));
  }

  class Request {
    constructor(input, init = undefined) {
      if (!arguments.length) {
        throw new TypeError('Request requires input');
      }
      const opts = dictionary(init);
      const parent = requests.get(input);
      const url = new host.URL(parent?.url ?? String(input));
      if (url.username || url.password) {
        throw new TypeError('Request URLs cannot contain credentials');
      }
      let method = String(opts.method === undefined ? (parent?.method ?? 'GET') : opts.method);
      if (!token.test(method) || ['CONNECT', 'TRACE', 'TRACK'].includes(method.toUpperCase())) {
        throw new TypeError('Invalid or forbidden method');
      }
      if (['DELETE', 'GET', 'HEAD', 'OPTIONS', 'POST', 'PUT'].includes(method.toUpperCase())) {
        method = method.toUpperCase();
      }
      const entry = {
        url: url.href,
        method,
        headers: new Headers(opts.headers === undefined ? parent?.headers : opts.headers),
        redirect: choice(
          opts.redirect,
          parent?.redirect ?? 'follow',
          ['follow', 'error', 'manual'],
          'redirect'
        ),
        mode: choice(opts.mode, parent?.mode ?? 'cors', ['cors', 'same-origin', 'no-cors'], 'mode'),
        credentials: choice(
          opts.credentials,
          parent?.credentials ?? 'same-origin',
          ['omit', 'same-origin', 'include'],
          'credentials'
        ),
        cache: choice(
          opts.cache,
          parent?.cache ?? 'default',
          ['default', 'no-store', 'reload', 'no-cache', 'force-cache', 'only-if-cached'],
          'cache'
        ),
        referrer: opts.referrer ?? parent?.referrer ?? 'about:client',
        referrerPolicy: opts.referrerPolicy ?? parent?.referrerPolicy ?? '',
        integrity: opts.integrity ?? parent?.integrity ?? '',
        keepalive: Boolean(opts.keepalive ?? parent?.keepalive ?? false),
        destination: '',
        duplex: 'half',
        isReloadNavigation: false,
        isHistoryNavigation: false
      };
      if (entry.mode === 'no-cors') {
        throw host.unsupported('Fetch no-cors guards');
      }
      if (!['default', 'no-store'].includes(entry.cache)) {
        throw host.unsupported('Fetch cache/revalidation');
      }
      if (entry.referrer !== 'about:client' || entry.referrerPolicy !== '') {
        throw host.unsupported('Fetch referrer policy');
      }
      if (entry.integrity !== '') {
        throw host.unsupported('Fetch integrity verification');
      }
      if (opts.window !== undefined && opts.window !== null) {
        throw new TypeError('window must be null');
      }
      if (opts.duplex !== undefined && opts.duplex !== 'half') {
        throw new TypeError('duplex must be half');
      }
      const signal = opts.signal === undefined ? parent?.signal : opts.signal;
      if (signal !== undefined && signal !== null && !host.abort.isSignal(signal)) {
        throw new TypeError('signal must be an AbortSignal');
      }
      entry.signal = host.abort.AbortSignal.any(signal == null ? [] : [signal]);
      const hasBody = opts.body !== undefined && opts.body !== null;
      const body = hasBody ? bodies.extract(opts.body) : (parent?.body ?? bodies.extract(null));
      if ((method === 'GET' || method === 'HEAD') && body.stream !== null) {
        throw new TypeError('GET/HEAD cannot have a body');
      }
      if (hasBody && bodies.isStream(opts.body) && opts.duplex === undefined) {
        throw new TypeError('Stream bodies require duplex: half');
      }
      if (entry.keepalive && body.stream !== null && body.source === null) {
        throw new TypeError('keepalive does not support stream bodies');
      }
      entry.body = initializeBody(parent && !hasBody ? bodies.transfer(body) : body, entry.headers);
      requests.set(this, entry);
    }

    clone() {
      const entry = state(requests, this);
      const result = Object.create(Request.prototype);
      requests.set(result, {
        ...entry,
        headers: new Headers(entry.headers),
        signal: host.abort.AbortSignal.any([entry.signal]),
        body: bodies.clone(entry.body)
      });
      return result;
    }
  }

  class Response {
    constructor(body = null, init = undefined) {
      const opts = dictionary(init);
      const status = opts.status === undefined ? 200 : Math.trunc(Number(opts.status));
      if (!Number.isInteger(status) || status < 200 || status > 599) {
        throw new RangeError('Response status must be 200..599');
      }
      const target = new Headers(opts.headers);
      const extracted = bodies.extract(body);
      if (emptyStatuses.has(status) && extracted.stream !== null) {
        throw new TypeError('This status cannot have a body');
      }
      responses.set(this, {
        status,
        statusText: statusText(opts.statusText),
        headers: target,
        body: initializeBody(extracted, target),
        type: 'default',
        url: '',
        redirected: false
      });
    }

    get ok() {
      const { status } = state(responses, this);
      return status >= 200 && status <= 299;
    }

    clone() {
      const entry = state(responses, this);
      const result = Object.create(Response.prototype);
      const target = new Headers(entry.headers);
      if (headers.isImmutable(entry.headers)) {
        headers.immutable(target);
      }
      responses.set(result, { ...entry, headers: target, body: bodies.clone(entry.body) });
      return result;
    }

    static error() {
      const result = Object.create(Response.prototype);
      responses.set(result, {
        status: 0,
        statusText: '',
        headers: headers.immutable(new Headers()),
        body: bodies.extract(null),
        type: 'error',
        url: '',
        redirected: false
      });
      return result;
    }

    static redirect(url, status = 302) {
      status = Number(status);
      if (!redirects.has(status)) {
        throw new RangeError('Invalid redirect status');
      }
      const result = new Response(null, {
        status,
        headers: { location: new host.URL(String(url)).href }
      });
      headers.immutable(state(responses, result).headers);
      return result;
    }

    static json(data, init = undefined) {
      const text = JSON.stringify(data);
      if (text === undefined) {
        throw new TypeError('JSON data cannot be undefined');
      }
      const opts = dictionary(init);
      const target = new Headers(opts.headers);
      if (!target.has('content-type')) {
        target.set('content-type', 'application/json');
      }
      return new Response(text, { ...opts, headers: target });
    }
  }
  properties(Request, requests, [
    'url',
    'method',
    'headers',
    'redirect',
    'mode',
    'credentials',
    'cache',
    'referrer',
    'referrerPolicy',
    'integrity',
    'keepalive',
    'destination',
    'duplex',
    'signal',
    'isReloadNavigation',
    'isHistoryNavigation'
  ]);
  properties(Response, responses, ['status', 'statusText', 'headers', 'type', 'url', 'redirected']);
  return {
    Request,
    Response,

    requestState: (value) => state(requests, value),

    networkResponse(stream, metadata) {
      const result = Object.create(Response.prototype);
      responses.set(result, {
        ...metadata,
        headers: headers.immutable(metadata.headers),
        body: bodies.extract(stream)
      });
      return result;
    }
  };
}
