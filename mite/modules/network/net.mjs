/**
 * TCP Socket/Server state machines on the native transport. This layer coordinates stream
 * pressure, lookup/retry policy and close/error ordering.
 *
 * Native dependencies are supplied by the embedding loader. One factory call
 * constructs the shared builtin instance for one runtime/realm.
 */
export default function createNetworkModule(host) {
  const native = host.native;
  const { Duplex } = host.stream;
  const EventEmitter = host.EventEmitter;
  const Buffer = host.Buffer;
  const tick = host.process.nextTick;
  const dnsLookup = host.dns.lookup;
  const abort = host.abort;
  const sockets = new WeakMap();
  const servers = new WeakMap();

  function error(Type, code, message) {
    const e = new Type(message);
    e.code = code;
    return e;
  }

  function unsupported(name) {
    throw error(Error, 'ERR_MITE_UNSUPPORTED_API', `${name} is not implemented`);
  }

  function validateSignal(signal) {
    if (signal !== undefined && !abort.isSignal(signal)) {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'signal must be an AbortSignal');
    }
    return signal;
  }

  function abortError(signal) {
    const failure = error(Error, 'ABORT_ERR', 'The operation was aborted');
    failure.name = 'AbortError';
    failure.cause = abort.reason(signal);
    return failure;
  }

  /** Dispose at resource shutdown, including before a delayed lookup can complete. */
  function releaseSignal(state) {
    state.unsubscribe?.();
    state.unsubscribe = null;
  }

  function port(value) {
    if (typeof value !== 'number' && typeof value !== 'string') {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'port must be a number or string');
    }
    const number = Number(value);
    if (
      (typeof value === 'string' && value.trim() === '') ||
      !Number.isInteger(number) ||
      number < 0 ||
      number > 65535
    ) {
      throw error(RangeError, 'ERR_SOCKET_BAD_PORT', `Invalid port: ${String(value)}`);
    }
    return number;
  }

  function ip(value) {
    if (typeof value !== 'string') {
      return 0;
    }
    const zone = value.indexOf('%');
    if (zone !== -1 && !/^[0-9A-Za-z.:-]+$/.test(value.slice(zone + 1))) {
      return 0;
    }
    return native.isIP(value);
  }

  function options(args) {
    const list = [...args];
    let callback;
    if (typeof list.at(-1) === 'function') {
      callback = list.pop();
    }
    let result;
    if (list[0] !== null && typeof list[0] === 'object') {
      result = { ...list[0] };
    } else if (typeof list[0] === 'string' && Number.isNaN(Number(list[0]))) {
      unsupported('Unix sockets and named pipes');
    } else {
      result = {
        port: list[0],
        host: typeof list[1] === 'string' ? list[1] : undefined,
        backlog: typeof list[1] === 'number' ? list[1] : list[2]
      };
    }
    return [result, callback];
  }

  function hostname(value, fallback) {
    value = value || fallback;
    if (typeof value !== 'string') {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'host must be a string');
    }

    return value;
  }

  function systemError(status, syscall, address, port) {
    const info = native.error(status);
    const e = error(
      Error,
      info.code,
      `${syscall} ${info.code}: ${info.message}${address === undefined ? '' : ` ${address}:${port}`}`
    );
    e.errno = status;
    e.syscall = syscall;
    if (address !== undefined) {
      e.address = address;
      e.port = port;
    }
    return e;
  }

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

  function socketEvent(socket, type, value) {
    const state = sockets.get(socket);
    if (type === 'close') {
      state.id = null;
      if (state.closeCallback) {
        const cb = state.closeCallback;
        state.closeCallback = null;
        cb(state.destroyError);
      }
      return;
    }
    if (socket.destroyed) {
      return;
    }
    if (type === 'connect') {
      if (value) {
        connectFailure(socket, value);
        return;
      }
      clearAttempt(state);
      socket.connecting = false;
      state.connected = true;
      socket.emit('connect');
      socket.emit('ready');
      if (state.writePending) {
        const write = state.writePending.run;
        state.writePending = null;
        write();
      }
      socket._read();
    } else if (type === 'data') {
      socket.bytesRead += value.byteLength;
      refreshTimeout(socket);
      if (!socket.push(Buffer.from(value.buffer, value.byteOffset, value.byteLength))) {
        native.read(state.id, false);
      }
    } else if (type === 'end') {
      socket.push(null);
      socket.read(0);
    } else if (type === 'error') {
      socket.destroy(systemError(value, 'read'));
    }
  }

  function refreshTimeout(socket) {
    const state = sockets.get(socket);
    if (state.timer) {
      clearTimeout(state.timer);
    }
    state.timer = null;
    if (socket.timeout > 0 && !socket.destroyed) {
      state.timer = setTimeout(() => {
        state.timer = null;
        socket.emit('timeout');
      }, socket.timeout).unref();
    }
  }

  /** A Duplex stream owns one native transport and coordinates connect/read/write/close state. */
  class Socket extends Duplex {
    constructor(opts = {}) {
      if (typeof opts === 'number') {
        unsupported('Socket descriptor adoption');
      }
      if (opts === null || typeof opts !== 'object') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'options must be an object');
      }
      for (const key of ['fd', 'onread', 'handle', 'blockList']) {
        if (opts[key] !== undefined) {
          unsupported(`Socket option ${key}`);
        }
      }
      for (const key of ['objectMode', 'readableObjectMode', 'writableObjectMode']) {
        if (opts[key]) {
          throw error(TypeError, 'ERR_INVALID_ARG_VALUE', `${key} is not supported for sockets`);
        }
      }
      // Node ignores falsy constructor signals. Do not pass our public signal to
      // readable-stream's separate private AbortController implementation.
      const signal = validateSignal(opts.signal || undefined);
      super({
        ...opts,
        signal: undefined,
        autoDestroy: true,
        emitClose: true,
        objectMode: false,
        readableObjectMode: false,
        writableObjectMode: false,
        allowHalfOpen: opts.allowHalfOpen ?? false,
        decodeStrings: true
      });
      const state = {
        addresses: [],
        failures: [],
        attemptTimer: null,
        attemptTimeout: 250,
        multiple: false,
        id: null,
        connected: false,
        closeCallback: null,
        destroyError: null,
        writePending: null,
        timer: null,
        referenced: true,
        noDelay: opts.noDelay,
        keepAlive: opts.keepAlive,
        keepAliveInitialDelay: opts.keepAliveInitialDelay ?? 0,
        unsubscribe: null
      };
      sockets.set(this, state);
      // Accepted channels are private to this factory, never user-supplied ids.
      this.connecting = false;
      this.bytesRead = 0;
      this.bytesWritten = 0;
      this.timeout = 0;
      this._hadError = false;
      if (signal !== undefined) {
        const cancel = () => this.destroy(abortError(signal));

        if (signal.aborted) {
          // Let the caller install error listeners before a pre-aborted socket fails.
          tick(() => {
            if (!this.destroyed) {
              cancel();
            }
          });
        } else {
          state.unsubscribe = abort.subscribe(signal, cancel);
        }
      }
    }

    emit(type, ...args) {
      return super.emit(type, ...(type === 'close' ? [this._hadError] : args));
    }

    connect(...args) {
      const state = sockets.get(this);
      if (this.connecting) {
        throw error(Error, 'ERR_SOCKET_CONNECTING', 'Socket is already connecting');
      }
      if (state.connected) {
        throw error(Error, 'ERR_SOCKET_ALREADY_BOUND', 'Socket is already connected');
      }
      if (this.destroyed) {
        unsupported('Reconnecting a destroyed Socket');
      }
      const [opts, cb] = options(args);
      for (const key of ['path', 'fd', 'localAddress', 'localPort', 'signal']) {
        if (opts[key] !== undefined) {
          unsupported(`connect option ${key}`);
        }
      }
      const targetPort = port(opts.port);
      const targetHost = hostname(opts.host, 'localhost');
      let family = opts.family ?? 0;
      if (family === 'IPv4') {
        family = 4;
      }
      if (family === 'IPv6') {
        family = 6;
      }
      if (![0, 4, 6].includes(family)) {
        throw error(TypeError, 'ERR_INVALID_ARG_VALUE', 'Invalid address family');
      }
      const lookup = opts.lookup ?? dnsLookup;
      if (typeof lookup !== 'function') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'lookup must be a function');
      }
      if (opts.autoSelectFamily !== undefined && typeof opts.autoSelectFamily !== 'boolean') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'autoSelectFamily must be a boolean');
      }
      const timeout = opts.autoSelectFamilyAttemptTimeout ?? 250;
      if (
        typeof timeout !== 'number' ||
        !Number.isInteger(timeout) ||
        timeout < 1 ||
        timeout > 2147483647
      ) {
        throw error(RangeError, 'ERR_OUT_OF_RANGE', 'Invalid family attempt timeout');
      }
      if (cb) {
        this.once('connect', cb);
      }
      state.port = targetPort;
      state.attemptTimeout = Math.max(10, timeout);
      this.connecting = true;
      const numeric = ip(targetHost);
      if (numeric) {
        state.addresses = [{ address: targetHost, family: numeric }];
        handle(this);
        attempt(this);
      } else {
        handle(this);
        const generation = state.id;
        const all = family === 0 && opts.autoSelectFamily !== false;
        let called = false;
        let hints = opts.hints ?? 0;
        if (!hints && family === 0 && host.platform !== 'win32') {
          hints = host.dns.ADDRCONFIG;
        }
        try {
          lookup(
            targetHost,
            { family: family || undefined, hints, ...(all ? { all: true } : {}) },
            (err, address, returnedFamily) => {
              if (called) {
                return;
              }
              called = true;
              if (this.destroyed || state.id !== generation) {
                return;
              }
              if (err) {
                this.emit('lookup', err, undefined, undefined, targetHost);
                this.connecting = false;
                this.destroy(err);
                return;
              }
              if (!all) {
                this.emit('lookup', null, address, returnedFamily, targetHost);
              }
              if (this.destroyed) {
                return;
              }
              const values = all ? address : [{ address, family: returnedFamily }];
              if (!Array.isArray(values) || !values.length) {
                this.destroy(
                  error(TypeError, 'ERR_INVALID_IP_ADDRESS', 'Lookup returned no addresses')
                );
                return;
              }
              const seen = new Set();
              const valid = [];
              for (const entry of values) {
                if (all) {
                  this.emit('lookup', null, entry?.address, entry?.family, targetHost);
                }
                if (this.destroyed) {
                  return;
                }
                const kind = entry && ip(entry.address);
                if (!kind) {
                  this.destroy(
                    error(TypeError, 'ERR_INVALID_IP_ADDRESS', 'Invalid address returned by lookup')
                  );
                  return;
                }
                if (entry.family !== 4 && entry.family !== 6) {
                  this.destroy(
                    error(
                      RangeError,
                      'ERR_INVALID_ADDRESS_FAMILY',
                      'Invalid family returned by lookup'
                    )
                  );
                  return;
                }
                if (kind !== entry.family) {
                  this.destroy(
                    error(
                      TypeError,
                      'ERR_INVALID_IP_ADDRESS',
                      'Address family does not match lookup result'
                    )
                  );
                  return;
                }
                if (!seen.has(entry.address)) {
                  seen.add(entry.address);
                  valid.push(entry);
                }
              }
              // Alternate address families, preserving order within
              // each family, before bounded connection attempts.
              const first = valid[0].family;
              const a = valid.filter((v) => v.family === first);
              const b = valid.filter((v) => v.family !== first);
              const ordered = [];
              while (a.length || b.length) {
                if (a.length) {
                  ordered.push(a.shift());
                }
                if (b.length) {
                  ordered.push(b.shift());
                }
              }
              state.addresses = ordered;
              state.multiple = all && ordered.length > 1;
              if (state.multiple) {
                this.autoSelectFamilyAttemptedAddresses = [];
              }
              attempt(this);
            }
          );
        } catch (err) {
          this.destroy();
          throw err;
        }
      }
      return this;
    }

    _read() {
      const state = sockets.get(this);
      if (state.connected && state.id !== null && !this.destroyed) {
        const status = native.read(state.id, true);
        if (status) {
          this.destroy(systemError(status, 'read'));
        }
      }
    }

    _write(chunk, encoding, callback) {
      const state = sockets.get(this);
      if (this.connecting) {
        state.writePending = { run: () => this._write(chunk, encoding, callback), callback };
        return;
      }
      if (!state.connected || state.id === null) {
        callback(error(Error, 'ERR_SOCKET_CLOSED', 'Socket is closed'));
        return;
      }
      this.bytesWritten += chunk.length;
      refreshTimeout(this);
      const status = native.write(state.id, chunk, (status) => {
        refreshTimeout(this);
        callback(status ? systemError(status, 'write') : undefined);
      });
      if (status) {
        callback(status < 0 ? systemError(status, 'write') : undefined);
      }
    }

    _final(callback) {
      const state = sockets.get(this);
      if (this.connecting) {
        this.once('connect', () => this._final(callback));

        return;
      }
      if (!state.connected || state.id === null) {
        callback();
        return;
      }
      const status = native.shutdown(state.id, (status) =>
        callback(status ? systemError(status, 'shutdown') : undefined)
      );
      if (status) {
        callback(systemError(status, 'shutdown'));
      }
    }

    _destroy(err, callback) {
      const state = sockets.get(this);
      this._hadError = !!err;
      this.connecting = false;
      state.connected = false;
      state.destroyError = err;
      releaseSignal(state);
      clearAttempt(state);
      if (state.timer) {
        clearTimeout(state.timer);
      }
      state.timer = null;
      if (state.writePending) {
        const cb = state.writePending.callback;
        state.writePending = null;
        // A write waiting for connect never reached the transport. Its callback
        // reports the closed-before-connect condition; the socket error event
        // separately carries the abort reason or original connection failure.
        cb(
          error(
            Error,
            'ERR_SOCKET_CLOSED_BEFORE_CONNECTION',
            'Socket closed before the connection was established'
          )
        );
      }
      if (state.id === null) {
        callback(err);
        return;
      }
      state.closeCallback = callback;
      native.close(state.id);
    }

    address() {
      const id = sockets.get(this).id;
      return id === null ? {} : (native.address(id, false) ?? {});
    }

    get remoteAddress() {
      const s = sockets.get(this);
      return s.id === null ? undefined : native.address(s.id, true)?.address;
    }

    get remotePort() {
      const s = sockets.get(this);
      return s.id === null ? undefined : native.address(s.id, true)?.port;
    }

    get remoteFamily() {
      const s = sockets.get(this);
      return s.id === null ? undefined : native.address(s.id, true)?.family;
    }

    get localAddress() {
      return this.address().address;
    }

    get localPort() {
      return this.address().port;
    }

    get localFamily() {
      return this.address().family;
    }

    get pending() {
      const s = sockets.get(this);
      return !s.connected || this.connecting;
    }

    get readyState() {
      return this.connecting
        ? 'opening'
        : this.readable && this.writable
          ? 'open'
          : this.readable
            ? 'readOnly'
            : this.writable
              ? 'writeOnly'
              : 'closed';
    }

    get bufferSize() {
      return this.writableLength;
    }

    setNoDelay(value = true) {
      const state = sockets.get(this);
      state.noDelay = !!value;
      const id = state.id;
      if (id !== null) {
        native.option(id, 0, !!value, 0);
      }
      return this;
    }

    setKeepAlive(value = false, delay = 0) {
      if (typeof delay !== 'number') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'initialDelay must be a number');
      }
      const state = sockets.get(this);
      state.keepAlive = !!value;
      state.keepAliveInitialDelay = delay;
      const id = state.id;
      if (id !== null) {
        native.option(
          id,
          1,
          !!value,
          Math.max(1, Math.min(2147483647, Math.trunc(delay / 1000) || 1))
        );
      }
      return this;
    }

    setTimeout(value, callback) {
      if (typeof value !== 'number') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'timeout must be a number');
      }
      if (!Number.isFinite(value) || value < 0) {
        throw error(RangeError, 'ERR_OUT_OF_RANGE', 'Invalid timeout');
      }
      if (callback) {
        this.once('timeout', callback);
      }
      this.timeout = value;
      refreshTimeout(this);
      return this;
    }

    ref() {
      const s = sockets.get(this);
      s.referenced = true;
      if (s.id !== null) {
        native.ref(s.id, true);
      }
      return this;
    }

    unref() {
      const s = sockets.get(this);
      s.referenced = false;
      if (s.id !== null) {
        native.ref(s.id, false);
      }
      return this;
    }

    resetAndDestroy() {
      unsupported('Socket.resetAndDestroy');
    }

    destroySoon() {
      if (this.writableFinished) {
        this.destroy();
      } else {
        this.end(() => this.destroy());
      }
    }
  }

  function accepted(id, opts) {
    const socket = new Socket(opts);
    const state = sockets.get(socket);
    state.id = id;
    state.connected = true;

    native.setCallback(id, (type, value) => socketEvent(socket, type, value));

    if (opts.noDelay !== undefined) {
      socket.setNoDelay(opts.noDelay);
    }
    if (opts.keepAlive !== undefined) {
      socket.setKeepAlive(opts.keepAlive, opts.keepAliveInitialDelay ?? 0);
    }
    return socket;
  }

  function finishClose(server) {
    const state = servers.get(server);
    if (!state.closing || state.id !== null || state.connections.size) {
      return;
    }
    state.closing = false;

    tick(() => server.emit('close'));
  }

  /** Track accepted connections so close waits for the native listener and active sockets. */
  class Server extends EventEmitter {
    constructor(opts = {}, listener) {
      super();
      if (typeof opts === 'function') {
        listener = opts;
        opts = {};
      }
      if (opts === null || typeof opts !== 'object') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'options must be an object');
      }
      for (const key of ['signal', 'blockList']) {
        if (opts[key] !== undefined) {
          unsupported(`Server option ${key}`);
        }
      }
      servers.set(this, {
        id: null,
        connections: new Set(),
        closing: false,
        opts: { ...opts },
        referenced: true,
        unsubscribe: null
      });
      this.listening = false;
      if (listener) {
        this.on('connection', listener);
      }
    }

    listen(...args) {
      const state = servers.get(this);
      if (state.closing) {
        unsupported('listen while Server.close is pending');
      }
      if (state.id !== null) {
        throw error(Error, 'ERR_SERVER_ALREADY_LISTEN', 'Listen called more than once');
      }
      const [opts, callback] = options(args);
      for (const key of ['path', 'fd', 'handle', 'reusePort']) {
        if (opts[key] !== undefined) {
          unsupported(`listen option ${key}`);
        }
      }
      const signal = validateSignal(opts.signal);
      const listenPort = port(opts.port ?? 0);
      const listenHost = hostname(opts.host, '::');
      const backlog = opts.backlog === undefined ? 511 : Number(opts.backlog);
      if (!Number.isInteger(backlog) || backlog < 1 || backlog > 2147483647) {
        throw error(RangeError, 'ERR_OUT_OF_RANGE', 'Invalid backlog');
      }
      if (callback) {
        this.once('listening', callback);
      }
      state.closing = false;
      const id = native.create((type, value) => {
        if (state.id !== id) {
          return;
        }
        if (type === 'close') {
          state.id = null;
          finishClose(this);
        } else if (type === 'error') {
          this.emit('error', systemError(value, 'accept'));
        } else if (type === 'connection') {
          const socket = accepted(value, state.opts);
          state.connections.add(socket);

          socket.once('close', () => {
            state.connections.delete(socket);
            finishClose(this);
          });

          this.emit('connection', socket);
          if (!state.opts.pauseOnConnect) {
            socket.read(0);
          }
        }
      });
      state.id = id;
      if (!state.referenced) {
        native.ref(state.id, false);
      }

      if (signal !== undefined) {
        const cancel = () => {
          if (state.id === id) {
            this.close();
          }
        };

        if (signal.aborted) {
          tick(cancel);
        } else {
          state.unsubscribe = abort.subscribe(signal, cancel);
        }
      }

      const bind = (address) => {
        if (state.id !== id || state.closing) {
          return;
        }
        const status = native.listen(id, address, listenPort, backlog, !!opts.ipv6Only);
        if (status) {
          releaseSignal(state);
          native.close(id);
          state.id = null;

          tick(() => this.emit('error', systemError(status, 'listen', address, listenPort)));
        } else {
          this.listening = true;

          tick(() => {
            if (this.listening) {
              this.emit('listening');
            }
          });
        }
      };

      if (ip(listenHost)) {
        // Match asynchronous listen startup and let a pre-aborted signal close
        // the allocated transport before binding or delivering listening.
        tick(() => bind(listenHost));
      } else {
        dnsLookup(listenHost, (err, address) => {
          if (state.id !== id || state.closing) {
            return;
          }
          if (err) {
            releaseSignal(state);
            native.close(id);
            state.id = null;
            this.emit('error', err);
          } else {
            bind(address);
          }
        });
      }
      return this;
    }

    address() {
      const id = servers.get(this).id;
      return this.listening && id !== null ? native.address(id, false) : null;
    }

    close(callback) {
      const state = servers.get(this);
      const running = this.listening;
      releaseSignal(state);
      if (callback) {
        this.once('close', () =>
          callback(
            running ? undefined : error(Error, 'ERR_SERVER_NOT_RUNNING', 'Server is not running')
          )
        );
      }
      this.listening = false;
      state.closing = true;
      if (state.id !== null) {
        native.close(state.id);
      } else {
        finishClose(this);
      }
      return this;
    }

    getConnections(callback) {
      if (typeof callback !== 'function') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'callback must be a function');
      }
      const count = servers.get(this).connections.size;

      tick(() => callback(null, count));
    }

    ref() {
      const s = servers.get(this);
      s.referenced = true;
      if (s.id !== null) {
        native.ref(s.id, true);
      }
      return this;
    }

    unref() {
      const s = servers.get(this);
      s.referenced = false;
      if (s.id !== null) {
        native.ref(s.id, false);
      }
      return this;
    }

    async [Symbol.asyncDispose]() {
      if (!this.listening) {
        return;
      }
      await new Promise((resolve, reject) => this.close((err) => (err ? reject(err) : resolve())));
    }
  }

  function createServer(...args) {
    return new Server(...args);
  }

  function connect(...args) {
    const [opts, callback] = options(args);
    const socket = new Socket(opts);
    try {
      if (opts.timeout) {
        socket.setTimeout(opts.timeout);
      }
      // The public signal belongs to Socket construction. Direct Socket.connect
      // signal options remain explicitly unsupported rather than silently ignored.
      return socket.connect({ ...opts, signal: undefined }, callback);
    } catch (failure) {
      // A failed factory call never exposes its Socket. Release any signal and
      // native state rather than retaining that abandoned object until abort.
      socket.destroy();
      throw failure;
    }
  }

  return {
    Socket,
    Server,
    createServer,
    connect,
    createConnection: connect,
    isIP: ip,

    isIPv4: (value) => ip(value) === 4,

    isIPv6: (value) => ip(value) === 6
  };
}
