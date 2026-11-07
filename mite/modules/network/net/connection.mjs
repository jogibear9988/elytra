/**
 * Own bounded native connection attempts, retry timers and transport generation
 * checks. Socket state stays in the socket factory's WeakMap. Native callbacks
 * return through its event dispatcher; late callbacks cannot adopt a new handle.
 */
export default function createConnectionAttempts(host, policy, sockets, socketEvent) {
  const native = host.native;
  const tick = host.process.nextTick;
  const { error, systemError } = policy;

  function handle(socket) {
    const state = sockets.get(socket);
    const id = native.create((type, value) => {
      if (state.id === id) {
        socketEvent(socket, type, value);
      }
    });
    state.id = id;
    if (!state.referenced) {
      native.ref(id, false);
    }
    if (state.noDelay !== undefined) {
      socket.setNoDelay(state.noDelay);
    }
    if (state.keepAlive !== undefined) {
      socket.setKeepAlive(state.keepAlive, state.keepAliveInitialDelay);
    }
  }

  function clearAttempt(state) {
    if (state.attemptTimer) {
      clearTimeout(state.attemptTimer);
    }
    state.attemptTimer = null;
  }

  function attempt(socket) {
    const state = sockets.get(socket);
    if (socket.destroyed) {
      return;
    }
    const target = state.addresses.shift();
    state.host = target.address;
    state.family = target.family;
    if (state.id === null) {
      handle(socket);
    }
    if (state.multiple) {
      socket.autoSelectFamilyAttemptedAddresses.push(`${state.host}:${state.port}`);
    }
    socket.emit('connectionAttempt', state.host, state.port, state.family);
    if (socket.destroyed) {
      return;
    }
    const status = native.connect(state.id, state.host, state.port);
    if (status) {
      tick(() => {
        if (!socket.destroyed) {
          connectFailure(socket, status);
        }
      });
    } else if (state.addresses.length) {
      state.attemptTimer = setTimeout(() => {
        state.attemptTimer = null;
        if (socket.destroyed) {
          return;
        }
        const failure = error(Error, 'ETIMEDOUT', 'Connection attempt timed out');
        failure.syscall = 'connect';
        failure.address = state.host;
        failure.port = state.port;
        state.failures.push(failure);
        socket.emit('connectionAttemptTimeout', state.host, state.port, state.family);
        if (socket.destroyed) {
          return;
        }
        native.close(state.id);
        state.id = null;
        attempt(socket);
      }, state.attemptTimeout).unref();
    }
  }

  /** Preserve the createConnection/connect overloads while returning a fresh Socket. */
  function connectFailure(socket, status) {
    const state = sockets.get(socket);
    clearAttempt(state);
    const failure = systemError(status, 'connect', state.host, state.port);
    state.failures.push(failure);
    socket.emit('connectionAttemptFailed', state.host, state.port, state.family, failure);
    if (socket.destroyed) {
      return;
    }
    if (state.addresses.length) {
      native.close(state.id);
      state.id = null;
      attempt(socket);
      return;
    }
    socket.connecting = false;
    if (state.multiple) {
      const e = new AggregateError(state.failures);
      e.code = state.failures[0].code;
      socket.destroy(e);
    } else {
      socket.destroy(failure);
    }
  }

  return { handle, clearAttempt, attempt, connectFailure };
}
