/* Generated from mite/modules/network/http.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/network/http/protocol.mjs
function createHttpProtocol(native) {
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
  function integer(value, name) {
    if (typeof value !== "number") {
      throw error(TypeError, "ERR_INVALID_ARG_TYPE", `${name} must be a number`);
    }
    if (!Number.isInteger(value) || value < 0 || value > 2147483647) {
      throw error(RangeError, "ERR_OUT_OF_RANGE", `Invalid ${name}`);
    }
    return value;
  }
  return {
    METHODS,
    STATUS_CODES,
    token,
    error,
    unsupported,
    validateHeaderName,
    validateHeaderValue,
    merged,
    headerLines,
    pairs,
    integer
  };
}

// mite/modules/network/http/messages.mjs
function createHttpMessages(host, protocol) {
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
  const incoming = /* @__PURE__ */ new WeakMap();
  const outgoing = /* @__PURE__ */ new WeakMap();
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
      const get = (name) => headers.get(name)?.value;
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
      if (has("transfer-encoding") && String(get("transfer-encoding")).toLowerCase() !== "chunked") {
        unsupported("Response transfer encoding other than chunked");
      }
      s.chunked = !s.noBody && has("transfer-encoding");
      if (String(get("connection") ?? "").toLowerCase().split(/\s*,\s*/).includes("close")) {
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
  return { IncomingMessage, OutgoingMessage, ServerResponse, incoming, outgoing };
}

