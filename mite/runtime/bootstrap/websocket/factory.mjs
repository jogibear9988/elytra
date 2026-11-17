/** A browser-style client facade over the pinned RFC 6455 engine and real Mite sockets. */
import createWebSocketEvents from './events.mjs';
import createPayloadCapture from './payload.mjs';

export default function createWebSocketGlobals(host) {
  const { Event, EventTarget, getEventAttributeValue, setEventAttributeValue } = host.events;
  const { Buffer, URL, Blob, DOMException, Engine } = host;
  const { MessageEvent, CloseEvent } = createWebSocketEvents({ Event });
  const capture = createPayloadCapture(host);
  const apply = Reflect.apply;
  const dispatch = EventTarget.prototype.dispatchEvent;
  const define = Object.defineProperty;
  const states = new WeakMap();
  const maximumBuffered = 4 * 1024 * 1024;
  const maximumConnections = 128;
  let connections = 0;
  // Active native sockets own their public event target until genuine close.
  const active = new Set();

  function state(value) {
    const record = states.get(value);
    if (!record) {
      throw new TypeError('Invalid WebSocket receiver');
    }
    return record;
  }

  function unsupported(message) {
    const error = new Error(message);
    error.code = 'ERR_MITE_UNSUPPORTED_API';
    throw error;
  }

  function limit(message) {
    const error = new RangeError(message);
    error.code = 'ERR_MITE_WEBSOCKET_LIMIT';
    throw error;
  }

  function string(value) {
    if (typeof value === 'symbol') {
      throw new TypeError('Cannot convert a Symbol value to a string');
    }
    return String(value);
  }

  function configuration(input, protocols) {
    const source = string(input);
    let url;
    try {
      url = new URL(source);
    } catch {
      throw new DOMException('Invalid WebSocket URL', 'SyntaxError');
    }
    if (url.protocol === 'http:') {
      url.protocol = 'ws:';
    } else if (url.protocol === 'https:') {
      url.protocol = 'wss:';
    }
    if (!['ws:', 'wss:'].includes(url.protocol) || url.href.includes('#')) {
      throw new DOMException('Invalid WebSocket URL scheme or fragment', 'SyntaxError');
    }
    if (
      protocols !== null &&
      (typeof protocols === 'object' || typeof protocols === 'function') &&
      !Array.isArray(protocols)
    ) {
      // Node's Undici overload carries proxy/certificate policy in a dispatcher.
      // Reject it explicitly until that actual transport delegation is supported.
      for (const key of ['dispatcher', 'headers', 'agent', 'ca', 'rejectUnauthorized']) {
        if (protocols[key] !== undefined) {
          unsupported(`WebSocket option ${key} is not implemented`);
        }
      }
      protocols = protocols.protocols;
    }
    const values = [];
    if (typeof protocols === 'symbol') {
      throw new TypeError('WebSocket protocols cannot be a Symbol');
    }
    if (typeof protocols === 'string') {
      values.push(protocols);
    } else if (protocols != null && protocols[Symbol.iterator] !== undefined) {
      for (const value of protocols) {
        if (values.length === 64) {
          limit('WebSocket protocol list exceeds 64 entries');
        }
        values.push(string(value));
      }
    }
    const seen = new Set();
    for (const value of values) {
      if (!/^[!#$%&'*+\-.^_`|~0-9A-Za-z]+$/.test(value) || seen.has(value)) {
        throw new DOMException('Invalid or duplicate WebSocket protocol', 'SyntaxError');
      }
      seen.add(value);
    }
    return { url: url.href, origin: url.origin, protocols: values };
  }

  function buffered(record) {
    return record.queuedBytes + record.discardedBytes + (record.engine?.bufferedAmount ?? 0);
  }

  function emit(socket, event) {
    apply(dispatch, socket, [event]);
  }

  function failed(socket, error) {
    const record = state(socket);
    if (record.errored || record.closed) {
      return;
    }
    record.errored = true;
    record.ready = WebSocket.CLOSED;
    const event = new Event('error');
    define(event, 'error', { value: error, enumerable: true });
    define(event, 'message', {
      value: String(error?.message ?? 'WebSocket connection failed'),
      enumerable: true
    });
    emit(socket, event);
  }

  function closed(socket, code, reason) {
    const record = state(socket);
    if (record.closed) {
      return;
    }
    record.closed = true;
    record.ready = WebSocket.CLOSED;
    record.queue.length = 0;
    record.queuedBytes = 0;
    --connections;
    active.delete(socket);
    // These private provider flags describe a real close-frame exchange. They
    // are source-pinned and never supplied by the application or the peer.
    const clean = Boolean(record.engine?._closeFrameSent && record.engine?._closeFrameReceived);
    emit(
      socket,
      new CloseEvent('close', {
        code,
        reason: Buffer.from(reason).toString('utf8'),
        wasClean: clean
      })
    );
  }

  function flush(socket) {
    const record = state(socket);

    const canSend = () =>
      record.ready === WebSocket.OPEN ||
      (record.ready === WebSocket.CLOSING && record.closeRequest);

    if (record.flushing || !canSend()) {
      return;
    }
    record.flushing = true;

    const drain = () => {
      while (canSend() && record.queue.length) {
        const entry = record.queue[0];
        if (entry.read) {
          const read = entry.read;
          entry.read = null;

          read().then(
            (value) => {
              entry.value = value;
              drain();
            },
            (error) => {
              record.flushing = false;
              failed(socket, error);
              record.engine.terminate();
            }
          );

          return;
        }
        record.queue.shift();
        record.queuedBytes -= entry.bytes;

        record.engine.send(entry.value, { binary: entry.binary, compress: false }, (error) => {
          if (error) {
            failed(socket, error);
            record.engine.terminate();
          }
        });
      }
      record.flushing = false;
      if (record.ready === WebSocket.CLOSING && record.closeRequest && !record.queue.length) {
        const request = record.closeRequest;
        record.closeRequest = null;
        record.engine.close(request.code, request.reason);
      }
    };

    drain();
  }

  class WebSocket extends EventTarget {
    constructor(url, protocols = []) {
      super();
      if (!arguments.length) {
        throw new TypeError('WebSocket requires a URL');
      }
      const config = configuration(url, protocols);
      if (connections >= maximumConnections) {
        limit('WebSocket connection limit exceeded');
      }
      const record = {
        ready: WebSocket.CONNECTING,
        url: config.url,
        origin: config.origin,
        protocol: '',
        extensions: '',
        binaryType: 'blob',
        queue: [],
        queuedBytes: 0,
        discardedBytes: 0,
        flushing: false,
        closed: false,
        errored: false,
        engine: null
      };
      states.set(this, record);
      ++connections;
      active.add(this);
      try {
        const engine = new Engine(config.url, config.protocols, {
          perMessageDeflate: false,
          maxPayload: 1024 * 1024,
          handshakeTimeout: 10000,
          closeTimeout: 5000,
          followRedirects: false,
          allowSynchronousEvents: false,
          skipUTF8Validation: false
        });
        record.engine = engine;
        engine.binaryType = 'arraybuffer';

        engine.on('open', () => {
          if (record.ready !== WebSocket.CONNECTING) {
            return;
          }
          record.ready = WebSocket.OPEN;
          record.protocol = engine.protocol;
          record.extensions = engine.extensions;
          emit(this, new Event('open'));
        });

        engine.on('message', (data, binary) => {
          if (record.ready !== WebSocket.OPEN) {
            return;
          }
          const value = binary
            ? record.binaryType === 'blob'
              ? new Blob([data])
              : data
            : Buffer.from(data).toString('utf8');
          emit(this, new MessageEvent('message', { data: value, origin: config.origin }));
        });

        engine.on('error', (error) => failed(this, error));

        engine.on('close', (code, reason) => closed(this, code, reason));
      } catch (error) {
        --connections;
        active.delete(this);
        states.delete(this);
        throw error;
      }
    }

    get url() {
      return state(this).url;
    }

    get readyState() {
      return state(this).ready;
    }

    get protocol() {
      return state(this).protocol;
    }

    get extensions() {
      return state(this).extensions;
    }

    get bufferedAmount() {
      const record = state(this);
      return buffered(record);
    }

    get binaryType() {
      return state(this).binaryType;
    }

    set binaryType(value) {
      const record = state(this);
      value = string(value);
      if (value === 'blob' || value === 'arraybuffer') {
        record.binaryType = value;
      }
    }

    send(data) {
      const record = state(this);
      if (!arguments.length) {
        throw new TypeError('WebSocket.send requires data');
      }
      if (record.ready === WebSocket.CONNECTING) {
        throw new DOMException('WebSocket is still connecting', 'InvalidStateError');
      }
      const payload = capture(data);
      if (payload.bytes > maximumBuffered - buffered(record) || record.queue.length >= 1024) {
        limit('WebSocket outgoing queue exhausted');
      }
      if (record.ready !== WebSocket.OPEN) {
        record.discardedBytes += payload.bytes;
        return;
      }
      record.queue.push(payload);
      record.queuedBytes += payload.bytes;
      flush(this);
    }

    close(code, reason = '') {
      const record = state(this);
      if (code !== undefined) {
        const number = +code;
        code = Number.isFinite(number) ? ((Math.trunc(number) % 65536) + 65536) % 65536 : 0;
        if (code !== 1000 && !(code >= 3000 && code <= 4999)) {
          throw new DOMException('Invalid WebSocket close code', 'InvalidAccessError');
        }
      }
      reason = Buffer.from(string(reason), 'utf8');
      if (reason.byteLength > 123) {
        throw new DOMException('WebSocket close reason exceeds 123 bytes', 'SyntaxError');
      }
      if (record.ready >= WebSocket.CLOSING) {
        return;
      }
      const connecting = record.ready === WebSocket.CONNECTING;
      record.ready = WebSocket.CLOSING;
      if (connecting) {
        // Abort the genuine HTTP handshake. There is no open transport whose
        // send queue can drain, and a late open must never retain this socket.
        record.engine.close(code, reason);
        return;
      }
      // An accepted Blob read owns its FIFO position. Send its immutable bytes
      // and later queued messages before asking the engine to emit Close.
      record.closeRequest = { code: code === undefined && reason.length ? 1000 : code, reason };
      flush(this);
    }
  }

  for (const [name, value] of Object.entries({ CONNECTING: 0, OPEN: 1, CLOSING: 2, CLOSED: 3 })) {
    define(WebSocket, name, { value, enumerable: true });
    define(WebSocket.prototype, name, { value, enumerable: true });
  }
  for (const type of ['open', 'message', 'error', 'close']) {
    define(WebSocket.prototype, 'on' + type, {
      enumerable: true,
      configurable: true,

      get() {
        state(this);
        return getEventAttributeValue(this, type);
      },

      set(value) {
        state(this);
        setEventAttributeValue(this, type, value);
      }
    });
  }
  define(WebSocket.prototype, Symbol.toStringTag, { value: 'WebSocket', configurable: true });
  for (const name of [
    'url',
    'readyState',
    'protocol',
    'extensions',
    'bufferedAmount',
    'binaryType',
    'send',
    'close'
  ]) {
    const descriptor = Object.getOwnPropertyDescriptor(WebSocket.prototype, name);
    define(WebSocket.prototype, name, { ...descriptor, enumerable: true });
  }
  return { WebSocket, MessageEvent, CloseEvent };
}
