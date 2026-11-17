/* Generated from mite/modules/terminal/pty.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/terminal/pty.mjs
function createPty(host) {
  const { native, EventEmitter, StringDecoder, Buffer, process, signals } = host;
  const states = /* @__PURE__ */ new WeakMap();
  const error = (code, message, Type = Error) => Object.assign(new Type(message), { code });
  const unsupported = (name) => {
    throw error("ERR_MITE_UNSUPPORTED_API", `PTY ${name} is not implemented`);
  };
  function text(value, label, maximum = 65536) {
    if (typeof value !== "string") {
      throw error("ERR_INVALID_ARG_TYPE", `${label} must be a string`, TypeError);
    }
    if (value.includes("\0") || Buffer.byteLength(value) > maximum) {
      throw error("ERR_INVALID_ARG_VALUE", `${label} exceeds its byte limit or contains NUL`);
    }
    return value;
  }
  function integer(value, label, minimum = 1) {
    if (!Number.isInteger(value) || value < minimum || value > 65535) {
      throw error("ERR_OUT_OF_RANGE", `${label} must be ${minimum}..65535`, RangeError);
    }
    return value;
  }
  function record(value, label) {
    if (!value || typeof value !== "object" || Array.isArray(value)) {
      throw error("ERR_INVALID_ARG_TYPE", `${label} must be an object`, TypeError);
    }
  }
  function prepare(file, args, options) {
    const command = text(file, "file");
    if (!command) {
      throw error("ERR_INVALID_ARG_VALUE", "file must not be empty");
    }
    if (!Array.isArray(args) || args.length > 1024) {
      throw error(
        "ERR_INVALID_ARG_VALUE",
        "PTY arguments must be an array of at most 1024 strings"
      );
    }
    const argumentsCopy = [];
    for (let index = 0; index < args.length; index++) {
      argumentsCopy.push(text(args[index], "argument"));
    }
    record(options, "PTY options");
    const accepted = /* @__PURE__ */ new Set([
      "name",
      "cols",
      "rows",
      "cwd",
      "env",
      "encoding",
      "handleFlowControl",
      "flowControlPause",
      "flowControlResume",
      "uid",
      "gid",
      "useConpty",
      "useConptyDll",
      "conptyInheritCursor"
    ]);
    const snapshot = /* @__PURE__ */ Object.create(null);
    for (const key of Reflect.ownKeys(options)) {
      if (!accepted.has(key)) {
        unsupported("option " + String(key));
      }
      snapshot[key] = options[key];
    }
    for (const key of ["uid", "gid"]) {
      if (snapshot[key] !== void 0 && snapshot[key] !== -1) {
        unsupported(key);
      }
    }
    for (const key of ["useConpty", "useConptyDll", "conptyInheritCursor"]) {
      if (snapshot[key] !== void 0 && snapshot[key] !== false) {
        unsupported(key);
      }
    }
    if (snapshot.handleFlowControl !== void 0 && typeof snapshot.handleFlowControl !== "boolean") {
      throw error("ERR_INVALID_ARG_TYPE", "handleFlowControl must be boolean", TypeError);
    }
    const encoding = snapshot.encoding === void 0 ? "utf8" : snapshot.encoding;
    if (encoding !== null && encoding !== "utf8" && encoding !== "utf-8") {
      unsupported("encoding");
    }
    const cwd = text(snapshot.cwd ?? process.cwd(), "cwd");
    const source = snapshot.env ?? process.env;
    record(source, "PTY environment");
    const keys = Object.keys(source);
    if (keys.length > 510) {
      throw error("ERR_MITE_PTY_LIMIT", "PTY environment exceeds 512 entries");
    }
    const environment = /* @__PURE__ */ Object.create(null);
    for (const key of keys) {
      text(key, "environment key");
      if (!key || key.includes("=")) {
        throw error("ERR_INVALID_ARG_VALUE", "Invalid PTY environment key");
      }
      const value = source[key];
      if (value !== void 0) {
        environment[key] = text(value, "environment value");
      }
    }
    environment.PWD = cwd;
    environment.TERM = text(snapshot.name ?? environment.TERM ?? "xterm", "name");
    const env = Object.entries(environment).map(([key, value]) => key + "=" + value);
    const budget = [command, cwd, ...argumentsCopy, ...env].reduce(
      (sum, value) => sum + Buffer.byteLength(value) + 1,
      0
    );
    if (budget > 1048576) {
      throw error("ERR_MITE_PTY_LIMIT", "PTY argument/environment budget exceeds 1 MiB");
    }
    return {
      spec: {
        file: command,
        args: argumentsCopy,
        cwd,
        env,
        cols: integer(snapshot.cols ?? 80, "cols"),
        rows: integer(snapshot.rows ?? 24, "rows")
      },
      encoding,
      flow: snapshot.handleFlowControl ?? false,
      pause: text(snapshot.flowControlPause ?? "", "flowControlPause", 4096),
      resume: text(snapshot.flowControlResume ?? "", "flowControlResume", 4096)
    };
  }
  function state(terminal) {
    const value = states.get(terminal);
    if (!value) {
      throw error("ERR_INVALID_THIS", "Invalid PTY receiver", TypeError);
    }
    return value;
  }
  function writable(terminal) {
    const value = state(terminal);
    if (value.closed || value.destroyed) {
      throw error("ERR_MITE_PTY_CLOSED", "PTY is closed");
    }
    return value;
  }
  function subscribe(terminal, event, listener, transform) {
    if (typeof listener !== "function") {
      throw error("ERR_INVALID_ARG_TYPE", "PTY listener must be a function", TypeError);
    }
    const callback = transform ? (code, signal) => listener({ exitCode: code, signal }) : listener;
    terminal.on(event, callback);
    let active = true;
    return {
      dispose() {
        if (active) {
          active = false;
          terminal.removeListener(event, callback);
        }
      }
    };
  }
  class Terminal extends EventEmitter {
    constructor(prepared) {
      super();
      const value = {
        id: null,
        pid: 0,
        cols: prepared.spec.cols,
        rows: prepared.spec.rows,
        closed: false,
        destroyed: false,
        decoder: prepared.encoding === null ? null : new StringDecoder("utf8"),
        pause: prepared.pause,
        resume: prepared.resume
      };
      states.set(this, value);
      this.handleFlowControl = prepared.flow;
      Object.defineProperties(this, {
        onData: { value: (listener) => subscribe(this, "data", listener, false) },
        onExit: { value: (listener) => subscribe(this, "exit", listener, true) }
      });
      const started = native.spawn(prepared.spec, (kind, payload, signal) => {
        if (value.closed) {
          return;
        }
        if (kind === "data") {
          if (!value.destroyed) {
            const data = value.decoder ? value.decoder.write(Buffer.from(payload)) : Buffer.from(payload);
            if (data.length) {
              this.emit("data", data);
            }
          }
        } else if (kind === "exit") {
          value.closed = true;
          const tail = value.decoder?.end();
          if (tail && !value.destroyed) {
            this.emit("data", tail);
          }
          this.emit("close");
          this.emit("exit", payload, signal);
        } else if (kind === "error") {
          this.emit("error", payload);
        } else {
          throw error("ERR_MITE_PTY_DATA", "Invalid native PTY event");
        }
      });
      value.id = started.id;
      value.pid = started.pid;
    }
    get pid() {
      return state(this).pid;
    }
    get cols() {
      return state(this).cols;
    }
    get rows() {
      return state(this).rows;
    }
    get process() {
      const value = state(this);
      return value.closed || value.destroyed ? "" : native.process(value.id);
    }
    write(data) {
      const value = writable(this);
      if (this.handleFlowControl && (data === value.pause || data === value.resume)) {
        data === value.pause ? this.pause() : this.resume();
        return;
      }
      if (typeof data !== "string" && !Buffer.isBuffer(data) && !(data instanceof Uint8Array)) {
        throw error(
          "ERR_INVALID_ARG_TYPE",
          "PTY write requires a string or byte buffer",
          TypeError
        );
      }
      native.write(value.id, Buffer.from(data));
    }
    resize(cols, rows, pixelSize) {
      const value = writable(this);
      integer(cols, "cols");
      integer(rows, "rows");
      let width = 0;
      let height = 0;
      if (pixelSize !== void 0) {
        record(pixelSize, "pixelSize");
        width = integer(pixelSize.width, "pixel width", 0);
        height = integer(pixelSize.height, "pixel height", 0);
      }
      native.resize(value.id, cols, rows, width, height);
      value.cols = cols;
      value.rows = rows;
    }
    pause() {
      const value = state(this);
      if (!value.closed && !value.destroyed) {
        native.pause(value.id, true);
      }
    }
    resume() {
      const value = state(this);
      if (!value.closed && !value.destroyed) {
        native.pause(value.id, false);
      }
    }
    kill(signal = "SIGHUP") {
      const value = state(this);
      if (typeof signal !== "string" || !Object.hasOwn(signals, signal)) {
        throw error("ERR_UNKNOWN_SIGNAL", "Unknown PTY signal");
      }
      if (!value.closed) {
        native.kill(value.id, signal);
      }
    }
    destroy() {
      const value = state(this);
      if (!value.closed && !value.destroyed) {
        native.destroy(value.id);
        value.destroyed = true;
      }
    }
    clear() {
      state(this);
    }
    setParentWindow() {
      state(this);
      return false;
    }
  }
  function spawn(file, args = [], options = {}) {
    return new Terminal(prepare(file, args, options));
  }
  return {
    spawn,
    fork: spawn,
    createTerminal: spawn,
    open() {
      unsupported("open");
    },
    get native() {
      unsupported("raw native addon API");
    }
  };
}
return createPty;
})()
