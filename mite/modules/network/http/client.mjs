/**
 * One-connection HTTP client: validate options before creating a transport, own
 * parser/backpressure/cancellation state, and release sockets on completion.
 * HTTPS selects its validated transport through the private embedding hook only.
 */
export default function createHttpClient(host, protocol, messages) {
  const Buffer = host.Buffer;
  const URL = host.URL;
  const tick = host.process.nextTick;
  const native = host.native;
  const abort = host.abort;
  const { token, error, unsupported, merged, integer, headerLines, pairs } = protocol;
  const { IncomingMessage, OutgoingMessage, incoming, outgoing } = messages;
  const clients = new WeakMap();
  const clientToken = {};
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
    if (opts.agent !== undefined && opts.agent !== false) {
      unsupported('HTTP agents and connection pooling');
    }
    for (const key of [
      'createConnection',
      'socketPath',
      'localAddress',
      'localPort',
      'uniqueHeaders',
      'IncomingMessage',
      'defaultPort'
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
    const port = opts.port ?? transport.defaultPort;
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
    return { opts, hostname, port: number, method, path, maximum, callback };
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

  function clientEvents(req, result, raw) {
    const c = clients.get(req);
    for (const event of result.events) {
      if (c.failed || c.socket.destroyed) {
        break;
      }
      if (event.type === 0) {
        if (event.upgrade) {
          clientFailure(
            req,
            error(Error, 'ERR_MITE_UNSUPPORTED_API', 'HTTP upgrades are not implemented')
          );
          break;
        }
        const values = merged(event.headers, c.options.joinDuplicateHeaders);
        if (event.statusCode < 200) {
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

  /** Own one request connection and response lifecycle; unsupported Agent/transport modes fail explicitly. */
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
            (config.port === transport.defaultPort ? '' : ':' + config.port)
        );
      }
      if (opts.auth !== undefined && !this.hasHeader('authorization')) {
        if (typeof opts.auth !== 'string') {
          throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'auth must be a string');
        }
        this.setHeader('Authorization', 'Basic ' + Buffer.from(opts.auth).toString('base64'));
      }
      if (this.hasHeader('upgrade')) {
        unsupported('HTTP upgrades');
      }
      if (
        this.hasHeader('connection') &&
        String(this.getHeader('connection')).toLowerCase() !== 'close'
      ) {
        unsupported('Persistent HTTP client connections');
      }
      if (config.callback) {
        this.once('response', config.callback);
      }
      const parser = native.createParser(config.maximum, true, this.method === 'HEAD');
      const socket = transport.createSocket(config, this);
      const c = {
        socket,
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

      socket.on('data', (bytes) => {
        if (!c.failed) {
          clientEvents(this, parser(bytes), bytes);
        }
      });

      socket.on('end', () => {
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

      socket.on('error', (err) => {
        if (!this.destroyed) {
          clientFailure(this, err);
        }
      });

      socket.on('close', () => {
        // Request errors precede response abortion/close. Native teardown owns
        // the incomplete response; queued response errors follow request close.
        if (!c.complete && !c.failed && !this.destroyed) {
          clientFailure(this, error(Error, 'ECONNRESET', 'socket hang up'));
        }
        if (!c.closed) {
          c.closed = true;
          if (!this.destroyed) {
            this.destroy();
          }

          const finish = () => {
            if (c.current && !c.current.complete && !c.current.destroyed) {
              c.current.aborted = true;
              c.current.emit('aborted');
              c.current.destroy(error(Error, 'ECONNRESET', 'aborted'));
            }
            this.emit('close');
          };

          if (c.errorPending) {
            tick(finish);
          } else {
            finish();
          }
        }
      });

      socket.on('timeout', () => {
        this.emit('timeout');
        if (c.current) {
          c.current.emit('timeout');
        }
      });

      socket.setEncoding = () => {
        throw error(Error, 'ERR_HTTP_SOCKET_ENCODING', 'HTTP sockets require byte data');
      };

      socket.once(transport.readyEvent, () => {
        if (!this.destroyed) {
          this._activate();
        }
      });

      if (opts.timeout) {
        socket.setTimeout(opts.timeout);
      }

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

      tick(() => {
        if (this.destroyed) {
          return;
        }
        this.socket = socket;
        this.emit('socket', socket);
        if (this.destroyed) {
          return;
        }
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
      });
    }

    _prepare() {
      const s = outgoing.get(this);
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

      if (has('upgrade')) {
        unsupported('HTTP upgrades');
      }
      if (has('connection') && String(headers.get('connection').value).toLowerCase() !== 'close') {
        unsupported('Persistent HTTP client connections');
      }
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
      if (c && !c.socket.destroyed) {
        c.socket.destroy();
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
      clients.get(this).socket.setTimeout(value);
      return this;
    }

    setNoDelay(value = true) {
      clients.get(this).socket.setNoDelay(value);
      return this;
    }

    setSocketKeepAlive(enable = false, initialDelay = 0) {
      clients.get(this).socket.setKeepAlive(enable, initialDelay);
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
