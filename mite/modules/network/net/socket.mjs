/**
 * A Duplex owns its native TCP handle, byte pressure, abort subscription and
 * close/error ordering. Accepted and outbound sockets use the same constructor
 * and WeakMap; accepted native IDs are supplied only by the private server seam.
 */
import createConnectionAttempts from './connection.mjs';

export default function createSockets(host, policy) {
  const native = host.native;
  const { Duplex } = host.stream;
  const Buffer = host.Buffer;
  const tick = host.process.nextTick;
  const dnsLookup = host.dns.lookup;
  const abort = host.abort;
  const {
    error,
    unsupported,
    validateSignal,
    abortError,
    releaseSignal,
    port,
    ip,
    options,
    hostname,
    systemError
  } = policy;
  const sockets = new WeakMap();
  const { handle, clearAttempt, attempt, connectFailure } = createConnectionAttempts(
    host,
    policy,
    sockets,
    socketEvent
  );

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

  return { Socket, accepted, connect };
}