// mite/modules/network/http/client.mjs
function createHttpClient(host, protocol, messages, agents) {
  const Buffer = host.Buffer;
  const URL = host.URL;
  const tick = host.process.nextTick;
  const native = host.native;
  const abort = host.abort;
  const { token, error, unsupported, merged, integer, headerLines, pairs } = protocol;
  const { IncomingMessage, OutgoingMessage, incoming, outgoing } = messages;
  const clients = /* @__PURE__ */ new WeakMap();
  const clientToken = {};
  const assignedSockets = /* @__PURE__ */ new WeakMap();
  const descriptor = Object.getOwnPropertyDescriptor;
  const define = Object.defineProperty;
  const apply = Reflect.apply;
  const removeListener = host.net.Socket.prototype.removeListener;
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
    const connector = opts.agent == null && typeof opts.createConnection === "function" ? opts.createConnection : void 0;
    const agent = connector ? false : opts.agent == null ? transport.globalAgent ?? agents.globalAgent : opts.agent;
    if (agent !== false && (typeof agent !== "object" || typeof agent.addRequest !== "function")) {
      throw error(TypeError, "ERR_INVALID_ARG_TYPE", "agent must supply addRequest or be false");
    }
    if (agent !== false) {
      const expected = agent.protocol;
      if (expected !== void 0 && expected !== transport.protocol) {
        throw error(TypeError, "ERR_INVALID_PROTOCOL", `Expected ${expected} protocol`);
      }
    }
    for (const key of [
      "socketPath",
      "localAddress",
      "localPort",
      "uniqueHeaders",
      "IncomingMessage"
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
    const defaultPort = opts.defaultPort || (agent !== false ? agent.defaultPort : void 0) || transport.defaultPort;
    const port = opts.port ?? defaultPort;
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
    const requested = upgrade !== void 0;
    if (requested) {
      const protocols = String(upgrade).split(",");
      if (!protocols.length || protocols.some((value) => {
        const parts = value.trim().split("/");
        return parts.length > 2 || parts.some((part) => !token.test(part));
      })) {
        throw error(TypeError, "ERR_INVALID_HTTP_TOKEN", "Invalid Upgrade protocol");
      }
    }
    if (connection === void 0 || String(connection).toLowerCase() === "close") {
      return;
    }
    const tokens = String(connection).split(",").map((value) => value.trim());
    if (tokens.some((value) => !token.test(value))) {
      throw error(TypeError, "ERR_INVALID_HTTP_TOKEN", "Invalid Connection token");
    }
    if (!requested || !tokens.some((value) => value.toLowerCase() === "upgrade")) {
      unsupported("Persistent HTTP client connections");
    }
  }
  function handoff(req, res, result, raw) {
    const c = clients.get(req);
    const socket = c.socket;
    if (!result.upgrade || !raw || !Number.isInteger(result.bytesParsed) || result.bytesParsed < 0 || result.bytesParsed > raw.byteLength) {
      clientFailure(req, error(Error, "HPE_INVALID_UPGRADE", "Invalid HTTP upgrade boundary"));
      return;
    }
    c.upgraded = true;
    c.complete = true;
    res.complete = true;
    for (const [name, callback] of c.socketListeners) {
      apply(removeListener, socket, [name, callback]);
    }
    c.socketListeners.length = 0;
    if (descriptor(socket, "setEncoding")?.value === c.noEncoding) {
      if (c.encodingDescriptor) {
        define(socket, "setEncoding", c.encodingDescriptor);
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
    socket.emit("agentRemove");
    req.destroy();
    if (!req.emit("upgrade", res, socket, head)) {
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
        c.current.emit("aborted");
        c.current.destroy(error(Error, "ECONNRESET", "aborted"));
      }
      req.emit("close");
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
        if (event.upgrade) {
          handoff(req, res, result, raw);
          return;
        }
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
          (config.hostname.includes(":") ? "[" + config.hostname + "]" : config.hostname) + (config.port === Number(config.defaultPort) ? "" : ":" + config.port)
        );
      }
      if (opts.auth !== void 0 && !this.hasHeader("authorization")) {
        if (typeof opts.auth !== "string") {
          throw error(TypeError, "ERR_INVALID_ARG_TYPE", "auth must be a string");
        }
        this.setHeader("Authorization", "Basic " + Buffer.from(opts.auth).toString("base64"));
      }
      validateConnection(this.getHeader("upgrade"), this.getHeader("connection"));
      if (config.callback) {
        this.once("response", config.callback);
      }
      const transportOptions = transport.agentOptions?.(config, this);
      const parser = native.createParser(config.maximum, true, this.method === "HEAD");
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
        noDelay: void 0,
        keepAlive: void 0,
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
            secureEndpoint: transport.protocol === "https:"
          };
          if (!opts.path) {
            Object.setPrototypeOf(connectionOptions, null);
          }
          delete connectionOptions.path;
          try {
            const socket = apply(config.connector, connectionOptions, [
              connectionOptions,
              (err, socket2) => this.onSocket(socket2, err)
            ]);
            if (socket) {
              this.onSocket(socket);
            }
          } catch (err) {
            tick(() => this.destroy(err));
          }
        } else if (config.agent === false) {
          this.onSocket(transport.createSocket(config, this), void 0, clientToken);
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
              secureEndpoint: transport.protocol === "https:"
            });
          } catch (err) {
            tick(() => this.destroy(err));
          }
        }
      }
    }
    onSocket(socket, err, directToken = void 0) {
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
        tick(
          () => this.destroy(
            error(TypeError, "ERR_INVALID_ARG_TYPE", "Agent returned an unsupported socket")
          )
        );
        return;
      }
      if (c.transport.isSocket && !c.transport.isSocket(socket)) {
        if (!assignedSockets.has(socket)) {
          socket.destroy();
        }
        tick(
          () => this.destroy(
            error(Error, "ERR_MITE_UNSUPPORTED_API", "HTTPS requires a native TLS socket")
          )
        );
        return;
      }
      if (assignedSockets.has(socket)) {
        tick(
          () => this.destroy(
            error(Error, "ERR_HTTP_SOCKET_ASSIGNED", "Socket already belongs to a request")
          )
        );
        return;
      }
      if (this.destroyed || socket.destroyed) {
        if (!socket.destroyed) {
          socket.destroy();
        }
        if (!this.destroyed) {
          tick(() => this.destroy(error(Error, "ECONNRESET", "Socket is closed")));
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
      listen("data", (bytes) => {
        if (!c.failed) {
          clientEvents(this, parser(bytes), bytes);
        }
      });
      listen("end", () => {
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
      listen("error", (err2) => {
        if (!this.destroyed) {
          clientFailure(this, err2);
        }
      });
      listen("close", () => {
        assignedSockets.delete(socket);
        if (!c.complete && !c.failed && !this.destroyed) {
          clientFailure(this, error(Error, "ECONNRESET", "socket hang up"));
        }
        closeRequest(this);
      });
      listen("timeout", () => {
        this.emit("timeout");
        if (c.current) {
          c.current.emit("timeout");
        }
      });
      c.encodingDescriptor = descriptor(socket, "setEncoding");
      c.noEncoding = () => {
        throw error(Error, "ERR_HTTP_SOCKET_ENCODING", "HTTP sockets require byte data");
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
      if (c.timeout !== void 0) {
        socket.setTimeout(c.timeout);
      }
      if (c.noDelay !== void 0) {
        socket.setNoDelay(c.noDelay);
      }
      if (c.keepAlive) {
        socket.setKeepAlive(...c.keepAlive);
      }
      const direct = directToken === clientToken;
      if (!direct) {
        c.ready = c.transport.protocol === "https:" ? !socket.secureConnecting : !socket.connecting;
      }
      tick(() => {
        if (this.destroyed) {
          return;
        }
        this.socket = socket;
        c.published = true;
        this.emit("socket", socket);
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
          } catch (err2) {
            this.destroy(err2);
          }
        } else if (c.ready) {
          this._activate();
        }
      });
    }
    _prepare() {
      const s = outgoing.get(this);
      if (clients.get(this)?.upgraded) {
        throw error(Error, "ERR_STREAM_DESTROYED", "HTTP request has upgraded");
      }
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
      validateConnection(headers.get("upgrade")?.value, headers.get("connection")?.value);
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
      tick(() => this.emit("abort"));
      this.destroy();
    }
    setTimeout(value, callback) {
      if (callback) {
        this.once("timeout", callback);
      }
      integer(value, "timeout");
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
      integer(initialDelay, "initialDelay");
      const c = clients.get(this);
      c.keepAlive = [!!enable, initialDelay];
      c.socket?.setKeepAlive(enable, initialDelay);
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
  function requestWithTransport(transport, input, options, callback) {
    return new ClientRequest(input, options, callback, clientToken, transport);
  }
  return { ClientRequest, request, get, requestWithTransport };
}

// mite/modules/network/http/server.mjs
function createHttpServer(host, protocol, messages) {
  const { Server: TCPServer } = host.net;
  const Buffer = host.Buffer;
  const tick = host.process.nextTick;
  const native = host.native;
  const { STATUS_CODES, error, unsupported, merged, integer } = protocol;
  const { IncomingMessage, ServerResponse, incoming, outgoing } = messages;
  const servers = /* @__PURE__ */ new WeakMap();
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
  return { Server, createServer };
}

// mite/modules/network/http/agent.mjs
function getAgentName(options = {}, secure = false) {
  let key = `${options.host || "localhost"}:${options.port || ""}:${options.localAddress || ""}`;
  if (options.family === 4 || options.family === 6) {
    key += `:${options.family}`;
  }
  if (options.socketPath) {
    key += `:${options.socketPath}`;
  }
  if (!secure) {
    return key;
  }
  for (const field of [
    "ca",
    "cert",
    "clientCertEngine",
    "ciphers",
    "key",
    "pfx",
    "rejectUnauthorized",
    "servername",
    "minVersion",
    "maxVersion",
    "secureProtocol",
    "crl",
    "honorCipherOrder",
    "ecdhCurve",
    "dhparam",
    "secureOptions",
    "sessionIdContext",
    "sigalgs",
    "privateKeyIdentifier",
    "privateKeyEngine"
  ]) {
    const value = options[field];
    const present = ["rejectUnauthorized", "honorCipherOrder", "secureOptions"].includes(field) ? value !== void 0 : !!value;
    key += ":";
    if (present && !(field === "servername" && value === options.host)) {
      key += field === "sigalgs" ? JSON.stringify(value) : String(value);
    }
  }
  return key;
}
function createHttpAgent(host, protocol, defaultConnect) {
  const { EventEmitter, net, process } = host;
  const tick = process.nextTick.bind(process);
  const Socket = net.Socket;
  const states = /* @__PURE__ */ new WeakMap();
  const socketOwners = /* @__PURE__ */ new WeakMap();
  function error(code, message, Type = Error) {
    return Object.assign(new Type(message), { code });
  }
  function unsupported(message) {
    throw error("ERR_MITE_UNSUPPORTED_API", message);
  }
  function stateFor(agent) {
    const state = states.get(agent);
    if (!state) {
      throw error("ERR_INVALID_THIS", "Invalid Agent receiver", TypeError);
    }
    return state;
  }
  function maximum(value, name) {
    if (value !== Infinity && (typeof value !== "number" || !Number.isInteger(value) || value < 1)) {
      throw error("ERR_OUT_OF_RANGE", `${name} must be a positive integer or Infinity`, RangeError);
    }
    return value;
  }
  function removePublic(agent, table, name, value) {
    const list = agent[table][name];
    if (!Array.isArray(list)) {
      return;
    }
    const index = list.indexOf(value);
    if (index !== -1) {
      list.splice(index, 1);
    }
    if (!list.length) {
      delete agent[table][name];
    }
  }
  function destroyExtra(socket, entry) {
    if (socket instanceof Socket && socket !== entry.socket && socket !== entry.delegated && !socketOwners.has(socket) && !socket.destroyed) {
      socket.destroy();
    }
  }
  function schedule(agent, state) {
    if (state.scheduled || state.destroying) {
      return;
    }
    state.scheduled = true;
    tick(() => {
      state.scheduled = false;
      pump(agent, state);
    });
  }
  function release(agent, state, entry) {
    if (entry.phase === "released") {
      return;
    }
    const queued = entry.phase === "queued";
    entry.phase = "released";
    state.entries.delete(entry);
    state.byRequest.delete(entry.req);
    const index = state.queue.indexOf(entry);
    if (index !== -1) {
      state.queue.splice(index, 1);
    }
    if (queued) {
      removePublic(agent, "requests", entry.name, entry.req);
    }
    if (entry.socket) {
      removePublic(agent, "sockets", entry.name, entry.socket);
      if (socketOwners.get(entry.socket) === entry) {
        socketOwners.delete(entry.socket);
      }
      agent.totalSocketCount--;
      entry.socket.removeListener("close", entry.socketClose);
      entry.socket.removeListener("agentRemove", entry.agentRemove);
    }
    entry.req.removeListener("close", entry.requestClose);
    entry.req.removeListener("socket", entry.requestSocket);
    schedule(agent, state);
  }
  function record(agent, state, req, options) {
    const entry = {
      req,
      options,
      name: agent.getName(options),
      phase: "queued",
      socket: null,
      delegated: null,
      callbackDone: false
    };
    entry.requestClose = () => {
      if (entry.phase === "socket") {
        if (!entry.socket.destroyed) {
          entry.socket.destroy();
        }
      } else {
        release(agent, state, entry);
      }
    };
    entry.requestSocket = (socket) => {
      if (entry.phase === "connecting" && entry.socket !== socket) {
        entry.delegated = socket;
        release(agent, state, entry);
      }
    };
    state.entries.add(entry);
    state.byRequest.set(req, entry);
    req.once("close", entry.requestClose);
    req.on("socket", entry.requestSocket);
    return entry;
  }
  function available(agent, state, name) {
    const perOrigin = maximum(agent.maxSockets, "maxSockets");
    const total = maximum(agent.maxTotalSockets, "maxTotalSockets");
    let originCount = 0;
    let count = 0;
    for (const entry of state.entries) {
      if (entry.phase !== "connecting" && entry.phase !== "socket") {
        continue;
      }
      count++;
      if (entry.name === name) {
        originCount++;
      }
    }
    return count < total && originCount < perOrigin;
  }
  function ownSocket(agent, state, entry, socket) {
    if (!(socket instanceof Socket) || socket.destroyed) {
      throw error(
        "ERR_INVALID_RETURN_VALUE",
        "Agent connector must return a live net.Socket",
        TypeError
      );
    }
    if (entry.socket === socket) {
      return;
    }
    if (socketOwners.has(socket)) {
      throw error("ERR_INVALID_ARG_VALUE", "Socket already belongs to a request");
    }
    if (entry.phase === "released" || entry.req.destroyed || state.destroying) {
      destroyExtra(socket, entry);
      throw error("ECONNRESET", "Request closed before socket admission");
    }
    entry.socket = socket;
    entry.phase = "socket";
    socketOwners.set(socket, entry);
    agent.sockets[entry.name] ??= [];
    agent.sockets[entry.name].push(socket);
    agent.totalSocketCount++;
    entry.socketClose = () => release(agent, state, entry);
    entry.agentRemove = () => release(agent, state, entry);
    socket.once("close", entry.socketClose);
    socket.once("agentRemove", entry.agentRemove);
  }
  function dispatch(agent, state, entry) {
    entry.phase = "connecting";
    entry.dispatched = true;
    removePublic(agent, "requests", entry.name, entry.req);
    const done = (failure, socket) => {
      if (entry.callbackDone) {
        destroyExtra(socket, entry);
        return;
      }
      entry.callbackDone = true;
      if (entry.phase === "released" || entry.req.destroyed) {
        destroyExtra(socket, entry);
        return;
      }
      try {
        if (!failure) {
          ownSocket(agent, state, entry, socket);
        }
      } catch (err) {
        failure = err;
      }
      if (failure) {
        destroyExtra(socket, entry);
        release(agent, state, entry);
        entry.req.onSocket(null, failure);
      } else {
        entry.req.onSocket(socket);
      }
    };
    try {
      agent.createSocket(entry.req, entry.options, done);
    } catch (failure) {
      done(failure);
    }
  }
  function pump(agent, state) {
    if (state.pumping || state.destroying) {
      return;
    }
    state.pumping = true;
    try {
      for (let index = 0; index < state.queue.length; ) {
        const entry = state.queue[index];
        if (entry.req.destroyed) {
          release(agent, state, entry);
          continue;
        }
        let allowed;
        try {
          allowed = available(agent, state, entry.name);
        } catch (failure) {
          release(agent, state, entry);
          entry.req.onSocket(null, failure);
          continue;
        }
        if (!allowed) {
          index++;
          continue;
        }
        state.queue.splice(index, 1);
        dispatch(agent, state, entry);
      }
    } finally {
      state.pumping = false;
    }
  }
  class Agent extends EventEmitter {
    constructor(options = {}) {
      super();
      if (!options || typeof options !== "object" || Array.isArray(options)) {
        throw error("ERR_INVALID_ARG_TYPE", "Agent options must be an object", TypeError);
      }
      if (options.keepAlive === true) {
        unsupported("Agent keep-alive/reuse is not implemented");
      }
      if (options.keepAlive !== void 0 && typeof options.keepAlive !== "boolean") {
        throw error("ERR_INVALID_ARG_TYPE", "keepAlive must be boolean", TypeError);
      }
      if (options.proxyEnv !== void 0) {
        unsupported("Agent proxyEnv is not implemented");
      }
      this.options = { ...options, path: null };
      this.options.noDelay ??= true;
      this.defaultPort = options.defaultPort ?? (protocol === "https:" ? 443 : 80);
      this.protocol = options.protocol ?? protocol;
      this.requests = /* @__PURE__ */ Object.create(null);
      this.sockets = /* @__PURE__ */ Object.create(null);
      this.freeSockets = /* @__PURE__ */ Object.create(null);
      this.keepAlive = false;
      this.keepAliveMsecs = options.keepAliveMsecs ?? 1e3;
      this.maxSockets = maximum(options.maxSockets ?? Agent.defaultMaxSockets, "maxSockets");
      this.maxTotalSockets = maximum(options.maxTotalSockets ?? Infinity, "maxTotalSockets");
      this.maxFreeSockets = options.maxFreeSockets ?? 256;
      this.totalSocketCount = 0;
      this.scheduling = options.scheduling ?? "lifo";
      if (!["lifo", "fifo"].includes(this.scheduling)) {
        throw error("ERR_INVALID_ARG_VALUE", "Invalid Agent scheduling");
      }
      states.set(this, {
        entries: /* @__PURE__ */ new Set(),
        byRequest: /* @__PURE__ */ new WeakMap(),
        queue: [],
        scheduled: false,
        pumping: false,
        destroying: false
      });
    }
    getName(options = {}) {
      return getAgentName(options, protocol === "https:");
    }
    createConnection(options, callback) {
      stateFor(this);
      return defaultConnect(options, callback);
    }
    createSocket(req, options, callback) {
      const state = stateFor(this);
      if (typeof callback !== "function") {
        throw error("ERR_INVALID_ARG_TYPE", "callback must be a function", TypeError);
      }
      let entry = state.byRequest.get(req);
      if (!entry) {
        const merged = { ...options, ...this.options };
        entry = record(this, state, req, merged);
        if (!available(this, state, entry.name)) {
          release(this, state, entry);
          tick(() => callback(error("ERR_SOCKET_CANNOT_SEND", "Agent connection limit reached")));
          return;
        }
        entry.phase = "connecting";
      }
      let completed = false;
      let accepted;
      const finish = (failure, socket) => {
        if (completed) {
          if (socket !== accepted) {
            destroyExtra(socket, entry);
          }
          return;
        }
        completed = true;
        if (entry.phase === "released" || req.destroyed) {
          destroyExtra(socket, entry);
          return;
        }
        try {
          if (!failure) {
            ownSocket(this, state, entry, socket);
          }
        } catch (err) {
          failure = err;
        }
        if (failure) {
          destroyExtra(socket, entry);
          if (!entry.dispatched) {
            release(this, state, entry);
          }
        } else {
          accepted = socket;
        }
        callback(failure, socket);
      };
      try {
        const socket = this.createConnection({ ...entry.options, ...this.options }, finish);
        if (socket !== void 0 && socket !== null) {
          finish(null, socket);
        }
      } catch (failure) {
        finish(failure);
      }
    }
    addRequest(req, options, port, localAddress) {
      const state = stateFor(this);
      if (!req || typeof req.onSocket !== "function" || typeof req.once !== "function") {
        throw error("ERR_INVALID_ARG_TYPE", "request must be a ClientRequest", TypeError);
      }
      if (req.destroyed) {
        return;
      }
      req.shouldKeepAlive = false;
      if (state.destroying) {
        req.onSocket(null, error("ECONNRESET", "Agent is being destroyed"));
        return;
      }
      if (this.keepAlive) {
        unsupported("Agent keep-alive/reuse is not implemented");
      }
      if (state.byRequest.has(req)) {
        throw error("ERR_INVALID_ARG_VALUE", "Request already admitted by this Agent");
      }
      if (typeof options === "string") {
        options = { host: options, port, localAddress };
      }
      const merged = { ...options, ...this.options };
      const entry = record(this, state, req, merged);
      this.requests[entry.name] ??= [];
      this.requests[entry.name].push(req);
      state.queue.push(entry);
      pump(this, state);
    }
    destroy() {
      const state = stateFor(this);
      state.destroying = true;
      try {
        for (const entry of [...state.entries]) {
          if (entry.socket) {
            if (!entry.socket.destroyed) {
              entry.socket.destroy();
            }
          } else {
            release(this, state, entry);
            entry.req.onSocket(null, error("ECONNRESET", "Agent connection destroyed"));
          }
        }
      } finally {
        state.destroying = false;
      }
    }
    keepSocketAlive() {
      stateFor(this);
      return false;
    }
    reuseSocket() {
      stateFor(this);
      unsupported("Agent socket reuse is not implemented");
    }
  }
  Agent.defaultMaxSockets = Infinity;
  return { Agent, globalAgent: new Agent() };
}

// mite/modules/network/http.mjs
function createHttpModule(host) {
  const protocol = createHttpProtocol(host.native);
  const messages = createHttpMessages(host, protocol);
  const agents = createHttpAgent(host, "http:", (options) => {
    for (const key of ["socketPath", "localAddress", "localPort", "proxyEnv"]) {
      if (options[key] !== void 0) {
        protocol.unsupported(`HTTP connection option ${key}`);
      }
    }
    if (options.path != null) {
      protocol.unsupported("HTTP Unix-domain connections");
    }
    return host.net.createConnection({
      port: options.port ?? 80,
      host: options.host ?? options.hostname ?? "localhost",
      family: options.family,
      lookup: options.lookup,
      hints: options.hints,
      autoSelectFamily: options.autoSelectFamily,
      autoSelectFamilyAttemptTimeout: options.autoSelectFamilyAttemptTimeout,
      noDelay: options.noDelay ?? true,
      keepAlive: false,
      highWaterMark: options.highWaterMark
    });
  });
  const client = createHttpClient(host, protocol, messages, agents);
  const server = createHttpServer(host, protocol, messages);
  Object.defineProperty(host.clientHooks, "request", { value: client.requestWithTransport });
  Object.defineProperty(host.clientHooks, "Agent", { value: agents.Agent });
  return {
    Agent: agents.Agent,
    globalAgent: agents.globalAgent,
    Server: server.Server,
    ServerResponse: messages.ServerResponse,
    OutgoingMessage: messages.OutgoingMessage,
    IncomingMessage: messages.IncomingMessage,
    ClientRequest: client.ClientRequest,
    request: client.request,
    get: client.get,
    createServer: server.createServer,
    METHODS: protocol.METHODS,
    STATUS_CODES: protocol.STATUS_CODES,
    validateHeaderName: protocol.validateHeaderName,
    validateHeaderValue: protocol.validateHeaderValue,
    maxHeaderSize: 16384
  };
}
return createHttpModule;
})()
