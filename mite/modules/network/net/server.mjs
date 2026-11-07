/**
 * Listener and accepted-connection ownership. Closing waits for the native
 * listener and every accepted Socket; lookup/abort callbacks retain their
 * generation checks and release subscriptions before native listener disposal.
 */
export default function createServers(host, policy, accepted) {
  const native = host.native;
  const EventEmitter = host.EventEmitter;
  const tick = host.process.nextTick;
  const dnsLookup = host.dns.lookup;
  const abort = host.abort;
  const {
    error,
    unsupported,
    validateSignal,
    releaseSignal,
    port,
    ip,
    options,
    hostname,
    systemError
  } = policy;
  const servers = new WeakMap();

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

  return { Server, createServer };
}
