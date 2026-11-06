/* Generated from mite/modules/network/tls.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/network/tls.mjs
function createTlsModule(host) {
  const { Socket } = host.net;
  const { Buffer, abort, native } = host;
  const tick = host.process.nextTick;
  const states = /* @__PURE__ */ new WeakMap();
  const token = {};
  function failure(code, message, Type = Error) {
    return Object.assign(new Type(message), { code });
  }
  function unsupported(name) {
    throw failure("ERR_MITE_UNSUPPORTED_API", `${name} is not implemented`);
  }
  function configuration(options) {
    for (const key of [
      "socket",
      "secureContext",
      "isServer",
      "cert",
      "key",
      "pfx",
      "ciphers",
      "checkServerIdentity",
      "session",
      "pskCallback",
      "secureProtocol",
      "requestCert",
      "enableTrace",
      "handshakeTimeout"
    ]) {
      if (options[key] !== void 0) {
        unsupported(`TLS option ${key}`);
      }
    }
    if (options.timeout !== void 0 && (typeof options.timeout !== "number" || !Number.isFinite(options.timeout) || options.timeout < 0)) {
      throw failure(
        typeof options.timeout === "number" ? "ERR_OUT_OF_RANGE" : "ERR_INVALID_ARG_TYPE",
        "Invalid TLS timeout",
        TypeError
      );
    }
    const hostname = options.host ?? "localhost";
    const servername = options.servername ?? "";
    if (typeof hostname !== "string" || !hostname || hostname.includes("\0") || typeof servername !== "string" || servername.includes("\0")) {
      throw failure("ERR_INVALID_ARG_VALUE", "Invalid TLS host/servername", TypeError);
    }
    const identity = servername || hostname;
    if (servername && host.net.isIP(servername)) {
      throw failure("ERR_INVALID_ARG_VALUE", "TLS SNI requires a DNS name", TypeError);
    }
    const reject = options.rejectUnauthorized ?? true;
    if (typeof reject !== "boolean") {
      throw failure("ERR_INVALID_ARG_TYPE", "rejectUnauthorized must be boolean", TypeError);
    }
    if (options.signal && !abort.isSignal(options.signal)) {
      throw failure("ERR_INVALID_ARG_TYPE", "signal must be an AbortSignal", TypeError);
    }
    let ca;
    if (options.ca !== void 0) {
      ca = (Array.isArray(options.ca) ? options.ca : [options.ca]).map((value) => {
        if (typeof value === "string") {
          return value;
        }
        if (value instanceof Uint8Array) {
          return Buffer.from(value).toString("utf8");
        }
        throw failure("ERR_INVALID_ARG_TYPE", "ca must contain PEM strings or bytes", TypeError);
      }).join("\n");
      if (Buffer.byteLength(ca) > 1024 * 1024) {
        throw failure("ERR_TLS_BUFFER_LIMIT", "CA input exceeds one MiB");
      }
    }
    const versions = /* @__PURE__ */ new Map([
      ["TLSv1.2", 771],
      ["TLSv1.3", 772]
    ]);
    const minimum = versions.get(options.minVersion ?? "TLSv1.2");
    const maximum = versions.get(options.maxVersion ?? "TLSv1.3");
    if (!minimum || !maximum || minimum > maximum) {
      throw failure("ERR_TLS_INVALID_PROTOCOL_VERSION", "Only TLSv1.2/TLSv1.3 are supported");
    }
    const protocols = options.ALPNProtocols ?? [];
    if (!Array.isArray(protocols)) {
      throw failure("ERR_INVALID_ARG_TYPE", "ALPNProtocols must be an array of strings", TypeError);
    }
    const alpn = Buffer.concat(
      protocols.map((value) => {
        if (typeof value !== "string" || !value || Buffer.byteLength(value) > 255) {
          throw failure(
            "ERR_INVALID_ARG_VALUE",
            "ALPN protocols must be 1–255 UTF-8 bytes",
            TypeError
          );
        }
        const bytes = Buffer.from(value);
        return Buffer.concat([Buffer.from([bytes.length]), bytes]);
      })
    );
    if (alpn.length > 65535) {
      throw failure("ERR_INVALID_ARG_VALUE", "ALPN list exceeds 65535 bytes", TypeError);
    }
    return { identity, servername, ca, reject, minimum, maximum, alpn };
  }
  function pump(socket) {
    const state = states.get(socket);
    const pending = state.pending;
    if (!state.ready || !pending || pending.sending || socket.destroyed) {
      return;
    }
    try {
      const frame = state.engine.write(
        pending.data.subarray(pending.offset, pending.offset + 16384)
      );
      pending.offset += frame.written;
      pending.sending = true;
      output(socket, frame, (error) => {
        if (state.pending !== pending || socket.destroyed) {
          return;
        }
        pending.sending = false;
        if (error) {
          socket.destroy(error);
        } else if (pending.offset === pending.data.length) {
          state.pending = null;
          pending.callback();
        } else if (frame.written) {
          pump(socket);
        }
      });
    } catch (error) {
      socket.destroy(error);
    }
  }
  function output(socket, frame, callback = () => {
  }) {
    const state = states.get(socket);
    if (frame.output.length) {
      state.transport.write(Buffer.from(frame.output), callback);
    } else {
      tick(callback);
    }
    if (socket.destroyed) {
      return;
    }
    if (frame.ready && !state.ready) {
      state.ready = true;
      state.protocol = frame.protocol;
      socket.secureConnecting = false;
      socket.authorized = frame.authorized;
      socket.authorizationError = frame.authorizationError || null;
      socket.alpnProtocol = frame.alpn || false;
      socket.emit("secureConnect");
    }
    if (socket.destroyed) {
      return;
    }
    if (frame.data.length) {
      socket.bytesRead += frame.data.length;
      if (!socket.push(Buffer.from(frame.data))) {
        state.transport.pause();
      }
    }
    if (frame.ended && !state.ended) {
      state.ended = true;
      socket.push(null);
    }
  }
  class TLSSocket extends Socket {
    constructor(privateToken, options) {
      if (privateToken !== token) {
        unsupported("Direct TLSSocket construction/wrapping");
      }
      const config = configuration(options);
      super({ allowHalfOpen: options.allowHalfOpen, highWaterMark: options.highWaterMark });
      const engine = native.create(
        config.identity,
        config.servername,
        config.ca,
        config.reject,
        config.minimum,
        config.maximum,
        config.alpn
      );
      const transport = new Socket({ noDelay: options.noDelay ?? true });
      const state = {
        engine,
        transport,
        ready: false,
        ended: false,
        closed: false,
        pending: null,
        final: null,
        closing: null,
        unsubscribe: null,
        protocol: null
      };
      states.set(this, state);
      this.encrypted = true;
      this.authorized = false;
      this.authorizationError = null;
      this.alpnProtocol = false;
      this.secureConnecting = true;
      this.servername = config.servername || void 0;
      transport.on("error", (error) => this.destroy(error));
      transport.on("timeout", () => this.emit("timeout"));
      transport.on("data", (data) => {
        if (this.destroyed) {
          return;
        }
        try {
          output(this, engine.receive(data), (error) => error && this.destroy(error));
          pump(this);
        } catch (error) {
          this.destroy(error);
        }
      });
      transport.once("connect", () => {
        this.connecting = false;
        this.emit("connect");
        if (!this.destroyed) {
          try {
            output(this, engine.receive(), (error) => error && this.destroy(error));
          } catch (error) {
            this.destroy(error);
          }
        }
      });
      transport.on("end", () => {
        if (!state.ended && !this.destroyed) {
          this.destroy(
            failure("ERR_SSL_UNEXPECTED_EOF_WHILE_READING", "TLS peer closed without close_notify")
          );
        }
      });
      transport.once("close", () => {
        state.closed = true;
        if (!this.destroyed) {
          this.destroy(state.ended ? void 0 : failure("ECONNRESET", "TLS transport closed"));
        }
        state.closing?.();
        state.closing = null;
      });
      if (options.signal) {
        const signal = options.signal;
        const cancel = () => {
          const error = failure("ABORT_ERR", "The operation was aborted");
          error.name = "AbortError";
          error.cause = abort.reason(signal);
          this.destroy(error);
        };
        if (signal.aborted) {
          tick(cancel);
        } else {
          state.unsubscribe = abort.subscribe(signal, cancel);
        }
      }
      if (options.timeout !== void 0) {
        this.setTimeout(options.timeout);
      }
    }
    connect(...args) {
      const state = states.get(this);
      const previous = this.connecting;
      this.connecting = true;
      try {
        state.transport.connect(...args);
      } catch (error) {
        this.connecting = previous;
        throw error;
      }
      return this;
    }
    _read() {
      states.get(this)?.transport.resume();
    }
    _write(data, encoding, callback) {
      const state = states.get(this);
      this.bytesWritten += data.length;
      state.pending = { data, offset: 0, callback, sending: false };
      pump(this);
    }
    _final(callback) {
      const state = states.get(this);
      const finish = () => {
        if (this.destroyed) {
          return;
        }
        try {
          output(this, state.engine.shutdown(), (error) => {
            if (error) {
              callback(error);
            } else {
              state.transport.end(callback);
            }
          });
        } catch (error) {
          callback(error);
        }
      };
      if (state.ready) {
        finish();
      } else {
        this.once("secureConnect", finish);
      }
    }
    _destroy(error, callback) {
      const state = states.get(this);
      this._hadError = !!error;
      this.connecting = false;
      this.secureConnecting = false;
      state.unsubscribe?.();
      state.unsubscribe = null;
      state.engine.close();
      if (state.pending) {
        const pending = state.pending;
        state.pending = null;
        pending.callback(error ?? failure("ERR_STREAM_DESTROYED", "TLS stream destroyed"));
      }
      if (state.closed) {
        callback(error);
      } else {
        state.closing = () => callback(error);
        state.transport.destroy();
      }
    }
    address() {
      return states.get(this)?.transport.address() ?? {};
    }
    get remoteAddress() {
      return states.get(this)?.transport.remoteAddress;
    }
    get remotePort() {
      return states.get(this)?.transport.remotePort;
    }
    get remoteFamily() {
      return states.get(this)?.transport.remoteFamily;
    }
    get localAddress() {
      return states.get(this)?.transport.localAddress;
    }
    get localPort() {
      return states.get(this)?.transport.localPort;
    }
    get localFamily() {
      return states.get(this)?.transport.localFamily;
    }
    get pending() {
      return states.get(this)?.transport.pending ?? true;
    }
    getProtocol() {
      return states.get(this).protocol;
    }
    setTimeout(value, callback) {
      if (callback) {
        this.once("timeout", callback);
      }
      states.get(this).transport.setTimeout(value);
      return this;
    }
    setNoDelay(value = true) {
      states.get(this).transport.setNoDelay(value);
      return this;
    }
    setKeepAlive(value = false, delay = 0) {
      states.get(this).transport.setKeepAlive(value, delay);
      return this;
    }
    ref() {
      states.get(this).transport.ref();
      return this;
    }
    unref() {
      states.get(this).transport.unref();
      return this;
    }
    getPeerCertificate() {
      unsupported("TLSSocket.getPeerCertificate");
    }
    getCipher() {
      unsupported("TLSSocket.getCipher");
    }
    renegotiate() {
      unsupported("TLS renegotiation");
    }
    exportKeyingMaterial() {
      unsupported("TLS keying material export");
    }
  }
  function connect(...args) {
    const list = [...args];
    const callback = typeof list.at(-1) === "function" ? list.pop() : void 0;
    const options = typeof list[0] === "object" && list[0] !== null ? { ...list[0] } : {
      ...typeof list[1] === "object" ? list[1] : list[2],
      port: list[0],
      ...typeof list[1] === "string" ? { host: list[1] } : {}
    };
    const socket = new TLSSocket(token, options);
    if (callback) {
      socket.once("secureConnect", callback);
    }
    try {
      socket.connect({
        port: options.port,
        host: options.host ?? "localhost",
        family: options.family,
        lookup: options.lookup,
        hints: options.hints,
        autoSelectFamily: options.autoSelectFamily,
        autoSelectFamilyAttemptTimeout: options.autoSelectFamilyAttemptTimeout
      });
      return socket;
    } catch (error) {
      socket.destroy();
      throw error;
    }
  }
  return {
    connect,
    TLSSocket,
    createServer() {
      unsupported("TLS servers");
    },
    createSecureContext() {
      unsupported("Reusable TLS SecureContext");
    }
  };
}
return createTlsModule;
})()
