/** JSON-only inherited IPC. Own snapshots and bounded writes independently of child process handles. */
export default function createIpcChannel(config) {
  const {
    target,
    Buffer,
    tick,
    native,
    systemError,
    childMode = false,
    onClosed = () => {}
  } = config;
  const writes = new Set();
  const messages = [];
  let bytes = 0;
  let messageBytes = 0;
  let scheduled = false;
  let incoming = Buffer.alloc(0);
  let closed = false;
  let disconnecting = false;
  let shutting = false;
  let announced = false;
  let explicitRef = null;

  function error(code, message, Type = Error) {
    return Object.assign(new Type(message), { code });
  }

  function updateRef() {
    if (closed) {
      return;
    }
    const listeners = target.listenerCount('message') + target.listenerCount('disconnect');
    native.ref(explicitRef ?? (writes.size > 0 || !childMode || listeners > 0));
  }

  function report(failure, callback) {
    tick(() => {
      if (callback) {
        callback(failure);
      } else {
        target.emit('error', failure);
      }
    });
  }

  function removeHooks() {
    if (closed && messages.length === 0) {
      target.removeListener('newListener', listenerAdded);
      target.removeListener('removeListener', listenerRemoved);
    }
  }

  function schedule() {
    if (scheduled) {
      return;
    }
    scheduled = true;

    tick(() => {
      scheduled = false;
      for (let index = 0; index < messages.length; ) {
        const value = messages[index];
        if (target.listenerCount(value.event) === 0) {
          index++;
          continue;
        }
        messages.splice(index, 1);
        messageBytes -= value.bytes;
        // A callback may remove a listener or queue another message. Recheck each delivery.
        target.emit(value.event, value.message, undefined);
      }
      removeHooks();
    });
  }

  function announce() {
    if (announced) {
      return;
    }
    announced = true;
    target.connected = false;
    target.channel = null;

    tick(() => target.emit('disconnect'));
  }

  function close() {
    if (closed) {
      return;
    }
    closed = true;
    incoming = Buffer.alloc(0);
    removeHooks();
    native.close();
    announce();
  }

  function drain() {
    if (!disconnecting || closed || writes.size || shutting) {
      return;
    }
    shutting = true;
    const status = native.shutdown((status) => {
      if (status && !closed) {
        // An already disconnected peer is a completed disconnect, not a new send failure.
        const failure = systemError(status, 'shutdown');
        if (failure.code !== 'ENOTCONN' && failure.code !== 'EPIPE') {
          report(failure);
        }
      }
      close();
    });
    if (status) {
      close();
    }
  }

  function completed(write, status) {
    if (!writes.delete(write)) {
      return;
    }
    bytes -= write.bytes;
    const callback = write.callback;
    write.callback = null;
    if (status) {
      report(systemError(status, 'write'), callback);
    } else if (callback) {
      tick(() => callback(null));
    }
    updateRef();
    drain();
  }

  function send(message, handle, options, callback) {
    if (typeof handle === 'function') {
      callback = handle;
      handle = undefined;
      options = undefined;
    } else if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }
    // Node reports channel closure before validating/serializing a new message.
    if (closed || disconnecting || !target.connected) {
      report(
        error('ERR_IPC_CHANNEL_CLOSED', 'Channel closed'),
        typeof callback === 'function' ? callback : undefined
      );
      return false;
    }
    if (callback !== undefined && typeof callback !== 'function') {
      throw error('ERR_INVALID_ARG_TYPE', 'IPC callback must be a function', TypeError);
    }
    if (handle !== undefined && handle !== null) {
      throw error('ERR_MITE_UNSUPPORTED_API', 'IPC handle passing is not implemented');
    }
    if (
      options !== undefined &&
      (options === null || typeof options !== 'object' || Array.isArray(options))
    ) {
      throw error('ERR_INVALID_ARG_TYPE', 'IPC options must be an object', TypeError);
    }
    if (options && Reflect.ownKeys(options).length) {
      throw error('ERR_MITE_UNSUPPORTED_API', 'IPC send options are not implemented');
    }
    if (message === undefined) {
      throw error('ERR_MISSING_ARGS', 'IPC message is required', TypeError);
    }
    if (!['string', 'number', 'boolean', 'object'].includes(typeof message)) {
      throw error('ERR_INVALID_ARG_TYPE', 'IPC message must be JSON data', TypeError);
    }
    const json = JSON.stringify(message);
    if (json === undefined) {
      throw error('ERR_INVALID_ARG_TYPE', 'IPC message must serialize to JSON', TypeError);
    }
    const wire = Buffer.from(json + '\n', 'utf8');
    if (wire.byteLength > 1048576) {
      throw error('ERR_IPC_CHANNEL_LIMIT', 'IPC frame exceeds 1 MiB', RangeError);
    }
    // JSON getters/toJSON may disconnect the endpoint during snapshotting.
    if (closed || disconnecting || !target.connected) {
      report(error('ERR_IPC_CHANNEL_CLOSED', 'Channel closed'), callback);
      return false;
    }
    if (writes.size >= 64 || bytes + wire.byteLength > 4194304) {
      report(error('ERR_IPC_CHANNEL_LIMIT', 'IPC write backlog exceeds its budget'), callback);
      return false;
    }
    const write = { bytes: wire.byteLength, callback };
    writes.add(write);
    bytes += wire.byteLength;
    updateRef();
    const status = native.write(wire, (status) => completed(write, status));
    if (status) {
      completed(write, status);
      return false;
    }
    return bytes < 131072;
  }

  function disconnect() {
    if (closed || disconnecting || !target.connected) {
      report(error('ERR_IPC_DISCONNECTED', 'IPC channel is already disconnected'));
      return;
    }
    target.connected = false;
    disconnecting = true;
    drain();
  }

  function receive(kind, value) {
    if (kind === 'pipeClose') {
      if (!closed) {
        closed = true;
        incoming = Buffer.alloc(0);
        removeHooks();
        announce();
      }
      // disconnect was queued first; ChildProcess.close must not overtake that event.
      tick(onClosed);
      return;
    }
    if (closed) {
      return;
    }
    if (kind === 'pipeError') {
      report(systemError(value, 'read'));
      close();
      return;
    }
    if (kind === 'end') {
      if (incoming.length) {
        report(error('ERR_IPC_CHANNEL_DATA', 'Incomplete IPC JSON frame'));
      }
      target.connected = false;
      disconnecting = true;
      drain();
      return;
    }
    if (kind !== 'data') {
      return;
    }
    incoming = Buffer.concat([
      incoming,
      Buffer.from(value.buffer, value.byteOffset, value.byteLength)
    ]);
    while (!closed) {
      const newline = incoming.indexOf(10);
      if (newline === -1) {
        if (incoming.length >= 1048576) {
          report(error('ERR_IPC_CHANNEL_DATA', 'IPC JSON frame exceeds budget'));
          close();
        }
        return;
      }
      const frame = incoming.subarray(0, newline);
      incoming = incoming.subarray(newline + 1);
      let message;
      try {
        if (newline + 1 > 1048576) {
          throw Error('IPC JSON frame exceeds budget');
        }
        const json = frame.toString('utf8');
        if (!Buffer.from(json, 'utf8').equals(frame)) {
          throw Error('Invalid IPC UTF-8');
        }
        message = JSON.parse(json);
      } catch {
        report(error('ERR_IPC_CHANNEL_DATA', 'Invalid IPC JSON frame'));
        close();
        return;
      }
      const internal = typeof message?.cmd === 'string' && message.cmd.startsWith('NODE_');
      if (messages.length >= 64 || messageBytes + frame.length + 1 > 4194304) {
        report(error('ERR_IPC_CHANNEL_LIMIT', 'IPC delivery backlog exceeds its budget'));
        close();
        return;
      }
      messages.push({
        event: internal ? 'internalMessage' : 'message',
        message,
        bytes: frame.length + 1
      });
      messageBytes += frame.length + 1;
      schedule();
    }
  }

  function listenerAdded(name) {
    if (name === 'message' || name === 'internalMessage') {
      schedule();
    }
    if (name === 'message' || name === 'disconnect') {
      // newListener runs before insertion; adding either listener explicitly refs child IPC.
      if (!closed && childMode && explicitRef === null) {
        native.ref(true);
      }
    }
  }

  function listenerRemoved(name) {
    if (name === 'message' || name === 'disconnect') {
      updateRef();
    }
  }

  const channel = {
    ref() {
      explicitRef = true;
      updateRef();
    },

    unref() {
      explicitRef = false;
      updateRef();
    }
  };
  target.connected = true;
  target.channel = channel;
  target.on('newListener', listenerAdded);
  target.on('removeListener', listenerRemoved);
  if (childMode) {
    target.send = send;
    target.disconnect = disconnect;
  }
  updateRef();
  return { send, disconnect, receive, close };
}
