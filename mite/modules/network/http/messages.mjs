/**
 * Shared IncomingMessage/OutgoingMessage classes and the server response subtype.
 * The private WeakMaps coordinate parser connections, framing and stream pressure
 * across client/server factories; only the public constructors leave the facade.
 */
export default function createHttpMessages(host, protocol) {
  const { Readable, Writable } = host.stream;
  const Buffer = host.Buffer;
  const {
    error,
    unsupported,
    validateHeaderName,
    validateHeaderValue,
    STATUS_CODES,
    headerLines,
    pairs
  } = protocol;
  const incoming = new WeakMap();
  const outgoing = new WeakMap();

  /** Readable HTTP metadata/body view; parser and socket callbacks own the incoming byte flow. */
  class IncomingMessage extends Readable {
    constructor(socket) {
      super({ autoDestroy: true });
      this.socket = socket;
      this.httpVersionMajor = null;
      this.httpVersionMinor = null;
      this.httpVersion = null;
      this.complete = false;
      this.aborted = false;
      this.url = '';
      this.method = null;
      this.rawHeaders = [];
      this.headers = {};
      this.headersDistinct = {};
      this.rawTrailers = [];
      this.trailers = {};
      this.trailersDistinct = {};
      incoming.set(this, { connection: null });
    }

    get connection() {
      return this.socket;
    }

    _read() {
      const c = incoming.get(this).connection;
      if (c && !c.failed && !this.socket.destroyed && (!c.pausedBy || c.pausedBy === this)) {
        c.pausedBy = null;
        this.socket.resume();
      }
    }

    _destroy(err, callback) {
      if (!this.complete && !this.socket.destroyed) {
        this.socket.destroy(err);
      }
      callback(err);
    }

    setTimeout(value, callback) {
      if (callback) {
        this.once('timeout', callback);
      }
      this.socket.setTimeout(value);
      return this;
    }
  }

  function changeHeaders(response) {
    if (response.headersSent) {
      throw error(Error, 'ERR_HTTP_HEADERS_SENT', 'Cannot change headers after they are sent');
    }
    return outgoing.get(response);
  }
  /** Coordinate validated headers, framing and writable pressure before bytes reach TCP. */
  class OutgoingMessage extends Writable {
    constructor(options = {}) {
      super({ autoDestroy: false, decodeStrings: true, ...options });
      outgoing.set(this, {
        headers: new Map(),
        prepared: false,
        written: false,
        block: '',
        active: false,
        waiting: [],
        chunked: false,
        noBody: false,
        bytes: 0,
        endLength: null,
        trailers: new Map(),
        connection: null
      });
      this.sendDate = true;
      this.statusCode = 200;
      this.statusMessage = undefined;
      this.shouldKeepAlive = true;
      this.strictContentLength = false;
    }

    get headersSent() {
      return outgoing.get(this).prepared;
    }

    get finished() {
      return this.writableEnded;
    }

    get connection() {
      return this.socket;
    }

    setHeader(name, value) {
      const s = changeHeaders(this);
      validateHeaderName(name);
      validateHeaderValue(name, value);
      s.headers.set(name.toLowerCase(), { name, value });
      return this;
    }

    appendHeader(name, value) {
      validateHeaderName(name);
      validateHeaderValue(name, value);
      const old = this.getHeader(name);
      return this.setHeader(
        outgoing.get(this).headers.get(name.toLowerCase())?.name ?? name,
        old === undefined
          ? value
          : [...(Array.isArray(old) ? old : [old]), ...(Array.isArray(value) ? value : [value])]
      );
    }

    getHeader(name) {
      if (typeof name !== 'string') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'name must be a string');
      }
      return outgoing.get(this).headers.get(name.toLowerCase())?.value;
    }

    hasHeader(name) {
      return this.getHeader(name) !== undefined;
    }

    getHeaderNames() {
      return [...outgoing.get(this).headers.keys()];
    }

    getRawHeaderNames() {
      return [...outgoing.get(this).headers.values()].map((h) => h.name);
    }

    getHeaders() {
      const result = Object.create(null);
      for (const [key, h] of outgoing.get(this).headers) {
        result[key] = h.value;
      }
      return result;
    }

    removeHeader(name) {
      if (typeof name !== 'string') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'name must be a string');
      }
      changeHeaders(this).headers.delete(name.toLowerCase());
    }

    _prepare() {
      const s = outgoing.get(this);
      if (s.prepared) {
        return;
      }

      const headers = new Map(s.headers);

      const has = (name) => headers.has(name);

      const get = (name) => headers.get(name)?.value;

      const code = Number(this.statusCode);
      if (!Number.isInteger(code) || code < 100 || code > 999) {
        throw error(
          RangeError,
          'ERR_HTTP_INVALID_STATUS_CODE',
          `Invalid status code: ${this.statusCode}`
        );
      }
      const message =
        this.statusMessage === undefined
          ? (STATUS_CODES[code] ?? 'unknown')
          : String(this.statusMessage);
      if (/[^\t\x20-\x7e\x80-\xff]/.test(message)) {
        throw error(TypeError, 'ERR_INVALID_CHAR', 'Invalid character in statusMessage');
      }
      this.statusCode = code;
      this.statusMessage = message;
      const forbidden = code < 200 || code === 204 || code === 304;
      s.noBody = forbidden || this.req?.method === 'HEAD';
      if (this.sendDate && !has('date')) {
        headers.set('date', { name: 'Date', value: new Date().toUTCString() });
      }
      if (forbidden) {
        headers.delete('transfer-encoding');
        if (code === 204) {
          headers.delete('content-length');
        }
      }
      let framingName = null;
      if (!s.noBody && !has('content-length') && !has('transfer-encoding')) {
        if (this.req?.httpVersionMajor === 1 && this.req?.httpVersionMinor === 0) {
          this.shouldKeepAlive = false;
        } else if (s.endLength !== null) {
          headers.set('content-length', { name: 'Content-Length', value: s.endLength });
          framingName = 'content-length';
        } else {
          headers.set('transfer-encoding', { name: 'Transfer-Encoding', value: 'chunked' });
          framingName = 'transfer-encoding';
        }
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
        String(get('transfer-encoding')).toLowerCase() !== 'chunked'
      ) {
        unsupported('Response transfer encoding other than chunked');
      }
      s.chunked = !s.noBody && has('transfer-encoding');
      if (
        String(get('connection') ?? '')
          .toLowerCase()
          .split(/\s*,\s*/)
          .includes('close')
      ) {
        this.shouldKeepAlive = false;
      }
      if (s.connection?.closing && s.connection.queue.at(-1) === this) {
        this.shouldKeepAlive = false;
      }
      if (!has('connection')) {
        headers.set('connection', {
          name: 'Connection',
          value: this.shouldKeepAlive ? 'keep-alive' : 'close'
        });
      }
      if (this.shouldKeepAlive && !has('keep-alive')) {
        const seconds = Math.floor((s.connection?.server.keepAliveTimeout ?? 5000) / 1000);
        headers.set('keep-alive', { name: 'Keep-Alive', value: `timeout=${seconds}` });
      }
      if (framingName) {
        const h = headers.get(framingName);
        headers.delete(framingName);
        headers.set(framingName, h);
      }
      s.block = `HTTP/1.1 ${code} ${message}\r\n${headerLines(headers)}\r\n`;
      s.prepared = true;
    }

    _run(fn) {
      const s = outgoing.get(this);
      if (s.active) {
        fn();
      } else {
        s.waiting.push(fn);
      }
    }

    _activate() {
      const s = outgoing.get(this);
      s.active = true;
      while (s.waiting.length && !this.destroyed) {
        s.waiting.shift()();
      }
    }

    _output(data, callback) {
      this._prepare();

      this._run(() => {
        const s = outgoing.get(this);
        if (this.socket.destroyed) {
          callback(error(Error, 'ERR_STREAM_DESTROYED', 'Socket is destroyed'));
          return;
        }
        if (!s.written) {
          s.written = true;
          this.socket.write(s.block, 'latin1');
        }
        this.socket.write(data, callback);
      });
    }

    _write(chunk, encoding, callback) {
      const s = outgoing.get(this);
      this._prepare();
      s.bytes += chunk.length;
      if (
        this.strictContentLength &&
        !s.noBody &&
        this.hasHeader('content-length') &&
        s.bytes > Number(this.getHeader('content-length'))
      ) {
        callback(
          error(Error, 'ERR_HTTP_CONTENT_LENGTH_MISMATCH', 'Response exceeds Content-Length')
        );
        return;
      }
      if (s.noBody || chunk.length === 0) {
        this._output(Buffer.alloc(0), callback);
        return;
      }
      const data = s.chunked
        ? Buffer.concat([
            Buffer.from(chunk.length.toString(16) + '\r\n'),
            chunk,
            Buffer.from('\r\n')
          ])
        : chunk;
      this._output(data, callback);
    }

    _final(callback) {
      const s = outgoing.get(this);
      if (!s.prepared && s.endLength === null) {
        s.endLength = 0;
      }
      this._prepare();
      if (
        this.strictContentLength &&
        !s.noBody &&
        this.hasHeader('content-length') &&
        s.bytes !== Number(this.getHeader('content-length'))
      ) {
        callback(
          error(Error, 'ERR_HTTP_CONTENT_LENGTH_MISMATCH', 'Response does not match Content-Length')
        );
        return;
      }
      this._output(
        Buffer.from(s.chunked ? '0\r\n' + headerLines(s.trailers) + '\r\n' : ''),
        callback
      );
    }

    _destroy(err, callback) {
      if (this.socket && !this.socket.destroyed) {
        this.socket.destroy(err);
      }
      callback(err);
    }

    end(chunk, encoding, callback) {
      const s = outgoing.get(this);
      if (!s.prepared && !this.writableLength && s.bytes === 0) {
        if (typeof chunk === 'string') {
          s.endLength = Buffer.byteLength(chunk, typeof encoding === 'string' ? encoding : 'utf8');
        } else if (chunk instanceof Uint8Array) {
          s.endLength = chunk.byteLength;
        } else if (chunk == null || typeof chunk === 'function') {
          s.endLength = 0;
        }
      }
      return super.end(chunk, encoding, callback);
    }

    flushHeaders() {
      this._prepare();

      this._run(() => {
        const s = outgoing.get(this);
        if (!s.written) {
          s.written = true;
          this.socket.write(s.block, 'latin1');
        }
      });
    }

    addTrailers(headers) {
      const map = new Map();
      for (const [name, value] of Array.isArray(headers)
        ? pairs(headers)
        : Object.entries(headers)) {
        validateHeaderName(name);
        validateHeaderValue(name, value);
        map.set(name.toLowerCase(), { name, value });
      }
      outgoing.get(this).trailers = map;
    }

    setTimeout(value, callback) {
      if (callback) {
        this.once('timeout', callback);
      }
      this.socket.setTimeout(value);
      return this;
    }

    writeContinue(callback) {
      this._run(() => this.socket.write('HTTP/1.1 100 Continue\r\n\r\n', callback));
    }

    writeProcessing(callback) {
      this._run(() => this.socket.write('HTTP/1.1 102 Processing\r\n\r\n', callback));
    }

    writeEarlyHints() {
      unsupported('writeEarlyHints');
    }
  }
  /** Bind outgoing framing and completion to one incoming request. */
  class ServerResponse extends OutgoingMessage {
    constructor(req) {
      super();
      this.req = req;
      this.socket = req.socket;
      this.shouldKeepAlive = true;
    }

    writeHead(code, message, headers) {
      changeHeaders(this);
      if (typeof message !== 'string') {
        headers = message;
        message = undefined;
      }
      this.statusCode = code;
      if (message !== undefined) {
        this.statusMessage = message;
      }
      if (headers) {
        for (const [name, value] of Array.isArray(headers)
          ? pairs(headers)
          : Object.entries(headers)) {
          this.setHeader(name, value);
        }
      }
      this._prepare();
      return this;
    }
  }

  return { IncomingMessage, OutgoingMessage, ServerResponse, incoming, outgoing };
}
