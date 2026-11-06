/* Generated from mite/modules/network/http.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/network/http.mjs
function createHttpModule(host) {
  const { Readable, Writable } = host.stream;
  const { Server: TCPServer } = host.net;
  const Buffer = host.Buffer;
  const URL = host.URL;
  const tick = host.process.nextTick;
  const native = host.native;
  const abort = host.abort;
  const incoming = /* @__PURE__ */ new WeakMap();
  const outgoing = /* @__PURE__ */ new WeakMap();
  const servers = /* @__PURE__ */ new WeakMap();
  const clients = /* @__PURE__ */ new WeakMap();
  const clientToken = {};
  const plainTransport = Object.freeze({
    protocol: "http:",
    defaultPort: 80,
    readyEvent: "connect",
    createSocket(config) {
      return new host.net.Socket({
        noDelay: config.opts.noDelay ?? true,
        keepAlive: config.opts.keepAlive,
        keepAliveInitialDelay: config.opts.keepAliveInitialDelay
      });
    }
  });
  const token = /^[!#$%&'*+.^_`|~0-9A-Za-z-]+$/;
  const singleton = /* @__PURE__ */ new Set([
    "age",
    "authorization",
    "content-length",
    "content-type",
    "etag",
    "expires",
    "from",
    "host",
    "if-modified-since",
    "if-unmodified-since",
    "last-modified",
    "location",
    "max-forwards",
    "proxy-authorization",
    "referer",
    "retry-after",
    "server",
    "user-agent"
  ]);
  const STATUS_CODES = {
    100: "Continue",
    101: "Switching Protocols",
    102: "Processing",
    103: "Early Hints",
    200: "OK",
    201: "Created",
    202: "Accepted",
    203: "Non-Authoritative Information",
    204: "No Content",
    205: "Reset Content",
    206: "Partial Content",
    207: "Multi-Status",
    208: "Already Reported",
    226: "IM Used",
    300: "Multiple Choices",
    301: "Moved Permanently",
    302: "Found",
    303: "See Other",
    304: "Not Modified",
    305: "Use Proxy",
    307: "Temporary Redirect",
    308: "Permanent Redirect",
    400: "Bad Request",
    401: "Unauthorized",
    402: "Payment Required",
    403: "Forbidden",
    404: "Not Found",
    405: "Method Not Allowed",
    406: "Not Acceptable",
    407: "Proxy Authentication Required",
    408: "Request Timeout",
    409: "Conflict",
    410: "Gone",
    411: "Length Required",
    412: "Precondition Failed",
    413: "Payload Too Large",
    414: "URI Too Long",
    415: "Unsupported Media Type",
    416: "Range Not Satisfiable",
    417: "Expectation Failed",
    418: "I'm a Teapot",
    421: "Misdirected Request",
    422: "Unprocessable Entity",
    423: "Locked",
    424: "Failed Dependency",
    425: "Too Early",
    426: "Upgrade Required",
    428: "Precondition Required",
    429: "Too Many Requests",
    431: "Request Header Fields Too Large",
    451: "Unavailable For Legal Reasons",
    500: "Internal Server Error",
    501: "Not Implemented",
    502: "Bad Gateway",
    503: "Service Unavailable",
    504: "Gateway Timeout",
    505: "HTTP Version Not Supported",
    506: "Variant Also Negotiates",
    507: "Insufficient Storage",
    508: "Loop Detected",
    509: "Bandwidth Limit Exceeded",
    510: "Not Extended",
    511: "Network Authentication Required"
  };
  const METHODS = [...native.METHODS].sort();
  function error(Type, code, message) {
    const e = new Type(message);
    e.code = code;
    return e;
  }
  function unsupported(name) {
    throw error(Error, "ERR_MITE_UNSUPPORTED_API", `${name} is not implemented`);
  }
  function validateHeaderName(name) {
    if (typeof name !== "string" || !token.test(name)) {
      throw error(
        TypeError,
        "ERR_INVALID_HTTP_TOKEN",
        `Header name must be a valid HTTP token [${String(name)}]`
      );
    }
  }
  function validateHeaderValue(name, value) {
    if (value === void 0) {
      throw error(TypeError, "ERR_HTTP_INVALID_HEADER_VALUE", `Invalid value for header ${name}`);
    }
    const values = Array.isArray(value) ? value : [value];
    for (const item of values) {
      if (/[^\t\x20-\x7e\x80-\xff]/.test(String(item))) {
        throw error(TypeError, "ERR_INVALID_CHAR", `Invalid character in header content [${name}]`);
      }
    }
  }
  function merged(raw, join) {
    const headers = {};
    const distinct = {};
    for (let i = 0; i < raw.length; i += 2) {
      const name = raw[i].toLowerCase();
      const value = raw[i + 1].replace(/^[ \t]+|[ \t]+$/g, "");
      if (!Object.hasOwn(distinct, name)) {
        Object.defineProperty(distinct, name, {
          value: [],
          enumerable: true,
          writable: true,
          configurable: true
        });
      }
      distinct[name].push(value);
      if (!Object.hasOwn(headers, name)) {
        Object.defineProperty(headers, name, {
          value: name === "set-cookie" ? [value] : value,
          enumerable: true,
          writable: true,
          configurable: true
        });
      } else if (name === "set-cookie") {
        headers[name].push(value);
      } else if (name === "cookie") {
        headers[name] += "; " + value;
      } else if (join || !singleton.has(name)) {
        headers[name] += ", " + value;
      }
    }
    return { headers, distinct };
  }
  class IncomingMessage extends Readable {
    constructor(socket) {
      super({ autoDestroy: true });
      this.socket = socket;
      this.httpVersionMajor = null;
      this.httpVersionMinor = null;
      this.httpVersion = null;
      this.complete = false;
      this.aborted = false;
      this.url = "";
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
        this.once("timeout", callback);
      }
      this.socket.setTimeout(value);
      return this;
    }
  }
  function changeHeaders(response) {
    if (response.headersSent) {
      throw error(Error, "ERR_HTTP_HEADERS_SENT", "Cannot change headers after they are sent");
    }
    return outgoing.get(response);
  }
  function headerLines(headers) {
    let text = "";
    for (const { name, value } of headers.values()) {
      const values = Array.isArray(value) ? name.toLowerCase() === "cookie" ? [value.join("; ")] : value : [value];
      for (const item of values) {
        const field = String(item);
        validateHeaderValue(name, field);
        text += `${name}: ${field}\r
`;
      }
    }
    return text;
  }
  class OutgoingMessage extends Writable {
    constructor(options = {}) {
      super({ autoDestroy: false, decodeStrings: true, ...options });
      outgoing.set(this, {
        headers: /* @__PURE__ */ new Map(),
        prepared: false,
        written: false,
        block: "",
        active: false,
        waiting: [],
        chunked: false,
        noBody: false,
        bytes: 0,
        endLength: null,
        trailers: /* @__PURE__ */ new Map(),
        connection: null
      });
      this.sendDate = true;
      this.statusCode = 200;
      this.statusMessage = void 0;
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
        old === void 0 ? value : [...Array.isArray(old) ? old : [old], ...Array.isArray(value) ? value : [value]]
      );
    }
    getHeader(name) {
      if (typeof name !== "string") {
        throw error(TypeError, "ERR_INVALID_ARG_TYPE", "name must be a string");
      }
      return outgoing.get(this).headers.get(name.toLowerCase())?.value;
    }
    hasHeader(name) {
      return this.getHeader(name) !== void 0;
    }
    getHeaderNames() {
      return [...outgoing.get(this).headers.keys()];
    }
    getRawHeaderNames() {
      return [...outgoing.get(this).headers.values()].map((h) => h.name);
    }
    getHeaders() {
      const result = /* @__PURE__ */ Object.create(null);
      for (const [key, h] of outgoing.get(this).headers) {
        result[key] = h.value;
      }
      return result;
    }
    removeHeader(name) {
      if (typeof name !== "string") {
        throw error(TypeError, "ERR_INVALID_ARG_TYPE", "name must be a string");
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
      const get2 = (name) => headers.get(name)?.value;
      const code = Number(this.statusCode);
      if (!Number.isInteger(code) || code < 100 || code > 999) {
        throw error(
          RangeError,
          "ERR_HTTP_INVALID_STATUS_CODE",
          `Invalid status code: ${this.statusCode}`
        );
      }
      const message = this.statusMessage === void 0 ? STATUS_CODES[code] ?? "unknown" : String(this.statusMessage);
      if (/[^\t\x20-\x7e\x80-\xff]/.test(message)) {
        throw error(TypeError, "ERR_INVALID_CHAR", "Invalid character in statusMessage");
      }
      this.statusCode = code;
      this.statusMessage = message;
      const forbidden = code < 200 || code === 204 || code === 304;
      s.noBody = forbidden || this.req?.method === "HEAD";
      if (this.sendDate && !has("date")) {
        headers.set("date", { name: "Date", value: (/* @__PURE__ */ new Date()).toUTCString() });
      }
      if (forbidden) {
        headers.delete("transfer-encoding");
        if (code === 204) {
          headers.delete("content-length");
        }
      }
      let framingName = null;
      if (!s.noBody && !has("content-length") && !has("transfer-encoding")) {
        if (this.req?.httpVersionMajor === 1 && this.req?.httpVersionMinor === 0) {
          this.shouldKeepAlive = false;
        } else if (s.endLength !== null) {
          headers.set("content-length", { name: "Content-Length", value: s.endLength });
          framingName = "content-length";
        } else {
          headers.set("transfer-encoding", { name: "Transfer-Encoding", value: "chunked" });
          framingName = "transfer-encoding";
        }
      }
      if (has("content-length") && has("transfer-encoding")) {
        throw error(
          Error,
          "ERR_HTTP_INVALID_HEADER_VALUE",
          "Content-Length and Transfer-Encoding cannot both be sent"
        );
      }
      if (has("transfer-encoding") && String(get2("transfer-encoding")).toLowerCase() !== "chunked") {
        unsupported("Response transfer encoding other than chunked");
      }
      s.chunked = !s.noBody && has("transfer-encoding");
      if (String(get2("connection") ?? "").toLowerCase().split(/\s*,\s*/).includes("close")) {
        this.shouldKeepAlive = false;
      }
      if (s.connection?.closing && s.connection.queue.at(-1) === this) {
        this.shouldKeepAlive = false;
      }
      if (!has("connection")) {
        headers.set("connection", {
          name: "Connection",
          value: this.shouldKeepAlive ? "keep-alive" : "close"
        });
      }
      if (this.shouldKeepAlive && !has("keep-alive")) {
        const seconds = Math.floor((s.connection?.server.keepAliveTimeout ?? 5e3) / 1e3);
        headers.set("keep-alive", { name: "Keep-Alive", value: `timeout=${seconds}` });
      }
      if (framingName) {
        const h = headers.get(framingName);
        headers.delete(framingName);
        headers.set(framingName, h);
      }
      s.block = `HTTP/1.1 ${code} ${message}\r
${headerLines(headers)}\r
`;
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
          callback(error(Error, "ERR_STREAM_DESTROYED", "Socket is destroyed"));
          return;
        }
        if (!s.written) {
          s.written = true;
          this.socket.write(s.block, "latin1");
        }
        this.socket.write(data, callback);
      });
    }
    _write(chunk, encoding, callback) {
      const s = outgoing.get(this);
      this._prepare();
      s.bytes += chunk.length;
      if (this.strictContentLength && !s.noBody && this.hasHeader("content-length") && s.bytes > Number(this.getHeader("content-length"))) {
        callback(
          error(Error, "ERR_HTTP_CONTENT_LENGTH_MISMATCH", "Response exceeds Content-Length")
        );
        return;
      }
      if (s.noBody || chunk.length === 0) {
        this._output(Buffer.alloc(0), callback);
        return;
      }
      const data = s.chunked ? Buffer.concat([
        Buffer.from(chunk.length.toString(16) + "\r\n"),
        chunk,
        Buffer.from("\r\n")
      ]) : chunk;
      this._output(data, callback);
    }
    _final(callback) {
      const s = outgoing.get(this);
      if (!s.prepared && s.endLength === null) {
        s.endLength = 0;
      }
      this._prepare();
      if (this.strictContentLength && !s.noBody && this.hasHeader("content-length") && s.bytes !== Number(this.getHeader("content-length"))) {
        callback(
          error(Error, "ERR_HTTP_CONTENT_LENGTH_MISMATCH", "Response does not match Content-Length")
        );
        return;
      }
      this._output(
        Buffer.from(s.chunked ? "0\r\n" + headerLines(s.trailers) + "\r\n" : ""),
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
        if (typeof chunk === "string") {
          s.endLength = Buffer.byteLength(chunk, typeof encoding === "string" ? encoding : "utf8");
        } else if (chunk instanceof Uint8Array) {
          s.endLength = chunk.byteLength;
        } else if (chunk == null || typeof chunk === "function") {
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
          this.socket.write(s.block, "latin1");
        }
      });
    }
    addTrailers(headers) {
      const map = /* @__PURE__ */ new Map();
      for (const [name, value] of Array.isArray(headers) ? pairs(headers) : Object.entries(headers)) {
        validateHeaderName(name);
        validateHeaderValue(name, value);
        map.set(name.toLowerCase(), { name, value });
      }
      outgoing.get(this).trailers = map;
    }
    setTimeout(value, callback) {
      if (callback) {
        this.once("timeout", callback);
      }
      this.socket.setTimeout(value);
      return this;
    }
    writeContinue(callback) {
      this._run(() => this.socket.write("HTTP/1.1 100 Continue\r\n\r\n", callback));
    }
    writeProcessing(callback) {
      this._run(() => this.socket.write("HTTP/1.1 102 Processing\r\n\r\n", callback));
    }
    writeEarlyHints() {
      unsupported("writeEarlyHints");
    }
  }
  function pairs(raw) {
    if (raw.length % 2) {
      throw error(TypeError, "ERR_INVALID_ARG_VALUE", "Header array must have pairs");
    }
    const result = [];
    for (let i = 0; i < raw.length; i += 2) {
      result.push([raw[i], raw[i + 1]]);
    }
    return result;
  }
  class ServerResponse extends OutgoingMessage {
    constructor(req) {
      super();
      this.req = req;
      this.socket = req.socket;
      this.shouldKeepAlive = true;
    }
    writeHead(code, message, headers) {
      changeHeaders(this);
      if (typeof message !== "string") {
        headers = message;
        message = void 0;
      }
      this.statusCode = code;
      if (message !== void 0) {
        this.statusMessage = message;
      }
      if (headers) {
        for (const [name, value] of Array.isArray(headers) ? pairs(headers) : Object.entries(headers)) {
          this.setHeader(name, value);
        }
      }
      this._prepare();
      return this;
    }
  }
  function clientOptions(input, options, callback, transport) {
    if (typeof options === "function") {
      callback = options;
      options = void 0;
    }
    let base = {};
    if (typeof input === "string" || input instanceof URL) {
      const url = input instanceof URL ? input : new URL(input);
      base = {
        protocol: url.protocol,
        hostname: url.hostname,
        port: url.port || void 0,
        path: url.pathname + url.search
      };
      if (url.username || url.password) {
        base.auth = decodeURIComponent(url.username) + ":" + decodeURIComponent(url.password);
      }
    } else if (input !== void 0 && input !== null) {
      base = { ...input };
    }
    const opts = { ...base, ...options };
    if (callback !== void 0 && typeof callback !== "function") {
      throw error(TypeError, "ERR_INVALID_ARG_TYPE", "callback must be a function");
    }
    if (opts.protocol !== void 0 && opts.protocol !== transport.protocol) {
      throw error(TypeError, "ERR_INVALID_PROTOCOL", `Expected ${transport.protocol} protocol`);
    }
    if (opts.agent !== void 0 && opts.agent !== false) {
      unsupported("HTTP agents and connection pooling");
    }
    for (const key of [
      "createConnection",
      "socketPath",
      "localAddress",
      "localPort",
      "uniqueHeaders",
      "IncomingMessage",
      "defaultPort"
    ]) {
      if (opts[key] !== void 0) {
        unsupported(`HTTP client option ${key}`);
      }
    }
    if (opts.insecureHTTPParser) {
      unsupported("insecureHTTPParser");
    }
    if (opts.setDefaultHeaders === false) {
      unsupported("setDefaultHeaders:false");
    }
    if (opts.signal && !abort.isSignal(opts.signal)) {
      throw error(TypeError, "ERR_INVALID_ARG_TYPE", "signal must be an AbortSignal");
    }
    let hostname = opts.hostname ?? opts.host ?? "localhost";
    if (typeof hostname !== "string") {
      throw error(TypeError, "ERR_INVALID_ARG_TYPE", "hostname must be a string");
    }
    if (hostname.startsWith("[") && hostname.endsWith("]")) {
      hostname = hostname.slice(1, -1);
    }
    const port = opts.port ?? transport.defaultPort;
    const number = Number(port);
    if (!["number", "string"].includes(typeof port) || typeof port === "string" && !port.trim() || !Number.isInteger(number) || number < 0 || number > 65535) {
      throw error(RangeError, "ERR_SOCKET_BAD_PORT", "Invalid HTTP port");
    }
    let method = opts.method ?? "GET";
    if (typeof method !== "string") {
      throw error(TypeError, "ERR_INVALID_ARG_TYPE", "method must be a string");
    }
    if (!token.test(method)) {
      throw error(TypeError, "ERR_INVALID_HTTP_TOKEN", "Invalid HTTP method");
    }
    method = method.toUpperCase();
    if (method === "CONNECT") {
      unsupported("HTTP CONNECT");
    }
    const path = opts.path ?? "/";
    if (typeof path !== "string") {
      throw error(TypeError, "ERR_INVALID_ARG_TYPE", "path must be a string");
    }
    if (/[^\x21-\xff]/.test(path)) {
      throw error(
        TypeError,
        "ERR_UNESCAPED_CHARACTERS",
        "Request path contains unescaped characters"
      );
    }
    const maximum = opts.maxHeaderSize === void 0 ? 16384 : integer(opts.maxHeaderSize, "maxHeaderSize") || 16384;
    if (opts.timeout !== void 0) {
      integer(opts.timeout, "timeout");
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
            error(Error, "ERR_MITE_UNSUPPORTED_API", "HTTP upgrades are not implemented")
          );
          break;
        }
        const values = merged(event.headers, c.options.joinDuplicateHeaders);
        if (event.statusCode < 200) {
          if (event.statusCode === 100) {
            req.emit("continue");
          }
          req.emit("information", {
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
            error(Error, "HPE_UNEXPECTED_CONTENT_LENGTH", "Unexpected extra HTTP response")
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
        const count = req.maxHeadersCount == null ? 2e3 : req.maxHeadersCount > 0 ? 2 * req.maxHeadersCount : Infinity;
        res.rawHeaders = event.headers.slice(0, count);
        const headers = merged(res.rawHeaders, c.options.joinDuplicateHeaders);
        res.headers = headers.headers;
        res.headersDistinct = headers.distinct;
        incoming.get(res).connection = c;
        c.current = res;
        if (!req.emit("response", res)) {
          res.resume();
        }
      } else if (event.type === 1) {
        if (c.current && !c.current.push(
          Buffer.from(event.data.buffer, event.data.byteOffset, event.data.byteLength)
        )) {
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
  class ClientRequest extends OutgoingMessage {
    constructor(input, options, callback, privateToken = void 0, privateTransport = void 0) {
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
      for (const [name, value] of Array.isArray(opts.headers) ? pairs(opts.headers) : Object.entries(opts.headers ?? {})) {
        this.appendHeader(name, value);
      }
      if (!this.hasHeader("host") && opts.setHost !== false) {
        this.setHeader(
          "Host",
          (config.hostname.includes(":") ? "[" + config.hostname + "]" : config.hostname) + (config.port === transport.defaultPort ? "" : ":" + config.port)
        );
      }
      if (opts.auth !== void 0 && !this.hasHeader("authorization")) {
        if (typeof opts.auth !== "string") {
          throw error(TypeError, "ERR_INVALID_ARG_TYPE", "auth must be a string");
        }
        this.setHeader("Authorization", "Basic " + Buffer.from(opts.auth).toString("base64"));
      }
      if (this.hasHeader("upgrade")) {
        unsupported("HTTP upgrades");
      }
      if (this.hasHeader("connection") && String(this.getHeader("connection")).toLowerCase() !== "close") {
        unsupported("Persistent HTTP client connections");
      }
      if (config.callback) {
        this.once("response", config.callback);
      }
      const parser = native.createParser(config.maximum, true, this.method === "HEAD");
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
      socket.on("data", (bytes) => {
        if (!c.failed) {
          clientEvents(this, parser(bytes), bytes);
        }
      });
      socket.on("end", () => {
        if (!c.failed && !c.complete) {
          const result = parser();
          if (result.error && c.current) {
            c.failed = true;
            c.current.aborted = true;
            c.current.emit("aborted");
            c.current.destroy(error(Error, "ECONNRESET", "aborted"));
            socket.destroy();
          } else {
            clientEvents(this, result);
          }
        }
        if (!c.failed && !c.complete) {
          if (c.current) {
            c.failed = true;
            c.current.aborted = true;
            c.current.emit("aborted");
            c.current.destroy(error(Error, "ECONNRESET", "aborted"));
            socket.destroy();
          } else {
            clientFailure(this, error(Error, "ECONNRESET", "socket hang up"));
          }
        }
      });
      socket.on("error", (err) => {
        if (!this.destroyed) {
          clientFailure(this, err);
        }
      });
      socket.on("close", () => {
        if (!c.complete && !c.failed && !this.destroyed) {
          clientFailure(this, error(Error, "ECONNRESET", "socket hang up"));
        }
        if (!c.closed) {
          c.closed = true;
          if (!this.destroyed) {
            this.destroy();
          }
          const finish = () => {
            if (c.current && !c.current.complete && !c.current.destroyed) {
              c.current.aborted = true;
              c.current.emit("aborted");
              c.current.destroy(error(Error, "ECONNRESET", "aborted"));
            }
            this.emit("close");
          };
          if (c.errorPending) {
            tick(finish);
          } else {
            finish();
          }
        }
      });
      socket.on("timeout", () => {
        this.emit("timeout");
        if (c.current) {
          c.current.emit("timeout");
        }
      });
      socket.setEncoding = () => {
        throw error(Error, "ERR_HTTP_SOCKET_ENCODING", "HTTP sockets require byte data");
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
          const failure = error(Error, "ABORT_ERR", "The operation was aborted");
          failure.name = "AbortError";
          failure.cause = abort.reason(signal);
          this.destroy(failure);
        };
        if (signal.aborted) {
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
        this.emit("socket", socket);
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
      if (typeof this.method !== "string" || !token.test(this.method)) {
        throw error(TypeError, "ERR_INVALID_HTTP_TOKEN", "Invalid HTTP method");
      }
      if (typeof this.path !== "string" || /[^\x21-\xff]/.test(this.path)) {
        throw error(TypeError, "ERR_UNESCAPED_CHARACTERS", "Invalid request path");
      }
      const headers = new Map(s.headers);
      const has = (name) => headers.has(name);
      if (has("upgrade")) {
        unsupported("HTTP upgrades");
      }
      if (has("connection") && String(headers.get("connection").value).toLowerCase() !== "close") {
        unsupported("Persistent HTTP client connections");
      }
      if (has("content-length") && has("transfer-encoding")) {
        throw error(
          Error,
          "ERR_HTTP_INVALID_HEADER_VALUE",
          "Content-Length and Transfer-Encoding cannot both be sent"
        );
      }
      if (has("transfer-encoding") && String(headers.get("transfer-encoding").value).toLowerCase() !== "chunked") {
        unsupported("Request transfer encoding other than chunked");
      }
      if (!has("content-length") && !has("transfer-encoding")) {
        if (s.endLength !== null) {
          if (s.endLength || !["GET", "HEAD"].includes(this.method)) {
            headers.set("content-length", { name: "Content-Length", value: s.endLength });
          }
        } else {
          headers.set("transfer-encoding", { name: "Transfer-Encoding", value: "chunked" });
        }
      }
      s.chunked = headers.has("transfer-encoding");
      if (!has("connection")) {
        headers.set("connection", { name: "Connection", value: "close" });
      }
      s.block = `${this.method} ${this.path} HTTP/1.1\r
${headerLines(headers)}\r
`;
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
      callback(err);
      if (c?.errorPending) {
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
      tick(() => this.emit("abort"));
      this.destroy();
    }
    setTimeout(value, callback) {
      if (callback) {
        this.once("timeout", callback);
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
      unsupported("ClientRequest.writeContinue");
    }
    writeProcessing() {
      unsupported("ClientRequest.writeProcessing");
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
    if (c.server.emit("clientError", err, c.socket)) {
      return;
    }
    const code = err.code === "HPE_HEADER_OVERFLOW" ? 431 : err.code === "ERR_HTTP_REQUEST_TIMEOUT" ? 408 : err.code === "ERR_MITE_UNSUPPORTED_API" ? 501 : 400;
    if (c.queue.some((res) => outgoing.get(res).written)) {
      c.socket.destroy();
      return;
    }
    c.socket.end(
      `HTTP/1.1 ${code} ${STATUS_CODES[code]}\r
Connection: close\r
\r
`,
      () => c.socket.destroy()
    );
  }
  function aborted(c) {
    for (const req of c.requests) {
      if (!req.complete && !req.destroyed) {
        req.aborted = true;
        req.emit("aborted");
        req.destroy(error(Error, "ECONNRESET", "aborted"));
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
    tick(() => res.emit("close"));
    if (!res.shouldKeepAlive) {
      c.socket.destroySoon();
      return;
    }
    if (c.queue.length) {
      c.queue[0]._activate();
    } else if (c.closing) {
      c.socket.destroySoon();
    } else if (!c.current && c.phase === "idle") {
      arm(c, c.server.keepAliveTimeout, () => c.socket.destroy());
    }
  }
  function events(c, result, raw) {
    for (const event of result.events) {
      if (c.failed || c.socket.destroyed) {
        break;
      }
      if (event.type === 4) {
        c.phase = "headers";
        c.startedInput = true;
        c.started = Date.now();
        const deadline = c.server.requestTimeout && c.server.headersTimeout ? Math.min(c.server.requestTimeout, c.server.headersTimeout) : c.server.headersTimeout || c.server.requestTimeout;
        arm(
          c,
          deadline,
          () => fail(c, error(Error, "ERR_HTTP_REQUEST_TIMEOUT", "Headers timeout"))
        );
      } else if (event.type === 0) {
        c.phase = "body";
        clearTimer(c);
        if (event.upgrade || event.method === "CONNECT") {
          fail(
            c,
            error(
              Error,
              "ERR_MITE_UNSUPPORTED_API",
              "HTTP protocol upgrades and CONNECT are not implemented"
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
        const pairsLimit = c.server.maxHeadersCount == null ? 2e3 : c.server.maxHeadersCount > 0 ? 2 * c.server.maxHeadersCount : Infinity;
        req.rawHeaders = event.headers.slice(0, pairsLimit);
        const values = merged(req.rawHeaders, c.options.joinDuplicateHeaders);
        req.headers = values.headers;
        req.headersDistinct = values.distinct;
        c.current = req;
        c.requests.add(req);
        req.on("error", () => {
        });
        req.once("end", () => {
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
        res.once("finish", () => responseDone(c, res));
        res.on("error", (err) => c.socket.destroy(err));
        arm(
          c,
          c.server.requestTimeout ? Math.max(1, c.server.requestTimeout - (Date.now() - c.started)) : 0,
          () => fail(c, error(Error, "ERR_HTTP_REQUEST_TIMEOUT", "Request timeout"))
        );
        if (req.httpVersion === "1.1" && req.headers.host === void 0 && c.options.requireHostHeader !== false) {
          res.shouldKeepAlive = false;
          res.writeHead(400);
          res.end();
        } else if (String(req.headers.expect ?? "").toLowerCase() === "100-continue") {
          if (c.server.listenerCount("checkContinue")) {
            c.server.emit("checkContinue", req, res);
          } else {
            res.writeContinue();
            c.server.emit("request", req, res);
          }
        } else if (req.headers.expect !== void 0) {
          if (c.server.listenerCount("checkExpectation")) {
            c.server.emit("checkExpectation", req, res);
          } else {
            res.writeHead(417);
            res.end();
          }
        } else {
          c.server.emit("request", req, res);
        }
      } else if (event.type === 1) {
        if (c.current && !c.current.push(
          Buffer.from(event.data.buffer, event.data.byteOffset, event.data.byteLength)
        )) {
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
          c.phase = "idle";
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
  function integer(value, name) {
    if (typeof value !== "number") {
      throw error(TypeError, "ERR_INVALID_ARG_TYPE", `${name} must be a number`);
    }
    if (!Number.isInteger(value) || value < 0 || value > 2147483647) {
      throw error(RangeError, "ERR_OUT_OF_RANGE", `Invalid ${name}`);
    }
    return value;
  }
  class Server extends TCPServer {
    constructor(options = {}, listener) {
      if (typeof options === "function") {
        listener = options;
        options = {};
      }
      if (options == null) {
        options = {};
      }
      if (typeof options !== "object") {
        throw error(TypeError, "ERR_INVALID_ARG_TYPE", "options must be an object");
      }
      if (options.insecureHTTPParser) {
        unsupported("insecureHTTPParser");
      }
      for (const key of ["uniqueHeaders", "connectionsCheckingInterval"]) {
        if (options[key] !== void 0) {
          unsupported(key);
        }
      }
      if (options.rejectNonStandardBodyWrites) {
        unsupported("rejectNonStandardBodyWrites");
      }
      if (options.maxRequestsPerSocket) {
        unsupported("maxRequestsPerSocket");
      }
      super({ ...options, allowHalfOpen: true });
      const config = {
        ...options,
        IncomingMessage: options.IncomingMessage ?? IncomingMessage,
        ServerResponse: options.ServerResponse ?? ServerResponse
      };
      const maximum = options.maxHeaderSize === void 0 ? 16384 : integer(options.maxHeaderSize, "maxHeaderSize") || 16384;
      servers.set(this, { connections: /* @__PURE__ */ new Set(), options: config, maximum });
      this.maxHeaderSize = maximum;
      this.maxHeadersCount = null;
      this.requestTimeout = options.requestTimeout === void 0 ? 3e5 : integer(options.requestTimeout, "requestTimeout");
      this.headersTimeout = options.headersTimeout === void 0 ? Math.min(6e4, this.requestTimeout || 6e4) : integer(options.headersTimeout, "headersTimeout");
      if (this.headersTimeout && this.requestTimeout && this.headersTimeout > this.requestTimeout) {
        throw error(
          RangeError,
          "ERR_OUT_OF_RANGE",
          "headersTimeout must not exceed requestTimeout"
        );
      }
      this.keepAliveTimeout = options.keepAliveTimeout === void 0 ? 5e3 : integer(options.keepAliveTimeout, "keepAliveTimeout");
      this.timeout = 0;
      this.httpAllowHalfOpen = false;
      if (listener) {
        this.on("request", listener);
      }
      this.on("connection", (socket) => {
        const state = servers.get(this);
        const c = {
          server: this,
          socket,
          options: config,
          parser: native.createParser(maximum),
          queue: [],
          requests: /* @__PURE__ */ new Set(),
          current: null,
          phase: "headers",
          startedInput: false,
          pausedBy: null,
          started: Date.now(),
          timer: null,
          failed: false,
          closing: false
        };
        state.connections.add(c);
        socket.setEncoding = () => {
          throw error(Error, "ERR_HTTP_SOCKET_ENCODING", "HTTP sockets require byte data");
        };
        arm(
          c,
          this.headersTimeout,
          () => fail(c, error(Error, "ERR_HTTP_REQUEST_TIMEOUT", "Headers timeout"))
        );
        socket.on("data", (bytes) => {
          if (!c.failed) {
            events(c, c.parser(bytes), bytes);
          }
        });
        socket.on("end", () => {
          if (!c.failed) {
            events(c, c.parser());
          }
          if (!this.httpAllowHalfOpen || !c.queue.length) {
            socket.end();
          } else {
            c.closing = true;
          }
        });
        socket.on("error", (err) => {
          if (err.code !== "ECONNRESET") {
            this.emit("clientError", err, socket);
          }
        });
        socket.on("close", () => {
          clearTimer(c);
          state.connections.delete(c);
          aborted(c);
        });
        socket.on("timeout", () => {
          let handled = false;
          if (c.current) {
            handled = c.current.emit("timeout") || handled;
          }
          if (c.queue[0]) {
            handled = c.queue[0].emit("timeout") || handled;
          }
          handled = this.emit("timeout", socket) || handled;
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
      this.timeout = integer(value, "timeout");
      if (callback) {
        this.on("timeout", callback);
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
        unsupported("maxRequestsPerSocket");
      }
    }
  }
  function createServer(...args) {
    return new Server(...args);
  }
  Object.defineProperty(host.clientHooks, "request", {
    value: (transport, input, options, callback) => new ClientRequest(input, options, callback, clientToken, transport)
  });
  return {
    Server,
    ServerResponse,
    OutgoingMessage,
    IncomingMessage,
    ClientRequest,
    request,
    get,
    createServer,
    METHODS,
    STATUS_CODES,
    validateHeaderName,
    validateHeaderValue,
    maxHeaderSize: 16384
  };
}
return createHttpModule;
})()
