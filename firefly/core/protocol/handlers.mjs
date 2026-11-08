import createResponses from './responses.mjs';

/** Main-owned modern handlers, distinct from legacy native-file descriptors. */
export default function createHandlers(
  native,
  isReady,
  error,
  unsupported,
  transport,
  permitted,
  legacy
) {
  const routes = new Map();
  const pending = new Map();
  const local = new Set();
  let next = 0;
  let stopped = false;
  let responses;
  const web = transport ? { ...transport } : undefined;
  const signalAborted = web?.AbortSignal
    ? Object.getOwnPropertyDescriptor(web.AbortSignal.prototype, 'aborted').get
    : undefined;
  const completeTransport =
    web &&
    ['Request', 'Response', 'Headers', 'AbortController', 'ReadableStream'].every(
      (name) => typeof web[name] === 'function'
    );
  // Capture web prototypes during API initialization, before application code can replace them.
  if (completeTransport) {
    responses = createResponses(web, error, unsupported);
  }

  const requestGet = (name) =>
    completeTransport
      ? Object.getOwnPropertyDescriptor(web.Request.prototype, name).get
      : undefined;

  const requestURL = requestGet('url');
  const requestMethod = requestGet('method');
  const requestHeaders = requestGet('headers');
  const requestSignal = requestGet('signal');
  const DATA = 'ERR_FIREFLY_PROTOCOL_DATA';

  const canceled = () =>
    error('ERR_FIREFLY_PROTOCOL_CANCELED', 'Protocol handler ownership revoked');

  function ready() {
    if (stopped) {
      throw error('ERR_FIREFLY_APP_EXITED', 'Protocol application has exited');
    }
    if (!isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'Protocol handlers require app readiness');
    }
  }

  function scheme(value) {
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', 'scheme must be a string');
    }
    if (!/^[a-z][a-z0-9+.-]{0,63}$/.test(value)) {
      throw error('ERR_INVALID_ARG_VALUE', 'Invalid canonical scheme');
    }
    return value;
  }

  function opaque(value) {
    return typeof value === 'string' && /^[1-9][0-9]{0,19}$/.test(value);
  }

  function controller() {
    if (!completeTransport) {
      return unsupported('protocol captured web transport');
    }
    return (responses ??= createResponses(web, error, unsupported));
  }

  function allocate() {
    if (next >= 2147483647 || pending.size + local.size >= 64) {
      throw error('ERR_FIREFLY_PROTOCOL_LIMIT', 'Protocol handler budget exhausted');
    }
    return ++next;
  }

  function retire(entry, reason = canceled()) {
    if (entry.token !== undefined) {
      if (pending.get(entry.token) === entry) {
        pending.delete(entry.token);
      }
    } else {
      local.delete(entry);
    }
    entry.reason ??= reason;
    responses?.cancel(entry.id, 1, entry.reason);
    entry.unsubscribe?.();
    entry.unsubscribe = null;
  }

  function deny(token) {
    try {
      native.protocolResponseHead(JSON.stringify({ token, error: true }));
    } catch {
      /* Native shutdown owns transport retirement. */
    }
  }

  function matches(request, entry) {
    return entry && request.generation === entry.generation && !stopped;
  }

  function handle(name, handler) {
    ready();
    scheme(name);
    if (typeof handler !== 'function') {
      throw error('ERR_INVALID_ARG_TYPE', 'Protocol handler must be callable');
    }
    if (name !== 'file' && !permitted(name)) {
      unsupported('modern handler requires registered standard supportFetchAPI scheme');
    }
    if (routes.has(name) || legacy(name)) {
      throw error('ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED', 'Protocol scheme already handled');
    }
    controller();
    const register = native.protocolRegisterResponse;
    if (typeof register !== 'function') {
      return unsupported('protocol native response transport');
    }
    ready();
    const entry = { generation: null, handler };
    routes.set(name, entry);
    let generation;
    try {
      generation = register.call(native, name);
      if (stopped || routes.get(name) !== entry) {
        try {
          native.protocolUnregisterFile(name);
        } catch {
          /* Retire late admission. */
        }
        ready();
        throw canceled();
      }
      if (!opaque(generation)) {
        try {
          native.protocolUnregisterFile(name);
        } catch {
          /* Invalid admission is revoked. */
        }
        throw error(DATA, 'Invalid native protocol generation');
      }
      entry.generation = generation;
    } catch (failure) {
      if (routes.get(name) === entry) {
        routes.delete(name);
      }
      throw failure;
    }
  }

  function unhandle(name) {
    ready();
    scheme(name);
    const route = routes.get(name);
    if (!route) {
      return;
    }
    // Revoke callbacks and in-flight ownership before crossing native teardown.
    routes.delete(name);
    for (const entry of [...pending.values(), ...local]) {
      if (entry.scheme === name) {
        retire(entry);
      }
    }
    native.protocolUnregisterFile(name);
  }

  function dispatch(type, request) {
    if (!opaque(request?.token) || !opaque(request?.generation)) {
      return false;
    }
    if (type === 'protocol-cancel') {
      const entry = pending.get(request.token);
      if (!matches(request, entry)) {
        return false;
      }
      retire(entry);
      return true;
    }
    if (type === 'protocol-pull') {
      const entry = pending.get(request.token);
      if (!matches(request, entry)) {
        return false;
      }

      controller()
        .pull(entry.id, 1, request.credit)
        .then((item) => {
          if (pending.get(request.token) !== entry || stopped) {
            return;
          }
          const accepted = native.protocolResponseChunk(request.token, item.chunk, item.done);
          if (accepted !== true) {
            retire(entry);
            return;
          }
          if (item.done) {
            pending.delete(request.token);
          }
        })
        .catch(() => {
          if (pending.get(request.token) === entry) {
            retire(entry);
            deny(request.token);
          }
        });

      return true;
    }
    if (type !== 'protocol-request') {
      return false;
    }
    const route = routes.get(request.scheme);
    if (!route) {
      if (request.modern === true) {
        deny(request.token);
        return true;
      }
      return false;
    }
    if (!matches(request, route) || pending.has(request.token)) {
      deny(request.token);
      return true;
    }
    let entry;
    try {
      entry = {
        id: allocate(),
        token: request.token,
        generation: request.generation,
        scheme: request.scheme
      };
      pending.set(request.token, entry);
      const headers = typeof request.origin === 'string' ? { origin: request.origin } : undefined;

      controller()
        .start(entry.id, 1, { url: request.url, method: request.method, headers }, route.handler)
        .then((metadata) => {
          if (pending.get(request.token) !== entry || stopped) {
            return;
          }
          const head = {
            schema: 1,
            kind: 'stream-response',
            request: 1,
            status: metadata.status,
            length: metadata.contentLength,
            head: request.method === 'HEAD',
            bodyNull: metadata.bodyNull,
            headers: metadata.headers
          };
          const accepted = native.protocolResponseHead(
            JSON.stringify({ token: request.token, head })
          );
          if (accepted !== true) {
            retire(entry);
          }
        })
        .catch(() => {
          if (pending.get(request.token) === entry) {
            retire(entry);
            deny(request.token);
          }
        });
    } catch {
      if (entry) {
        retire(entry);
      }
      deny(request.token);
    }
    return true;
  }

  function canHandle(input) {
    if (stopped || !web) {
      return false;
    }
    const endpoint = new web.URL(input);
    return routes.has(endpoint.protocol.slice(0, -1));
  }

  function requireLocal(entry) {
    if (stopped || !local.has(entry) || routes.get(entry.scheme)?.generation !== entry.generation) {
      throw entry.reason ?? canceled();
    }
  }

  function streamedResponse(entry, metadata) {
    const { Response, ReadableStream } = web;
    const stream = new ReadableStream(
      {
        async pull(sink) {
          try {
            requireLocal(entry);
            const item = await controller().pull(entry.id, 1);
            requireLocal(entry);
            if (item.done) {
              local.delete(entry);
              entry.unsubscribe?.();
              entry.unsubscribe = null;
              sink.close();
            } else {
              sink.enqueue(item.chunk);
            }
          } catch (failure) {
            retire(entry, failure);
            sink.error(failure);
          }
        },

        cancel(reason) {
          retire(entry, reason);
        }
      },
      { highWaterMark: 0 }
    );
    return new Response(stream, {
      status: metadata.status,
      statusText: metadata.statusText,
      headers: metadata.headers
    });
  }

  async function fetch(input, init) {
    ready();
    controller();
    const { Request, Response, ReadableStream, abortHooks } = web;
    const request = new Request(input, init);
    const url = requestURL.call(request);
    const method = requestMethod.call(request);
    const headers = requestHeaders.call(request);
    const name = new web.URL(url).protocol.slice(0, -1);
    const route = routes.get(name);
    if (!route) {
      throw error('ERR_FIREFLY_PROTOCOL_UNHANDLED', 'No modern handler for URL');
    }
    const entry = { id: allocate(), generation: route.generation, scheme: name, unsubscribe: null };
    local.add(entry);
    const signal = requestSignal.call(request);
    if (!abortHooks || !signalAborted) {
      retire(entry);
      return unsupported('protocol captured abort hooks');
    }
    if (signalAborted.call(signal)) {
      retire(entry);
      throw abortHooks.reason(signal);
    }
    entry.unsubscribe = abortHooks.subscribe(signal, () =>
      retire(entry, abortHooks.reason(signal))
    );
    try {
      requireLocal(entry);
      const metadata = await controller().start(
        entry.id,
        1,
        { url, method, headers },
        route.handler
      );
      requireLocal(entry);
      if (metadata.bodyNull) {
        await controller().pull(entry.id, 1);
        requireLocal(entry);
        local.delete(entry);
        entry.unsubscribe?.();
        entry.unsubscribe = null;
        return new Response(null, {
          status: metadata.status,
          statusText: metadata.statusText,
          headers: metadata.headers
        });
      }
      return streamedResponse(entry, metadata);
    } catch (failure) {
      retire(entry, failure);
      throw failure;
    }
  }

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    for (const entry of [...pending.values(), ...local]) {
      retire(entry);
    }
    responses?.stop();
    const names = [...routes.keys()];
    routes.clear();
    for (const name of names) {
      try {
        native.protocolUnregisterFile(name);
      } catch {
        /* Retire every owned route. */
      }
    }
  }

  return {
    handle,
    unhandle,

    isHandled: (name) => routes.has(name),

    dispatch,
    canHandle,
    fetch,
    stop
  };
}
