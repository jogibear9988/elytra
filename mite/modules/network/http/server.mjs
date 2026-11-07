/**
 * HTTP server connection ownership: ordered responses, bounded parser input,
 * request/header/keep-alive deadlines, close/drain and explicit unsupported modes.
 * Reuse the shared message classes so HTTP/HTTPS constructors keep one identity.
 */
export default function createHttpServer(host, protocol, messages) {
  const { Server: TCPServer } = host.net;
  const Buffer = host.Buffer;
  const tick = host.process.nextTick;
  const native = host.native;
  const { STATUS_CODES, error, unsupported, merged, integer } = protocol;
  const { IncomingMessage, ServerResponse, incoming, outgoing } = messages;
  const servers = new WeakMap();

  function clearTimer(c) {
    if (c.timer) {
      clearTimeout(c.timer);
    }
    c.timer = null;
  }

  function arm(c, value, fn) {
    clearTimer(c);
    if (value > 0) {
      c.timer = setTimeout(() => {
        c.timer = null;
        fn();
      }, value).unref();
    }
  }

  function fail(c, err, raw) {
    if (c.failed || c.socket.destroyed) {
      return;
    }
    c.failed = true;
    clearTimer(c);
    if (raw) {
      err.rawPacket = raw;
    }
    if (c.server.emit('clientError', err, c.socket)) {
      return;
    }
    const code =
      err.code === 'HPE_HEADER_OVERFLOW'
        ? 431
        : err.code === 'ERR_HTTP_REQUEST_TIMEOUT'
          ? 408
          : err.code === 'ERR_MITE_UNSUPPORTED_API'
            ? 501
            : 400;
    // Never append a second HTTP response after bytes have already been sent.
    if (c.queue.some((res) => outgoing.get(res).written)) {
      c.socket.destroy();
      return;
    }

    c.socket.end(`HTTP/1.1 ${code} ${STATUS_CODES[code]}\r\nConnection: close\r\n\r\n`, () =>
      c.socket.destroy()
    );
  }

  function aborted(c) {
    for (const req of c.requests) {
      if (!req.complete && !req.destroyed) {
        req.aborted = true;
        req.emit('aborted');
        req.destroy(error(Error, 'ECONNRESET', 'aborted'));
      }
    }
    for (const res of c.queue) {
      if (!res.destroyed) {
        res.destroy();
      }
    }
    c.requests.clear();
  }

  function responseDone(c, res) {
    if (c.queue[0] !== res) {
      return;
    }
    c.queue.shift();
    if (res.req.complete) {
      c.requests.delete(res.req);
    }
    res.req.resume();

    tick(() => res.emit('close'));

    if (!res.shouldKeepAlive) {
      c.socket.destroySoon();
      return;
    }
    if (c.queue.length) {
      c.queue[0]._activate();
    } else if (c.closing) {
      c.socket.destroySoon();
    } else if (!c.current && c.phase === 'idle') {
      arm(c, c.server.keepAliveTimeout, () => c.socket.destroy());
    }
  }

  function events(c, result, raw) {
    for (const event of result.events) {
      if (c.failed || c.socket.destroyed) {
        break;
      }
      if (event.type === 4) {
        c.phase = 'headers';
        c.startedInput = true;
        c.started = Date.now();
        const deadline =
          c.server.requestTimeout && c.server.headersTimeout
            ? Math.min(c.server.requestTimeout, c.server.headersTimeout)
            : c.server.headersTimeout || c.server.requestTimeout;

        arm(c, deadline, () =>
          fail(c, error(Error, 'ERR_HTTP_REQUEST_TIMEOUT', 'Headers timeout'))
        );
      } else if (event.type === 0) {
        c.phase = 'body';
        clearTimer(c);
        if (event.upgrade || event.method === 'CONNECT') {
          fail(
            c,
            error(
              Error,
              'ERR_MITE_UNSUPPORTED_API',
              'HTTP protocol upgrades and CONNECT are not implemented'
            ),
            raw
          );
          break;
        }
        const req = new c.options.IncomingMessage(c.socket);
        incoming.get(req).connection = c;
        req.method = event.method;
        req.url = event.url;
        req.httpVersionMajor = event.major;
        req.httpVersionMinor = event.minor;
        req.httpVersion = `${event.major}.${event.minor}`;
        const pairsLimit =
          c.server.maxHeadersCount == null
            ? 2000
            : c.server.maxHeadersCount > 0
              ? 2 * c.server.maxHeadersCount
              : Infinity;
        req.rawHeaders = event.headers.slice(0, pairsLimit);
        const values = merged(req.rawHeaders, c.options.joinDuplicateHeaders);
        req.headers = values.headers;
        req.headersDistinct = values.distinct;
        c.current = req;
        c.requests.add(req);

        req.on('error', () => {});

        req.once('end', () => {
          if (c.pausedBy === req) {
            c.pausedBy = null;
            if (!c.socket.destroyed) {
              c.socket.resume();
            }
          }
        });

        const res = new c.options.ServerResponse(req);
        const s = outgoing.get(res);
        s.connection = c;
        incoming.get(req).response = res;
        res.shouldKeepAlive = event.keepAlive;
        c.queue.push(res);
        if (c.queue.length === 1) {
          res._activate();
        }

        res.once('finish', () => responseDone(c, res));

        res.on('error', (err) => c.socket.destroy(err));

        arm(
          c,
          c.server.requestTimeout
            ? Math.max(1, c.server.requestTimeout - (Date.now() - c.started))
            : 0,
          () => fail(c, error(Error, 'ERR_HTTP_REQUEST_TIMEOUT', 'Request timeout'))
        );

        if (
          req.httpVersion === '1.1' &&
          req.headers.host === undefined &&
          c.options.requireHostHeader !== false
        ) {
          res.shouldKeepAlive = false;
          res.writeHead(400);
          res.end();
        } else if (String(req.headers.expect ?? '').toLowerCase() === '100-continue') {
          if (c.server.listenerCount('checkContinue')) {
            c.server.emit('checkContinue', req, res);
          } else {
            res.writeContinue();
            c.server.emit('request', req, res);
          }
        } else if (req.headers.expect !== undefined) {
          if (c.server.listenerCount('checkExpectation')) {
            c.server.emit('checkExpectation', req, res);
          } else {
            res.writeHead(417);
            res.end();
          }
        } else {
          c.server.emit('request', req, res);
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
          const req = c.current;
          const values = merged(event.headers, c.options.joinDuplicateHeaders);
          req.rawTrailers = event.headers;
          req.trailers = values.headers;
          req.trailersDistinct = values.distinct;
        }
      } else if (event.type === 3) {
        if (c.current) {
          const req = c.current;
          req.complete = true;
          req.push(null);
          if (incoming.get(req).response.writableFinished) {
            c.requests.delete(req);
          }
          c.current = null;
          c.phase = 'idle';
          c.startedInput = false;
          clearTimer(c);
        }
      }
    }
    if (result.error && !c.failed) {
      const e = error(Error, result.error.code, `Parse Error: ${result.error.reason}`);
      e.bytesParsed = result.error.bytesParsed;
      e.reason = result.error.reason;
      fail(c, e, raw);
    }
  }

  class Server extends TCPServer {
    constructor(options = {}, listener) {
      if (typeof options === 'function') {
        listener = options;
        options = {};
      }
      if (options == null) {
        options = {};
      }
      if (typeof options !== 'object') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'options must be an object');
      }
      if (options.insecureHTTPParser) {
        unsupported('insecureHTTPParser');
      }
      for (const key of ['uniqueHeaders', 'connectionsCheckingInterval']) {
        if (options[key] !== undefined) {
          unsupported(key);
        }
      }
      if (options.rejectNonStandardBodyWrites) {
        unsupported('rejectNonStandardBodyWrites');
      }
      if (options.maxRequestsPerSocket) {
        unsupported('maxRequestsPerSocket');
      }
      super({ ...options, allowHalfOpen: true });
      const config = {
        ...options,
        IncomingMessage: options.IncomingMessage ?? IncomingMessage,
        ServerResponse: options.ServerResponse ?? ServerResponse
      };
      const maximum =
        options.maxHeaderSize === undefined
          ? 16384
          : integer(options.maxHeaderSize, 'maxHeaderSize') || 16384;
      servers.set(this, { connections: new Set(), options: config, maximum });
      this.maxHeaderSize = maximum;
      this.maxHeadersCount = null;
      this.requestTimeout =
        options.requestTimeout === undefined
          ? 300000
          : integer(options.requestTimeout, 'requestTimeout');
      this.headersTimeout =
        options.headersTimeout === undefined
          ? Math.min(60000, this.requestTimeout || 60000)
          : integer(options.headersTimeout, 'headersTimeout');
      if (this.headersTimeout && this.requestTimeout && this.headersTimeout > this.requestTimeout) {
        throw error(
          RangeError,
          'ERR_OUT_OF_RANGE',
          'headersTimeout must not exceed requestTimeout'
        );
      }
      this.keepAliveTimeout =
        options.keepAliveTimeout === undefined
          ? 5000
          : integer(options.keepAliveTimeout, 'keepAliveTimeout');
      this.timeout = 0;
      this.httpAllowHalfOpen = false;
      if (listener) {
        this.on('request', listener);
      }

      this.on('connection', (socket) => {
        const state = servers.get(this);
        const c = {
          server: this,
          socket,
          options: config,
          parser: native.createParser(maximum),
          queue: [],
          requests: new Set(),
          current: null,
          phase: 'headers',
          startedInput: false,
          pausedBy: null,
          started: Date.now(),
          timer: null,
          failed: false,
          closing: false
        };
        state.connections.add(c);

        socket.setEncoding = () => {
          throw error(Error, 'ERR_HTTP_SOCKET_ENCODING', 'HTTP sockets require byte data');
        };

        arm(c, this.headersTimeout, () =>
          fail(c, error(Error, 'ERR_HTTP_REQUEST_TIMEOUT', 'Headers timeout'))
        );

        socket.on('data', (bytes) => {
          if (!c.failed) {
            events(c, c.parser(bytes), bytes);
          }
        });

        socket.on('end', () => {
          if (!c.failed) {
            events(c, c.parser());
          }
          if (!this.httpAllowHalfOpen || !c.queue.length) {
            socket.end();
          } else {
            c.closing = true;
          }
        });

        socket.on('error', (err) => {
          if (err.code !== 'ECONNRESET') {
            this.emit('clientError', err, socket);
          }
        });

        socket.on('close', () => {
          clearTimer(c);
          state.connections.delete(c);
          aborted(c);
        });

        socket.on('timeout', () => {
          let handled = false;
          if (c.current) {
            handled = c.current.emit('timeout') || handled;
          }
          if (c.queue[0]) {
            handled = c.queue[0].emit('timeout') || handled;
          }
          handled = this.emit('timeout', socket) || handled;
          if (!handled) {
            socket.destroy();
          }
        });

        if (this.timeout) {
          socket.setTimeout(this.timeout);
        }
      });
    }

    close(callback) {
      const state = servers.get(this);
      for (const c of state.connections) {
        c.closing = true;
        if (!c.queue.length && !c.current && !c.startedInput) {
          c.socket.destroy();
        }
      }
      return super.close(callback);
    }

    closeIdleConnections() {
      for (const c of servers.get(this).connections) {
        if (!c.queue.length && !c.current && !c.startedInput) {
          c.socket.destroy();
        }
      }
    }

    closeAllConnections() {
      for (const c of servers.get(this).connections) {
        c.socket.destroy();
      }
    }

    setTimeout(value, callback) {
      this.timeout = integer(value, 'timeout');
      if (callback) {
        this.on('timeout', callback);
      }
      for (const c of servers.get(this).connections) {
        c.socket.setTimeout(value);
      }
      return this;
    }

    get maxRequestsPerSocket() {
      return 0;
    }

    set maxRequestsPerSocket(value) {
      if (value !== 0) {
        unsupported('maxRequestsPerSocket');
      }
    }
  }

  function createServer(...args) {
    return new Server(...args);
  }

  return { Server, createServer };
}
