/** WebSocket event payloads use real Event inheritance and private native-independent slots. */
export default function createWebSocketEvents({ Event }) {
  const messages = new WeakMap();
  const closes = new WeakMap();

  const string = (value) => {
    if (typeof value === 'symbol') {
      throw new TypeError('Cannot convert a Symbol value to a string');
    }
    return String(value);
  };

  function dictionary(value) {
    if (value == null) {
      return {};
    }
    if (typeof value !== 'object' && typeof value !== 'function') {
      throw new TypeError('EventInit must be a dictionary');
    }
    return value;
  }

  class MessageEvent extends Event {
    constructor(type, init = {}) {
      if (!arguments.length) {
        throw new TypeError('MessageEvent requires a type');
      }
      init = dictionary(init);
      super(string(type), init);
      const data = init.data;
      const origin = init.origin;
      const lastEventId = init.lastEventId;
      const source = init.source;
      const suppliedPorts = init.ports;
      const ports = [];
      if (suppliedPorts != null) {
        for (const port of suppliedPorts) {
          ports.push(port);
          break;
        }
      }
      if (ports.length || source != null) {
        const error = new Error('MessageEvent source and port capabilities are not implemented');
        error.code = 'ERR_MITE_UNSUPPORTED_API';
        throw error;
      }
      messages.set(this, {
        data: data === undefined ? null : data,
        origin: origin === undefined ? '' : string(origin),
        lastEventId: lastEventId === undefined ? '' : string(lastEventId),
        ports: Object.freeze(ports)
      });
    }

    get data() {
      return message(this).data;
    }

    get origin() {
      return message(this).origin;
    }

    get lastEventId() {
      return message(this).lastEventId;
    }

    get source() {
      message(this);
      return null;
    }

    get ports() {
      return message(this).ports;
    }
  }

  function message(value) {
    const state = messages.get(value);
    if (!state) {
      throw new TypeError('Invalid MessageEvent receiver');
    }
    return state;
  }

  class CloseEvent extends Event {
    constructor(type, init = {}) {
      if (!arguments.length) {
        throw new TypeError('CloseEvent requires a type');
      }
      init = dictionary(init);
      super(string(type), init);
      const wasClean = init.wasClean;
      const code = init.code;
      const reason = init.reason;
      closes.set(this, {
        wasClean: Boolean(wasClean),
        code: code === undefined ? 0 : +code & 65535,
        reason: reason === undefined ? '' : string(reason)
      });
    }

    get wasClean() {
      return close(this).wasClean;
    }

    get code() {
      return close(this).code;
    }

    get reason() {
      return close(this).reason;
    }
  }

  function close(value) {
    const state = closes.get(value);
    if (!state) {
      throw new TypeError('Invalid CloseEvent receiver');
    }
    return state;
  }

  for (const Type of [MessageEvent, CloseEvent]) {
    Object.defineProperty(Type.prototype, Symbol.toStringTag, {
      value: Type.name,
      configurable: true
    });
    for (const key of Object.getOwnPropertyNames(Type.prototype)) {
      const descriptor = Object.getOwnPropertyDescriptor(Type.prototype, key);
      if (descriptor.get) {
        Object.defineProperty(Type.prototype, key, { ...descriptor, enumerable: true });
      }
    }
  }
  return { MessageEvent, CloseEvent };
}
