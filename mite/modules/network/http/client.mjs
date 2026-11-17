/**
 * One-connection HTTP client: validate options before creating a transport, own
 * parser/backpressure/cancellation state, and release sockets on completion.
 * HTTPS selects its validated transport through the private embedding hook only.
 */
export default function createHttpClient(host, protocol, messages, agents) {
  const Buffer = host.Buffer;
  const URL = host.URL;
  const tick = host.process.nextTick;
  const native = host.native;
  const abort = host.abort;
  const { token, error, unsupported, merged, integer, headerLines, pairs } = protocol;
  const { IncomingMessage, OutgoingMessage, incoming, outgoing } = messages;
  const clients = new WeakMap();
  const clientToken = {};
  const assignedSockets = new WeakMap();
  const descriptor = Object.getOwnPropertyDescriptor;
  const define = Object.defineProperty;
  const apply = Reflect.apply;
  const removeListener = host.net.Socket.prototype.removeListener;
  const plainTransport = Object.freeze({
    protocol: 'http:',
    defaultPort: 80,
    readyEvent: 'connect',

    createSocket(config) {
      return new host.net.Socket({
        noDelay: config.opts.noDelay ?? true,
        keepAlive: config.opts.keepAlive,
        keepAliveInitialDelay: config.opts.keepAliveInitialDelay
      });
    }
  });

  /** Resolve URL/options overloads before opening a connection or publishing callbacks. */
  function clientOptions(input, options, callback, transport) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }
    let base = {};
    if (typeof input === 'string' || input instanceof URL) {
      const url = input instanceof URL ? input : new URL(input);
      base = {
        protocol: url.protocol,
        hostname: url.hostname,
        port: url.port || undefined,
        path: url.pathname + url.search
      };
      if (url.username || url.password) {
        base.auth = decodeURIComponent(url.username) + ':' + decodeURIComponent(url.password);
      }
    } else if (input !== undefined && input !== null) {
      base = { ...input };
    }
    const opts = { ...base, ...options };
    if (callback !== undefined && typeof callback !== 'function') {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'callback must be a function');
    }
    if (opts.protocol !== undefined && opts.protocol !== transport.protocol) {
      throw error(TypeError, 'ERR_INVALID_PROTOCOL', `Expected ${transport.protocol} protocol`);
    }
    // Like Node, an explicit Agent (including false) takes precedence over a
    // request-local connector. A callable connector bypasses only the default Agent.
    const connector =
      opts.agent == null && typeof opts.createConnection === 'function'
        ? opts.createConnection
        : undefined;
    const agent = connector
      ? false
      : opts.agent == null
        ? (transport.globalAgent ?? agents.globalAgent)
        : opts.agent;
    if (agent !== false && (typeof agent !== 'object' || typeof agent.addRequest !== 'function')) {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'agent must supply addRequest or be false');
    }
    if (agent !== false) {
      const expected = agent.protocol;
      if (expected !== undefined && expected !== transport.protocol) {
        throw error(TypeError, 'ERR_INVALID_PROTOCOL', `Expected ${expected} protocol`);
      }
    }
    for (const key of [
      'socketPath',
      'localAddress',
      'localPort',
      'uniqueHeaders',
      'IncomingMessage'
    ]) {
      if (opts[key] !== undefined) {
        unsupported(`HTTP client option ${key}`);
      }
    }
    if (opts.insecureHTTPParser) {
      unsupported('insecureHTTPParser');
    }
    if (opts.setDefaultHeaders === false) {
      unsupported('setDefaultHeaders:false');
    }
    // Like Node's request constructor, falsy signals select no cancellation.
    if (opts.signal && !abort.isSignal(opts.signal)) {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'signal must be an AbortSignal');
    }
    let hostname = opts.hostname ?? opts.host ?? 'localhost';
    if (typeof hostname !== 'string') {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'hostname must be a string');
    }
    if (hostname.startsWith('[') && hostname.endsWith(']')) {
      hostname = hostname.slice(1, -1);
    }
    const defaultPort =
      opts.defaultPort ||
      (agent !== false ? agent.defaultPort : undefined) ||
      transport.defaultPort;
    const port = opts.port ?? defaultPort;
    const number = Number(port);
    if (
      !['number', 'string'].includes(typeof port) ||
      (typeof port === 'string' && !port.trim()) ||
      !Number.isInteger(number) ||
      number < 0 ||
      number > 65535
    ) {
      throw error(RangeError, 'ERR_SOCKET_BAD_PORT', 'Invalid HTTP port');
    }
    let method = opts.method ?? 'GET';
    if (typeof method !== 'string') {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'method must be a string');
    }
    if (!token.test(method)) {
      throw error(TypeError, 'ERR_INVALID_HTTP_TOKEN', 'Invalid HTTP method');
    }
    method = method.toUpperCase();
    if (method === 'CONNECT') {
      unsupported('HTTP CONNECT');
    }
    const path = opts.path ?? '/';
    if (typeof path !== 'string') {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'path must be a string');
    }
    if (/[^\x21-\xff]/.test(path)) {
      throw error(
        TypeError,
        'ERR_UNESCAPED_CHARACTERS',
        'Request path contains unescaped characters'
      );
    }
    const maximum =
      opts.maxHeaderSize === undefined
        ? 16384
        : integer(opts.maxHeaderSize, 'maxHeaderSize') || 16384;
    if (opts.timeout !== undefined) {
      integer(opts.timeout, 'timeout');
    }
    return {
      opts,
      hostname,
      port: number,
      method,
      path,
      maximum,
      callback,
      agent,
      connector,
      defaultPort
    };
  }

  function validateConnection(upgrade, connection) {
    const requested = upgrade !== undefined;
    if (requested) {
      const protocols = String(upgrade).split(',');
      if (
        !protocols.length ||
        protocols.some((value) => {
          const parts = value.trim().split('/');
          return parts.length > 2 || parts.some((part) => !token.test(part));
        })
      ) {
        throw error(TypeError, 'ERR_INVALID_HTTP_TOKEN', 'Invalid Upgrade protocol');
      }
    }
    if (connection === undefined || String(connection).toLowerCase() === 'close') {
      return;
    }
    const tokens = String(connection)
      .split(',')
      .map((value) => value.trim());
    if (tokens.some((value) => !token.test(value))) {
      throw error(TypeError, 'ERR_INVALID_HTTP_TOKEN', 'Invalid Connection token');
    }
    if (!requested || !tokens.some((value) => value.toLowerCase() === 'upgrade')) {
      unsupported('Persistent HTTP client connections');
    }
  }

  function handoff(req, res, result, raw) {
    const c = clients.get(req);
    const socket = c.socket;
    if (
      !result.upgrade ||
      !raw ||
      !Number.isInteger(result.bytesParsed) ||
      result.bytesParsed < 0 ||
      result.bytesParsed > raw.byteLength
    ) {
      clientFailure(req, error(Error, 'HPE_INVALID_UPGRADE', 'Invalid HTTP upgrade boundary'));
      return;
    }
    c.upgraded = true;
    c.complete = true;
    res.complete = true;
    for (const [name, callback] of c.socketListeners) {
      apply(removeListener, socket, [name, callback]);
    }
    c.socketListeners.length = 0;
    if (descriptor(socket, 'setEncoding')?.value === c.noEncoding) {
      if (c.encodingDescriptor) {
        define(socket, 'setEncoding', c.encodingDescriptor);
      } else {
        delete socket.setEncoding;
      }
    }
    assignedSockets.delete(socket);
    c.unsubscribe?.();
    c.unsubscribe = null;
    const head = Buffer.from(
      raw.buffer,
      raw.byteOffset + result.bytesParsed,
      raw.byteLength - result.bytesParsed
    );
    // Agent credit ends here, but the upgraded native socket remains user-owned.
    socket.emit('agentRemove');
    req.destroy();
    if (!req.emit('upgrade', res, socket, head)) {
      socket.destroy();
    }
  }

  function clientFailure(req, err) {
    const c = clients.get(req);
    if (c.failed) {
      return;
    }
    c.failed = true;
    if (!req.destroyed) {
      req.destroy(err);
    }
  }

  function closeRequest(req) {
    const c = clients.get(req);
    if (c.closed) {
      return;
    }
    c.closed = true;
    c.unsubscribe?.();
    c.unsubscribe = null;
    if (!req.destroyed) {
      req.destroy();
    }

    const finish = () => {
      if (c.current && !c.current.complete && !c.current.destroyed) {
        c.current.aborted = true;
        c.current.emit('aborted');
        c.current.destroy(error(Error, 'ECONNRESET', 'aborted'));
      }
      req.emit('close');
    };

    if (c.errorPending) {
      tick(finish);
    } else {
      finish();
    }
  }

  function clientEvents(req, result, raw) {
    const c = clients.get(req);
    for (const event of result.events) {
      if (c.failed || c.socket.destroyed) {
        break;
      }
      if (event.type === 0) {
        const values = merged(event.headers, c.options.joinDuplicateHeaders);
        if (event.statusCode < 200 && !event.upgrade) {
          if (event.statusCode === 100) {
            req.emit('continue');
          }
          req.emit('information', {
            statusCode: event.statusCode,
            statusMessage: event.statusMessage,
            httpVersion: `${event.major}.${event.minor}`,
            httpVersionMajor: event.major,
            httpVersionMinor: event.minor,
            headers: values.headers,
            rawHeaders: event.headers
          });
          continue;
        }
        if (c.complete || c.current) {
          clientFailure(
            req,
            error(Error, 'HPE_UNEXPECTED_CONTENT_LENGTH', 'Unexpected extra HTTP response')
          );
          break;
        }
        const res = new IncomingMessage(c.socket);
        res.req = req;
        req.res = res;
        res.statusCode = event.statusCode;
        res.statusMessage = event.statusMessage;
        res.httpVersionMajor = event.major;
        res.httpVersionMinor = event.minor;
        res.httpVersion = `${event.major}.${event.minor}`;
        const count =
          req.maxHeadersCount == null
            ? 2000
            : req.maxHeadersCount > 0
              ? 2 * req.maxHeadersCount
              : Infinity;
        res.rawHeaders = event.headers.slice(0, count);
        const headers = merged(res.rawHeaders, c.options.joinDuplicateHeaders);
        res.headers = headers.headers;
        res.headersDistinct = headers.distinct;
        incoming.get(res).connection = c;
        c.current = res;
        if (event.upgrade) {
          handoff(req, res, result, raw);
          return;
        }
        if (!req.emit('response', res)) {
          res.resume();
        }
      } else if (event.type === 1) {
        if (
          c.current &&
          !c.current.push(
            Buffer.from(event.data.buffer, event.data.byteOffset, event.data.byteLength)
          )
        ) {
          c.pausedBy = c.current;
          c.socket.pause();
        }
      } else if (event.type === 2) {
        if (c.current) {
          const res = c.current;
          const values = merged(event.headers, c.options.joinDuplicateHeaders);
          res.rawTrailers = event.headers;
          res.trailers = values.headers;
          res.trailersDistinct = values.distinct;
        }
      } else if (event.type === 3 && c.current) {
        c.complete = true;
        c.current.complete = true;
        c.current.push(null);
        c.socket.destroySoon();
      }
    }
    if (result.error && !c.failed && !c.complete) {
      const e = error(Error, result.error.code, `Parse Error: ${result.error.reason}`);
      e.bytesParsed = result.error.bytesParsed;
      e.reason = result.error.reason;
      if (raw) {
        e.rawPacket = raw;
      }
      clientFailure(req, e);
    }
  }

  /** Own one admitted request connection and response lifecycle, without socket reuse. */
  class ClientRequest extends OutgoingMessage {
    constructor(input, options, callback, privateToken = undefined, privateTransport = undefined) {
      // Extra public constructor arguments cannot select an unvalidated transport.
      const transport = privateToken === clientToken ? privateTransport : plainTransport;
      const config = clientOptions(input, options, callback, transport);
      super({ emitClose: false, highWaterMark: config.opts.highWaterMark });
      this.method = config.method;
      this.path = config.path;
      this.host = config.hostname;
      this.protocol = transport.protocol;
      this.socket = null;
      this.res = null;
      this.sendDate = false;
      this.shouldKeepAlive = false;
      this.reusedSocket = false;
      this.aborted = false;
      this.maxHeadersCount = null;
      const opts = config.opts;
      for (const [name, value] of Array.isArray(opts.headers)
        ? pairs(opts.headers)
        : Object.entries(opts.headers ?? {})) {
        this.appendHeader(name, value);
      }
      if (!this.hasHeader('host') && opts.setHost !== false) {
        this.setHeader(
          'Host',
          (config.hostname.includes(':') ? '[' + config.hostname + ']' : config.hostname) +
            (config.port === Number(config.defaultPort) ? '' : ':' + config.port)
        );
      }
      if (opts.auth !== undefined && !this.hasHeader('authorization')) {
        if (typeof opts.auth !== 'string') {
          throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'auth must be a string');
        }
        this.setHeader('Authorization', 'Basic ' + Buffer.from(opts.auth).toString('base64'));
      }
      validateConnection(this.getHeader('upgrade'), this.getHeader('connection'));
      if (config.callback) {
        this.once('response', config.callback);
      }
      // Validate transport security before parser/Agent admission can acquire resources.
      const transportOptions = transport.agentOptions?.(config, this);
      const parser = native.createParser(config.maximum, true, this.method === 'HEAD');
      const c = {
        socket: null,
        transport,
        config,
        socketDone: false,
        socketListeners: [],
        upgraded: false,
        encodingDescriptor: null,
        noEncoding: null,
        published: false,
        ready: false,
        timeout: opts.timeout,
        noDelay: undefined,
        keepAlive: undefined,
        parser,
        options: opts,
        current: null,
        complete: false,
        failed: false,
        closed: false,
        pausedBy: null,
        unsubscribe: null,
        errorPending: false
      };
      clients.set(this, c);

      if (opts.signal) {
        const signal = opts.signal;

        const cancel = () => {
          const failure = error(Error, 'ABORT_ERR', 'The operation was aborted');
          failure.name = 'AbortError';
          failure.cause = abort.reason(signal);
          this.destroy(failure);
        };

        if (signal.aborted) {
          // Writable.destroy queues error delivery, so state is already destroyed
          // on return while callers can still attach error/close listeners.
          cancel();
        } else {
          c.unsubscribe = abort.subscribe(signal, cancel);
        }
      }

      if (!this.destroyed) {
        if (config.connector) {
          const connectionOptions = {
            ...opts,
            ...transportOptions,
            noDelay: opts.noDelay ?? true,
            host: config.hostname,
            hostname: config.hostname,
            port: config.port,
            protocol: transport.protocol,
            secureEndpoint: transport.protocol === 'https:'
          };
          if (!opts.path) {
            Object.setPrototypeOf(connectionOptions, null);
          }
          // The HTTP request target is not a Unix-domain socket pathname.
          delete connectionOptions.path;
          try {
            const socket = apply(config.connector, connectionOptions, [
              connectionOptions,
              (err, socket) => this.onSocket(socket, err)
            ]);
            if (socket) {
              this.onSocket(socket);
            }
          } catch (err) {
            tick(() => this.destroy(err));
          }
        } else if (config.agent === false) {
          this.onSocket(transport.createSocket(config, this), undefined, clientToken);
        } else {
          this.agent = config.agent;
          try {
            config.agent.addRequest(this, {
              ...opts,
              ...transportOptions,
              noDelay: opts.noDelay ?? true,
              host: config.hostname,
              hostname: config.hostname,
              port: config.port,
              protocol: transport.protocol,
              secureEndpoint: transport.protocol === 'https:'
            });
          } catch (err) {
            tick(() => this.destroy(err));
          }
        }
      }
    }

    onSocket(socket, err, directToken = undefined) {
      const c = clients.get(this);
      const genuine = socket instanceof host.net.Socket;
      if (c.socketDone) {
        if (genuine && c.socket !== socket && !assignedSockets.has(socket)) {
          socket.destroy();
        }
        return;
      }
      c.socketDone = true;
      if (err) {
        if (genuine && !assignedSockets.has(socket)) {
          socket.destroy();
        }

        tick(() => {
          if (!this.destroyed) {
            this.destroy(err);
          }
        });

        return;
      }
      if (!genuine) {
        tick(() =>
          this.destroy(
            error(TypeError, 'ERR_INVALID_ARG_TYPE', 'Agent returned an unsupported socket')
          )
        );

        return;
      }
      if (c.transport.isSocket && !c.transport.isSocket(socket)) {
        if (!assignedSockets.has(socket)) {
          socket.destroy();
        }

        tick(() =>
          this.destroy(
            error(Error, 'ERR_MITE_UNSUPPORTED_API', 'HTTPS requires a native TLS socket')
          )
        );

        return;
      }
      if (assignedSockets.has(socket)) {
        tick(() =>
          this.destroy(
            error(Error, 'ERR_HTTP_SOCKET_ASSIGNED', 'Socket already belongs to a request')
          )
        );

        return;
      }
      if (this.destroyed || socket.destroyed) {
        if (!socket.destroyed) {
          socket.destroy();
        }
        if (!this.destroyed) {
          tick(() => this.destroy(error(Error, 'ECONNRESET', 'Socket is closed')));
        }
        return;
      }
      c.socket = socket;
      assignedSockets.set(socket, this);
      const parser = c.parser;

      const listen = (name, callback, once = false) => {
        c.socketListeners.push([name, callback]);
        if (once) {
          socket.once(name, callback);
        } else {
          socket.on(name, callback);
        }
      };

      listen('data', (bytes) => {
        if (!c.failed) {
          clientEvents(this, parser(bytes), bytes);
        }
      });

      listen('end', () => {
        if (!c.failed && !c.complete) {
          const result = parser();
          // A premature EOF after response headers belongs to the
          // response stream, not a second request parser error.
          if (result.error && c.current) {
            c.failed = true;
            c.current.aborted = true;
            c.current.emit('aborted');
            c.current.destroy(error(Error, 'ECONNRESET', 'aborted'));
            socket.destroy();
          } else {
            clientEvents(this, result);
          }
        }
        if (!c.failed && !c.complete) {
          if (c.current) {
            c.failed = true;
            c.current.aborted = true;
            c.current.emit('aborted');
            c.current.destroy(error(Error, 'ECONNRESET', 'aborted'));
            socket.destroy();
          } else {
            clientFailure(this, error(Error, 'ECONNRESET', 'socket hang up'));
          }
        }
      });

      listen('error', (err) => {
        if (!this.destroyed) {
          clientFailure(this, err);
        }
      });

      listen('close', () => {
        assignedSockets.delete(socket);
        if (!c.complete && !c.failed && !this.destroyed) {
          clientFailure(this, error(Error, 'ECONNRESET', 'socket hang up'));
        }
        closeRequest(this);
      });

      listen('timeout', () => {
        this.emit('timeout');
        if (c.current) {
          c.current.emit('timeout');
        }
      });

      c.encodingDescriptor = descriptor(socket, 'setEncoding');

      c.noEncoding = () => {
        throw error(Error, 'ERR_HTTP_SOCKET_ENCODING', 'HTTP sockets require byte data');
      };

      socket.setEncoding = c.noEncoding;

      listen(
        c.transport.readyEvent,
        () => {
          if (!this.destroyed) {
            c.ready = true;
            if (c.published) {
              this._activate();
            }
          }
        },
        true
      );

      if (c.timeout !== undefined) {
        socket.setTimeout(c.timeout);
      }
      if (c.noDelay !== undefined) {
        socket.setNoDelay(c.noDelay);
      }
      if (c.keepAlive) {
        socket.setKeepAlive(...c.keepAlive);
      }

      const direct = directToken === clientToken;
      if (!direct) {
        c.ready = c.transport.protocol === 'https:' ? !socket.secureConnecting : !socket.connecting;
      }

      tick(() => {
        if (this.destroyed) {
          return;
        }
        this.socket = socket;
        c.published = true;
        this.emit('socket', socket);
        if (this.destroyed) {
          return;
        }
        if (direct) {
          const { config } = c;
          const opts = config.opts;
          try {
            socket.connect({
              port: config.port,
              host: config.hostname,
              family: opts.family,
              lookup: opts.lookup,
              hints: opts.hints,
              autoSelectFamily: opts.autoSelectFamily,
              autoSelectFamilyAttemptTimeout: opts.autoSelectFamilyAttemptTimeout
            });
          } catch (err) {
            this.destroy(err);
          }
        } else if (c.ready) {
          this._activate();
        }
      });
    }

    _prepare() {
      const s = outgoing.get(this);
      if (clients.get(this)?.upgraded) {
        throw error(Error, 'ERR_STREAM_DESTROYED', 'HTTP request has upgraded');
      }
      if (s.prepared) {
        return;
      }
      if (typeof this.method !== 'string' || !token.test(this.method)) {
        throw error(TypeError, 'ERR_INVALID_HTTP_TOKEN', 'Invalid HTTP method');
      }
      if (typeof this.path !== 'string' || /[^\x21-\xff]/.test(this.path)) {
        throw error(TypeError, 'ERR_UNESCAPED_CHARACTERS', 'Invalid request path');
      }

      const headers = new Map(s.headers);

      const has = (name) => headers.has(name);

      validateConnection(headers.get('upgrade')?.value, headers.get('connection')?.value);
      if (has('content-length') && has('transfer-encoding')) {
        throw error(
          Error,
          'ERR_HTTP_INVALID_HEADER_VALUE',
          'Content-Length and Transfer-Encoding cannot both be sent'
        );
      }
      if (
        has('transfer-encoding') &&
        String(headers.get('transfer-encoding').value).toLowerCase() !== 'chunked'
      ) {
        unsupported('Request transfer encoding other than chunked');
      }
      if (!has('content-length') && !has('transfer-encoding')) {
        if (s.endLength !== null) {
          if (s.endLength || !['GET', 'HEAD'].includes(this.method)) {
            headers.set('content-length', { name: 'Content-Length', value: s.endLength });
          }
        } else {
          headers.set('transfer-encoding', { name: 'Transfer-Encoding', value: 'chunked' });
        }
      }
      s.chunked = headers.has('transfer-encoding');
      if (!has('connection')) {
        headers.set('connection', { name: 'Connection', value: 'close' });
      }
      s.block = `${this.method} ${this.path} HTTP/1.1\r\n${headerLines(headers)}\r\n`;
      s.prepared = true;
    }

    _destroy(err, callback) {
      const c = clients.get(this);
      if (c) {
        c.failed = true;
        c.unsubscribe?.();
        c.unsubscribe = null;
        c.errorPending = !!err;
      }
      // Queue the request error before an unconnected socket queues its close.
      // No application JS runs until this frame has also initiated native close.
      callback(err);
      if (c?.errorPending) {
        // A transport can emit error and close in one tick. Its close must wait
        // behind the request error that Writable.destroy just enqueued.
        tick(() => {
          c.errorPending = false;
        });
      }
      if (c?.socket && !c.socket.destroyed && !c.upgraded) {
        c.socket.destroy();
      } else if (c && (!c.socket || c.upgraded)) {
        tick(() => closeRequest(this));
      }
    }

    abort() {
      if (this.aborted) {
        return;
      }
      this.aborted = true;

      tick(() => this.emit('abort'));

      this.destroy();
    }

    setTimeout(value, callback) {
      if (callback) {
        this.once('timeout', callback);
      }
      integer(value, 'timeout');
      const c = clients.get(this);
      c.timeout = value;
      c.socket?.setTimeout(value);
      return this;
    }

    setNoDelay(value = true) {
      const c = clients.get(this);
      c.noDelay = !!value;
      c.socket?.setNoDelay(value);
      return this;
    }

    setSocketKeepAlive(enable = false, initialDelay = 0) {
      integer(initialDelay, 'initialDelay');
      const c = clients.get(this);
      c.keepAlive = [!!enable, initialDelay];
      c.socket?.setKeepAlive(enable, initialDelay);
      return this;
    }

    writeContinue() {
      unsupported('ClientRequest.writeContinue');
    }

    writeProcessing() {
      unsupported('ClientRequest.writeProcessing');
    }
  }

  function request(input, options, callback) {
    return new ClientRequest(input, options, callback);
  }

  function get(input, options, callback) {
    const req = request(input, options, callback);
    req.end();
    return req;
  }

  function requestWithTransport(transport, input, options, callback) {
    return new ClientRequest(input, options, callback, clientToken, transport);
  }

  return { ClientRequest, request, get, requestWithTransport };
}
