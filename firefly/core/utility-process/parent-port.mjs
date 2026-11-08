/** Utility-realm parentPort: bounded binary snapshots over the native parent channel. */
export default function createParentPort(native, EventEmitter, codec) {
  const schedule = setImmediate;
  const queue = [];
  let queuedBytes = 0;
  let scheduled = false;
  let started = false;
  let closed = false;
  let retired = false;

  function failure(code, message) {
    return Object.assign(new Error(message), { code });
  }

  /** Stop accepting work before closing native state, which can call us back. */
  function abort(error) {
    if (retired) {
      return;
    }
    closed = true;
    retired = true;
    queue.length = 0;
    queuedBytes = 0;
    native.close();

    schedule(() => port.emit('error', error));
  }

  /** Deliver one event per turn. User listeners may add work or remove themselves. */
  function pump() {
    if (retired || scheduled || !started || !queue.length) {
      return;
    }
    scheduled = true;

    schedule(() => {
      scheduled = false;
      if (retired || !started || !queue.length) {
        return;
      }
      const entry = queue.shift();
      if (entry.close) {
        retired = true;
        port.emit('close');
        return;
      }
      queuedBytes -= entry.wire.byteLength;
      let data;
      try {
        data = codec.decodeIpc(entry.wire);
      } catch {
        abort(failure('ERR_FIREFLY_UTILITY_CHANNEL', 'Invalid utility parent message'));
        return;
      }
      try {
        port.emit('message', { data, ports: [] });
      } finally {
        pump();
      }
    });
  }

  class ParentPort extends EventEmitter {
    /** Registering a message listener starts delivery, including once/prependOnceListener. */
    on(name, listener) {
      super.on(name, listener);
      if (name === 'message') {
        this.start();
      }
      return this;
    }

    addListener(name, listener) {
      super.addListener(name, listener);
      if (name === 'message') {
        this.start();
      }
      return this;
    }

    prependListener(name, listener) {
      super.prependListener(name, listener);
      if (name === 'message') {
        this.start();
      }
      return this;
    }

    /** Explicit start also permits delivery without listeners, matching MessagePort behavior. */
    start() {
      started = true;
      pump();
    }

    postMessage(message, transfer) {
      if (transfer !== undefined && (!Array.isArray(transfer) || transfer.length)) {
        throw failure(
          'ERR_FIREFLY_UNSUPPORTED_API',
          'Utility native MessagePort transfer is not implemented'
        );
      }
      if (closed) {
        throw failure('ERR_FIREFLY_UTILITY_CLOSED', 'Utility parent channel has closed');
      }
      const wire = codec.encodeIpc(message);
      const status = native.send(wire);
      if (status) {
        throw failure(
          'ERR_FIREFLY_UTILITY_CHANNEL',
          `Utility parent channel send failed (${status})`
        );
      }
    }
  }

  const port = new ParentPort();

  native.setHandler((kind, value) => {
    if (closed || retired) {
      return;
    }
    if (kind === 'close') {
      // Keep accepted messages ahead of the terminal event, even in one native batch.
      closed = true;
      queue.push({ close: true });
      pump();
    } else if (kind === 'message') {
      if (!(value instanceof Uint8Array)) {
        abort(failure('ERR_FIREFLY_UTILITY_CHANNEL', 'Invalid utility parent frame'));
        return;
      }
      if (queue.length >= 64 || queuedBytes + value.byteLength > 1048576) {
        abort(failure('ERR_FIREFLY_UTILITY_LIMIT', 'Utility parent queue budget exceeded'));
        return;
      }
      queue.push({ wire: value });
      queuedBytes += value.byteLength;
      pump();
    } else if (kind === 'error') {
      abort(failure('ERR_FIREFLY_UTILITY_CHANNEL', String(value)));
    } else {
      abort(failure('ERR_FIREFLY_UTILITY_CHANNEL', 'Unknown utility parent event'));
    }
  });

  return port;
}
