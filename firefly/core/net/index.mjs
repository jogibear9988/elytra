/** Stateless HTTP(S) desktop requests over captured Mite transports, with explicit proxy/session gaps. */
export default function createNet(
  native,
  isReady,
  isExited,
  error,
  unsupported,
  transport,
  protocols = () => null,
  requestPolicy = () => null
) {
  if (!transport) {
    // Hosts without captured primitives cannot issue requests or consult mutable globals.
    const fail = () => unsupported('net captured transport');

    let stopped = false;
    return {
      net: {
        fetch: async () => fail(),

        request: fail,
        isOnline: fail,
        resolveHost: fail,

        get online() {
          return fail();
        }
      },

      stop() {
        if (!stopped) {
          stopped = true;
          try {
            native.networkStop?.();
          } catch {
            /* Optional component teardown is best effort. */
          }
        }
      }
    };
  }
  const {
    fetch: fetchTransport,
    Request,
    Response,
    Headers,
    URL,
    AbortController,
    ReadableStream,
    Readable,
    Writable,
    Buffer,
    abortHooks
  } = transport;
  const active = new Set();
  const requests = new Set();
  const states = new WeakMap();
  const redirects = new Set([301, 302, 303, 307, 308]);
  const responseClone = Response.prototype.clone;
  const fromWeb = Readable.fromWeb.bind(Readable);
  const from = Readable.from.bind(Readable);
  const destroyWritable = Writable.prototype.destroy;
  const destroyReadable = Readable.prototype.destroy;
  const abortController = AbortController.prototype.abort;
  let stopped = false;

  function requestDetails(endpoint, method, id) {
    return { id, url: endpoint.href, method, resourceType: 'other', referrer: '' };
  }

  async function beforeRequest(endpoint, method, id, entry, fields) {
    const policy = requestPolicy();
    if (policy) {
      const requestHeaders = Object.create(null);
      for (const [name, value] of fields) {
        requestHeaders[name] = [value];
      }
      const result = await policy.intercept(
        'before',
        { ...requestDetails(endpoint, method, id), requestHeaders },
        entry.controller.signal
      );
      if (result.requestHeaders !== undefined) {
        // This is the actual pre-transport list, before SDK-generated Cookie,
        // Host and framing fields. Preserve values rather than simulating a
        // final wire snapshot; the shared policy validates protected headers.
        const patched = new Headers();
        for (const [name, values] of Object.entries(result.requestHeaders)) {
          for (const value of values) {
            patched.append(name, value);
          }
        }
        fields = patched;
      }
    }
    alive();
    if (entry.controller.signal.aborted) {
      throw abortHooks.reason(entry.controller.signal);
    }
    return fields;
  }

  async function receivedHeaders(response, endpoint, method, id, entry) {
    const policy = requestPolicy();
    if (!policy) {
      return response;
    }
    try {
      const result = await policy.intercept(
        'headers',
        {
          ...requestDetails(endpoint, method, id),
          statusCode: response.status,
          responseHeaders: responseHeaders(response, true)
        },
        entry.controller.signal
      );
      alive();
      if (entry.controller.signal.aborted) {
        throw abortHooks.reason(entry.controller.signal);
      }
      if (!result.responseHeaders) {
        return response;
      }
      const fields = [];
      for (const [name, values] of Object.entries(result.responseHeaders)) {
        for (const value of values) {
          fields.push([name, value]);
        }
      }
      // The original transport still owns framing, backpressure and abort. Only
      // the application-visible head changes; never buffer the response body.
      return new Response(response.body, {
        status: response.status,
        statusText: response.statusText,
        headers: fields
      });
    } catch (failure) {
      await response.body?.cancel(failure);
      throw failure;
    }
  }

  function alive(ready = true) {
    if (stopped || isExited()) {
      stop();
      throw error('ERR_FIREFLY_APP_EXITED', 'Network application has exited');
    }
    if (ready && !isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'net requires app readiness');
    }
  }

  function proxy() {
    alive();
    if (typeof native.networkHasProxy !== 'function') {
      unsupported('net native proxy snapshot');
    }
    const value = native.networkHasProxy();
    alive();
    if (typeof value !== 'boolean') {
      throw error('ERR_FIREFLY_NET_DATA', 'Invalid native proxy snapshot');
    }
    if (value) {
      unsupported('net configured proxy/PAC');
    }
  }

  function url(value, base, custom = false) {
    const result = new URL(value, base);
    if (
      !['http:', 'https:'].includes(result.protocol) &&
      !(custom && protocols()?.canHandle(result.href))
    ) {
      unsupported('net URL scheme ' + result.protocol);
    }
    if (result.username || result.password) {
      unsupported('net URL authentication');
    }
    result.hash = '';
    return result;
  }

  function policy(value) {
    if (value === undefined || value === null) {
      return {};
    }
    if (typeof value !== 'object') {
      throw error('ERR_INVALID_ARG_TYPE', 'Network options must be an object');
    }
    const result = { ...value };
    for (const name of [
      'session',
      'partition',
      'useSessionCookies',
      'auth',
      'proxy',
      'proxyRules',
      'dispatcher',
      'origin',
      'priority',
      'priorityIncremental'
    ]) {
      if (result[name] !== undefined) {
        unsupported('net option ' + name);
      }
    }
    if (
      result.bypassCustomProtocolHandlers !== undefined &&
      typeof result.bypassCustomProtocolHandlers !== 'boolean'
    ) {
      throw error('ERR_INVALID_ARG_TYPE', 'bypassCustomProtocolHandlers must be a boolean');
    }
    if (result.agent !== undefined && result.agent !== null) {
      unsupported('net agent');
    }
    if (result.rejectUnauthorized !== undefined && result.rejectUnauthorized !== true) {
      unsupported('net disabled TLS verification');
    }
    if (result.credentials !== undefined && result.credentials !== 'omit') {
      unsupported('net session credentials');
    }
    if (result.cache !== undefined && !['default', 'no-store'].includes(result.cache)) {
      unsupported('net cache policy');
    }
    if (result.keepalive) {
      unsupported('net keepalive');
    }
    if (result.referrerPolicy || result.referrer || result.integrity) {
      unsupported('net referrer/integrity policy');
    }
    return result;
  }

  function acquire(signal) {
    alive();
    if (active.size >= 64) {
      throw error('ERR_FIREFLY_NET_LIMIT', 'Network request budget exhausted');
    }
    const controller = new AbortController();
    const entry = { controller, unsubscribe: null };
    active.add(entry);
    if (signal) {
      entry.unsubscribe = abortHooks.subscribe(signal, () =>
        abortController.call(controller, abortHooks.reason(signal))
      );
    }
    return entry;
  }

  function release(entry) {
    active.delete(entry);
    entry.unsubscribe?.();
    entry.unsubscribe = null;
  }

  function annotate(response, endpoint, redirected, headers = response.headers) {
    Object.defineProperties(response, {
      url: { value: endpoint, configurable: true },
      redirected: { value: redirected, configurable: true },
      headers: { value: headers, configurable: true },
      clone: {
        value() {
          return annotate(responseClone.call(this), endpoint, redirected, headers);
        },

        configurable: true
      }
    });
    return response;
  }

  function ownedResponse(response, entry, endpoint, redirected) {
    if (!response.body) {
      release(entry);
      return annotate(response, endpoint, redirected);
    }
    const reader = response.body.getReader();
    const body = new ReadableStream({
      type: 'bytes',

      async pull(controller) {
        try {
          const result = await reader.read();
          if (result.done) {
            release(entry);
            controller.close();
            controller.byobRequest?.respond(0);
          } else {
            controller.enqueue(result.value);
          }
        } catch (failure) {
          release(entry);
          controller.error(failure);
        }
      },

      async cancel(reason) {
        try {
          await reader.cancel(reason);
        } finally {
          release(entry);
        }
      }
    });
    return annotate(
      new Response(body, {
        status: response.status,
        statusText: response.statusText,
        headers: response.headers
      }),
      endpoint,
      redirected,
      response.headers
    );
  }

  function nextHop(endpoint, method, headers, body, status, location, custom = false) {
    const target = url(location, endpoint, custom);
    const fields = new Headers(headers);
    // WHATWG URL reports "null" for custom origins. Distinct custom hosts still
    // form distinct application authorities and must not inherit credentials.
    const sameOrigin =
      target.origin !== 'null' && endpoint.origin !== 'null'
        ? target.origin === endpoint.origin
        : target.protocol === endpoint.protocol && target.host === endpoint.host;
    if (!sameOrigin) {
      for (const name of ['authorization', 'proxy-authorization', 'cookie']) {
        fields.delete(name);
      }
    }
    if (
      (status === 303 && method !== 'GET' && method !== 'HEAD') ||
      ([301, 302].includes(status) && method === 'POST')
    ) {
      method = 'GET';
      body = null;
      for (const name of [
        'content-length',
        'content-type',
        'content-encoding',
        'content-language',
        'content-location'
      ]) {
        fields.delete(name);
      }
    }
    return { endpoint: target, method, headers: fields, body };
  }

  async function fetch(input, init) {
    alive();
    const selected = policy(init);
    const original = new Request(input, selected);
    if (original.credentials === 'include') {
      unsupported('net session credentials');
    }
    const custom = selected.bypassCustomProtocolHandlers !== true;
    let endpoint = url(original.url, undefined, custom);
    let method = original.method;
    let fields = new Headers(original.headers);
    let body = original.body;
    const mode = original.redirect;
    const requestId = requestPolicy()?.allocateId();
    const entry = acquire(original.signal);
    try {
      for (let count = 0; ; count++) {
        if (requestPolicy()) {
          fields = await beforeRequest(endpoint, method, requestId, entry, fields);
        }
        const registry = custom ? protocols() : null;
        const handled = registry?.canHandle(endpoint.href);
        if (!handled) {
          proxy();
        }
        const options = {
          method,
          headers: fields,
          body,
          duplex: 'half',
          redirect: 'manual',
          signal: entry.controller.signal,
          credentials: 'omit',
          cache: original.cache
        };
        let response = handled
          ? await registry.fetch(endpoint.href, options)
          : await fetchTransport(endpoint.href, options);
        response = await receivedHeaders(response, endpoint, method, requestId, entry);
        alive();
        const location = response.headers.get('location');
        if (!redirects.has(response.status) || !location || mode === 'manual') {
          return ownedResponse(response, entry, endpoint.href, count > 0);
        }
        await response.body?.cancel();
        if (mode === 'error') {
          throw error('ERR_FIREFLY_NET_REDIRECT', 'Redirect policy forbids redirect');
        }
        if (count >= 20) {
          throw error('ERR_FIREFLY_NET_REDIRECT', 'Too many redirects');
        }
        const next = nextHop(endpoint, method, fields, body, response.status, location, custom);
        if (next.body !== null) {
          unsupported('net.fetch replay of redirected streaming body');
        }
        ({ endpoint, method, headers: fields, body } = next);
      }
    } catch (failure) {
      abortController.call(entry.controller, failure);
      release(entry);
      throw failure;
    }
  }

  function responseHeaders(response, arrays = false) {
    const result = Object.create(null);
    for (const [name, value] of response.headers) {
      result[name] = arrays ? [value] : value;
    }
    const cookies = response.headers.getSetCookie?.() ?? [];
    if (cookies.length) {
      result['set-cookie'] = [...cookies];
    }
    return result;
  }

  function requestOptions(value) {
    const selected = typeof value === 'string' ? { url: value } : policy(value);
    let address = selected.url ?? selected.href;
    if (!address) {
      const protocol = selected.protocol ?? 'http:';
      let authority = selected.host ?? selected.hostname ?? 'localhost';
      if (!selected.host && selected.port) {
        authority += ':' + selected.port;
      }
      const pathname = selected.path ?? '/';
      if (String(pathname).includes(' ')) {
        throw error('ERR_INVALID_ARG_VALUE', 'Request path contains unescaped spaces');
      }
      address = protocol + '//' + authority + pathname;
    }
    const endpoint = url(address);
    const method = String(selected.method ?? 'GET').toUpperCase();
    if (
      !/^[!#$%&'*+.^_`|~0-9A-Z-]+$/.test(method) ||
      ['CONNECT', 'TRACE', 'TRACK'].includes(method)
    ) {
      throw error('ERR_INVALID_ARG_VALUE', 'Invalid request method');
    }
    const redirect = selected.redirect ?? 'follow';
    if (!['manual', 'follow', 'error'].includes(redirect)) {
      throw error('ERR_INVALID_ARG_VALUE', 'Invalid redirect policy');
    }
    return {
      endpoint,
      method,
      redirect,
      headers: selected.headers ?? {},
      cache: selected.cache ?? 'default'
    };
  }

  class ClientRequest extends Writable {
    constructor(spec, callback) {
      super({ autoDestroy: false });
      const state = {
        spec,
        headers: new Map(),
        chunks: [],
        size: 0,
        locked: false,
        started: false,
        aborted: false,
        entry: null,
        response: null,
        waiting: false,
        follow: false,
        chunked: undefined
      };
      states.set(this, state);
      if (callback) {
        this.once('response', callback);
      }
      for (const [name, value] of Object.entries(spec.headers)) {
        this.setHeader(name, value);
      }
      alive();
      if (requests.size >= 64) {
        throw error('ERR_FIREFLY_NET_LIMIT', 'ClientRequest budget exhausted');
      }
      requests.add(this);
    }

    setHeader(name, value) {
      const state = states.get(this);
      if (state.locked) {
        throw error('ERR_FIREFLY_NET_HEADERS_SENT', 'Request headers have already been sent');
      }
      if (typeof name !== 'string' || value === undefined || value === null) {
        throw error('ERR_INVALID_ARG_TYPE', 'Invalid request header');
      }
      const values = Array.isArray(value) ? value.map(String) : String(value);
      new Headers([[name, Array.isArray(values) ? values.join(',') : values]]);
      state.headers.set(name.toLowerCase(), { name, value: values });
    }

    getHeader(name) {
      if (typeof name !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Header name must be a string');
      }
      const value = states.get(this).headers.get(name.toLowerCase())?.value;
      return Array.isArray(value) ? [...value] : value;
    }

    removeHeader(name) {
      const state = states.get(this);
      if (state.locked) {
        throw error('ERR_FIREFLY_NET_HEADERS_SENT', 'Request headers have already been sent');
      }
      if (typeof name !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Header name must be a string');
      }
      state.headers.delete(name.toLowerCase());
    }

    get chunkedEncoding() {
      return false;
    }

    set chunkedEncoding(value) {
      const state = states.get(this);
      if (state.locked || state.chunked !== undefined) {
        throw error('ERR_INVALID_STATE', 'chunkedEncoding may only be set once before writing');
      }
      if (value) {
        unsupported('net.request chunked upload');
      }
      state.chunked = false;
    }

    _write(chunk, encoding, callback) {
      const state = states.get(this);
      state.locked = true;
      if (state.size + chunk.byteLength > 16777216) {
        callback(error('ERR_FIREFLY_NET_LIMIT', 'Request body exceeds 16 MiB'));
        return;
      }
      state.chunks.push(Buffer.from(chunk));
      state.size += chunk.byteLength;
      callback();
    }

    _final(callback) {
      const state = states.get(this);
      state.locked = true;
      state.started = true;
      callback();

      run(this).catch((failure) => {
        if (!this.destroyed && !state.aborted) {
          this.destroy(failure);
        }
      });
    }

    _destroy(failure, callback) {
      const state = states.get(this);
      if (state.entry) {
        abortController.call(state.entry.controller, failure);
      }
      if (state.entry) {
        release(state.entry);
      }
      if (state.response) {
        destroyReadable.call(state.response, failure);
      }
      state.chunks.length = 0;
      requests.delete(this);
      callback(failure);
    }

    abort() {
      const state = states.get(this);
      if (!state.aborted) {
        state.aborted = true;

        queueMicrotask(() => this.emit('abort'));

        this.destroy();
      }
    }

    followRedirect() {
      const state = states.get(this);
      if (!state.waiting) {
        throw error('ERR_INVALID_STATE', 'Not waiting for a redirect');
      }
      state.follow = true;
    }

    getUploadProgress() {
      return unsupported('net.request upload progress');
    }
  }

  async function run(request) {
    const state = states.get(request);
    if (request.destroyed) {
      return;
    }
    let { endpoint, method } = state.spec;
    let fields = new Headers();
    for (const { name, value } of state.headers.values()) {
      fields.set(name, Array.isArray(value) ? value.join(',') : value);
    }
    let body = state.size ? Buffer.concat(state.chunks) : null;
    state.chunks.length = 0;
    const requestId = requestPolicy()?.allocateId();
    const entry = (state.entry = acquire());
    try {
      for (let count = 0; ; count++) {
        if (requestPolicy()) {
          fields = await beforeRequest(endpoint, method, requestId, entry, fields);
        }
        proxy();
        if (request.destroyed) {
          return;
        }
        let response = await fetchTransport(endpoint.href, {
          method,
          headers: fields,
          body,
          redirect: 'manual',
          signal: entry.controller.signal,
          credentials: 'omit',
          cache: state.spec.cache
        });
        response = await receivedHeaders(response, endpoint, method, requestId, entry);
        alive();
        if (request.destroyed) {
          await response.body?.cancel();
          return;
        }
        const location = response.headers.get('location');
        if (redirects.has(response.status) && location) {
          const next = nextHop(endpoint, method, fields, body, response.status, location);
          await response.body?.cancel();
          if (state.spec.redirect === 'error' || count >= 20) {
            throw error('ERR_FIREFLY_NET_REDIRECT', 'Redirect policy forbids redirect');
          }
          state.waiting = true;
          state.follow = state.spec.redirect === 'follow';
          try {
            request.emit(
              'redirect',
              response.status,
              next.method,
              next.endpoint.href,
              responseHeaders(response, true)
            );
          } finally {
            state.waiting = false;
          }
          if (request.destroyed) {
            return;
          }
          if (!state.follow) {
            throw error('ERR_FIREFLY_NET_REDIRECT', 'Redirect was cancelled');
          }
          ({ endpoint, method, headers: fields, body } = next);
          continue;
        }
        const incoming = response.body ? fromWeb(response.body) : from([]);
        state.response = incoming;
        incoming.statusCode = response.status;
        incoming.statusMessage = response.statusText;
        incoming.headers = responseHeaders(response);
        for (const name of [
          'rawHeaders',
          'httpVersion',
          'httpVersionMajor',
          'httpVersionMinor',
          'trailers',
          'rawTrailers'
        ]) {
          Object.defineProperty(incoming, name, {
            get() {
              return unsupported('net response ' + name);
            }
          });
        }

        incoming.once('end', () => {
          release(entry);
          request.destroy();
        });

        incoming.once('error', (failure) => {
          if (!request.destroyed) {
            request.destroy(failure);
          }
        });

        incoming.once('close', () => {
          if (!request.destroyed) {
            request.destroy();
          }
        });

        request.emit('response', incoming);
        return;
      }
    } catch (failure) {
      abortController.call(entry.controller, failure);
      release(entry);
      throw failure;
    }
  }

  const net = {
    fetch,

    request(value, callback) {
      alive();
      if (requests.size >= 64) {
        throw error('ERR_FIREFLY_NET_LIMIT', 'ClientRequest budget exhausted');
      }
      if (callback !== undefined && typeof callback !== 'function') {
        throw error('ERR_INVALID_ARG_TYPE', 'Response callback must be a function');
      }
      const spec = requestOptions(value);
      alive();
      return new ClientRequest(spec, callback);
    },

    isOnline() {
      alive(false);
      if (typeof native.networkIsOnline !== 'function') {
        return unsupported('net native online query');
      }
      const value = native.networkIsOnline();
      alive(false);
      if (typeof value !== 'boolean') {
        throw error('ERR_FIREFLY_NET_DATA', 'Invalid native network status');
      }
      return value;
    },

    resolveHost() {
      return unsupported('net.resolveHost');
    }
  };
  Object.defineProperty(net, 'online', { get: () => net.isOnline() });

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    try {
      for (const entry of [...active]) {
        try {
          abortController.call(
            entry.controller,
            error('ERR_FIREFLY_APP_EXITED', 'Application exited during network request')
          );
        } catch {
          // A user abort listener cannot prevent other owned exchanges from being revoked.
        } finally {
          try {
            release(entry);
          } catch {
            active.delete(entry);
          }
        }
      }
      for (const request of [...requests]) {
        const state = states.get(request);
        state.chunks.length = 0;
        try {
          destroyWritable.call(request);
        } catch {
          // Public stream hooks may throw; the private transport was already aborted above.
        } finally {
          requests.delete(request);
          if (state.response) {
            try {
              destroyReadable.call(state.response);
            } catch {
              /* Continue cleanup. */
            }
          }
        }
      }
    } finally {
      try {
        native.networkStop?.();
      } catch {
        /* Provider failure cannot interrupt teardown. */
      }
    }
  }

  return { net, stop };
}
