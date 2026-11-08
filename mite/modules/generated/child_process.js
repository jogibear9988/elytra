/* Generated from mite/modules/process/child-process.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/process/child-process.mjs
function createChildProcessModule(host) {
  const native = host.native;
  const Buffer = host.Buffer;
  const tick = host.process.nextTick;
  const { Readable, Writable } = host.stream;
  const { EventEmitter } = host.events;
  const children = /* @__PURE__ */ new WeakMap();
  const pipes = /* @__PURE__ */ new WeakMap();
  const token = {};
  function fail(Type, code, message) {
    const e = new Type(message);
    e.code = code;
    throw e;
  }
  function unsupported(name) {
    fail(Error, "ERR_MITE_UNSUPPORTED_API", `${name} is not implemented`);
  }
  function text(value, name) {
    if (typeof value !== "string") {
      fail(TypeError, "ERR_INVALID_ARG_TYPE", `${name} must be a string`);
    }
    if (value.includes("\0")) {
      fail(TypeError, "ERR_INVALID_ARG_VALUE", `${name} must not contain null bytes`);
    }
    return value;
  }
  function systemError(status, syscall) {
    const info = native.error(status);
    const e = new Error(`${syscall} ${info.code}: ${info.message}`);
    e.code = info.code;
    e.errno = status;
    e.syscall = syscall;
    return e;
  }
  function signal(value = "SIGTERM") {
    if (typeof value === "string") {
      const name = value.toUpperCase();
      if (Object.hasOwn(host.signals, name)) {
        return host.signals[name];
      }
    } else if (typeof value === "number" && Number.isInteger(value) && value >= 0 && value <= 2147483647) {
      return value;
    }
    fail(TypeError, "ERR_UNKNOWN_SIGNAL", "Unknown signal");
  }
  function signalName(value) {
    for (const [name, number] of Object.entries(host.signals)) {
      if (number === value) {
        return name;
      }
    }
    return String(value);
  }
  function pipeDestroy(stream, error, callback) {
    const state = pipes.get(stream);
    state.error = error;
    if (state.closed) {
      callback(error);
      return;
    }
    state.closeCallback = callback;
    native.closePipe(state.child.id, state.index);
  }
  function pipeRef(stream, value) {
    const s = pipes.get(stream);
    if (!s.closed) {
      native.ref(s.child.id, s.index, value);
    }
    return stream;
  }
  class Input extends Writable {
    constructor(child) {
      super({ autoDestroy: true });
      pipes.set(this, { child, index: 0, closed: false, closeCallback: null, error: null });
    }
    _write(chunk, encoding, callback) {
      const s = pipes.get(this);
      const status = native.write(
        s.child.id,
        chunk,
        (status2) => callback(status2 ? systemError(status2, "write") : void 0)
      );
      if (status) {
        callback(systemError(status, "write"));
      }
    }
    _final(callback) {
      const s = pipes.get(this);
      if (s.child.failed || s.closed) {
        callback();
        return;
      }
      const status = native.shutdown(s.child.id, () => callback());
      if (status) {
        const error = systemError(status, "shutdown");
        callback(error.code === "ENOTCONN" ? void 0 : error);
      }
    }
    _destroy(error, callback) {
      pipeDestroy(this, error, callback);
    }
    ref() {
      return pipeRef(this, true);
    }
    unref() {
      return pipeRef(this, false);
    }
  }
  class Output extends Readable {
    constructor(child, index) {
      super({ autoDestroy: true });
      pipes.set(this, {
        child,
        index,
        closed: false,
        ended: false,
        closeCallback: null,
        error: null
      });
    }
    _read() {
      const s = pipes.get(this);
      if (s.closed || s.ended || s.child.failed) {
        return;
      }
      const status = native.read(s.child.id, s.index, true);
      if (status) {
        this.destroy(systemError(status, "read"));
      }
    }
    _destroy(error, callback) {
      pipeDestroy(this, error, callback);
    }
    ref() {
      return pipeRef(this, true);
    }
    unref() {
      return pipeRef(this, false);
    }
  }
  class ChildProcess extends EventEmitter {
    constructor(key) {
      super();
      if (key !== token) {
        unsupported("Direct ChildProcess construction");
      }
      this.pid = void 0;
      this.spawnfile = void 0;
      this.spawnargs = [];
      this.exitCode = null;
      this.signalCode = null;
      this.killed = false;
      this.connected = false;
      this.stdin = null;
      this.stdout = null;
      this.stderr = null;
      this.stdio = [];
    }
    kill(value) {
      const number = signal(value);
      const c = children.get(this);
      const status = native.kill(c.id, number);
      if (!status) {
        this.killed = true;
        return true;
      }
      const e = systemError(status, "kill");
      if (e.code !== "ESRCH") {
        this.emit("error", e);
      }
      return false;
    }
    ref() {
      native.ref(children.get(this).id, -1, true);
    }
    unref() {
      native.ref(children.get(this).id, -1, false);
    }
    send() {
      unsupported("ChildProcess IPC");
    }
    disconnect() {
      unsupported("ChildProcess IPC");
    }
    [Symbol.dispose]() {
      if (!this.killed) {
        this.kill();
      }
    }
  }
  function modes(value) {
    if (value === void 0) {
      value = "pipe";
    }
    if (typeof value === "string") {
      if (!["pipe", "ignore", "inherit"].includes(value)) {
        unsupported(`stdio ${value}`);
      }
      value = [value, value, value];
    }
    if (!Array.isArray(value)) {
      fail(TypeError, "ERR_INVALID_ARG_VALUE", "stdio must be an array or string");
    }
    if (value.length > 3) {
      unsupported("Additional stdio descriptors or IPC");
    }
    return Array.from({ length: 3 }, (_, index) => {
      const mode = value[index] ?? "pipe";
      if (mode === "pipe") {
        return -1;
      }
      if (mode === "ignore") {
        return -2;
      }
      if (mode === "inherit") {
        return index;
      }
      if (typeof mode === "number" && Number.isInteger(mode) && mode >= 0 && mode <= 2147483647) {
        return mode;
      }
      unsupported("stdio streams, IPC and nonstandard descriptors");
    });
  }
  function options(file, args, opts) {
    text(file, "file");
    if (!file) {
      fail(TypeError, "ERR_INVALID_ARG_VALUE", "file must not be empty");
    }
    if (args !== void 0 && !Array.isArray(args)) {
      opts = args;
      args = [];
    }
    args = args ?? [];
    if (opts === void 0 || opts === null) {
      opts = {};
    }
    if (typeof opts !== "object" || Array.isArray(opts)) {
      fail(TypeError, "ERR_INVALID_ARG_TYPE", "options must be an object");
    }
    opts = { ...opts };
    for (const key of ["signal", "uid", "gid", "serialization"]) {
      if (opts[key] !== void 0) {
        unsupported(`spawn option ${key}`);
      }
    }
    if (opts.shell) {
      unsupported("Shell execution");
    }
    for (const key of ["detached", "windowsHide", "windowsVerbatimArguments"]) {
      if (opts[key] !== void 0 && typeof opts[key] !== "boolean") {
        fail(TypeError, "ERR_INVALID_ARG_TYPE", `${key} must be a boolean`);
      }
    }
    let cwd = opts.cwd ?? "";
    if (cwd instanceof host.URL) {
      cwd = host.fileURLToPath(cwd);
    }
    text(cwd, "cwd");
    const argv0 = opts.argv0 === void 0 ? file : text(opts.argv0, "argv0");
    const publicArgs = [argv0, ...args];
    const normalized = publicArgs.map(
      (value) => text(typeof value === "symbol" ? "" : String(value), "args")
    );
    const env = opts.env ?? host.process.env;
    if (typeof env !== "object") {
      fail(TypeError, "ERR_INVALID_ARG_TYPE", "env must be an object");
    }
    const entries = [];
    const seen = /* @__PURE__ */ new Set();
    const keys = [];
    for (const key in env) {
      keys.push(key);
    }
    if (host.platform === "win32") {
      keys.sort();
    }
    for (const key of keys) {
      const name = host.platform === "win32" ? key.toLowerCase() : key;
      if (seen.has(name)) {
        continue;
      }
      seen.add(name);
      const value = env[key];
      if (value === void 0) {
        continue;
      }
      text(key, "env key");
      const encoded = text(`${value}`, "env value");
      entries.push(`${key}=${encoded}`);
    }
    const timeout = opts.timeout ?? 0;
    if (typeof timeout !== "number" || !Number.isInteger(timeout) || timeout < 0) {
      fail(RangeError, "ERR_OUT_OF_RANGE", "Invalid timeout");
    }
    if (timeout > 2147483647) {
      unsupported("Spawn timeouts above the timer limit");
    }
    const killSignal = signal(opts.killSignal);
    return {
      file,
      args: normalized,
      publicArgs,
      env: entries,
      cwd,
      stdio: modes(opts.stdio),
      detached: !!opts.detached,
      windowsHide: !!opts.windowsHide,
      windowsVerbatimArguments: !!opts.windowsVerbatimArguments,
      timeout,
      killSignal
    };
  }
  function spawn(file, args, opts) {
    const spec = options(file, args, opts);
    const child = new ChildProcess(token);
    const c = {
      id: null,
      child,
      processClosed: false,
      settled: false,
      closed: false,
      streams: [],
      timer: null
    };
    children.set(child, c);
    const maybeClose = () => {
      if (c.processClosed && c.settled && !c.closed && c.streams.every((stream) => stream === null || stream.closed)) {
        c.closed = true;
        if (c.timer) {
          clearTimeout(c.timer);
        }
        child.emit("close", child.exitCode, child.signalCode);
      }
    };
    const result = native.spawn(spec, (kind, index, value, termination) => {
      if (kind === "exit") {
        c.settled = true;
        if (c.timer) {
          clearTimeout(c.timer);
        }
        child.exitCode = termination ? null : value;
        child.signalCode = termination ? signalName(termination) : null;
        child.emit("exit", child.exitCode, child.signalCode);
        tick(() => {
          for (const stream of [child.stdout, child.stderr]) {
            if (stream && stream.readableFlowing === null) {
              stream.resume();
            }
          }
        });
      } else if (kind === "processClose") {
        c.processClosed = true;
        maybeClose();
      } else {
        const stream = c.streams[index];
        if (!stream) {
          return;
        }
        const s = pipes.get(stream);
        if (kind === "data") {
          if (!stream.destroyed && !stream.push(Buffer.from(value.buffer, value.byteOffset, value.byteLength))) {
            native.read(c.id, index, false);
          }
        } else if (kind === "end") {
          s.ended = true;
          stream.push(null);
        } else if (kind === "pipeError") {
          stream.destroy(systemError(value, "read"));
        } else if (kind === "pipeClose") {
          s.closed = true;
          if (s.closeCallback) {
            const callback = s.closeCallback;
            s.closeCallback = null;
            callback(s.error);
          } else if (!stream.destroyed) {
            stream.destroy();
          }
        }
      }
    });
    c.id = result.id;
    child.pid = result.pid;
    child.spawnfile = file;
    child.spawnargs = spec.publicArgs;
    c.failed = !!result.status;
    c.processClosed = !(result.handles & 8);
    c.streams = spec.stdio.map(
      (mode, index) => mode === -1 ? index === 0 ? new Input(c) : new Output(c, index) : null
    );
    child.stdio = c.streams;
    [child.stdin, child.stdout, child.stderr] = c.streams;
    for (const stream of c.streams) {
      if (stream) {
        stream.once("close", maybeClose);
      }
    }
    if (result.status) {
      child.exitCode = result.status;
      c.settled = true;
      tick(() => {
        const e = systemError(result.status, `spawn ${file}`);
        e.path = file;
        e.spawnargs = spec.publicArgs.slice(1);
        child.emit("error", e);
        for (let i = 0; i < 3; i++) {
          if (c.streams[i] && !(result.handles & 1 << i)) {
            pipes.get(c.streams[i]).closed = true;
            c.streams[i].destroy();
          }
        }
        maybeClose();
      });
    } else {
      tick(() => child.emit("spawn"));
      for (const stream of [child.stdout, child.stderr]) {
        if (stream) {
          stream.read(0);
        }
      }
      if (spec.timeout) {
        c.timer = setTimeout(() => {
          if (!c.settled) {
            child.kill(spec.killSignal);
          }
        }, spec.timeout).unref();
      }
    }
    return child;
  }
  function execFileImpl(file, args, opts, callback, displayCommand) {
    if (Array.isArray(args)) {
      args = args.slice();
    } else if (args !== null && typeof args === "object") {
      callback = opts;
      opts = args;
      args = [];
    } else if (typeof args === "function") {
      callback = args;
      opts = void 0;
      args = [];
    }
    args = args ?? [];
    if (typeof opts === "function") {
      callback = opts;
      opts = void 0;
    } else if (opts != null && (typeof opts !== "object" || Array.isArray(opts))) {
      fail(TypeError, "ERR_INVALID_ARG_TYPE", "options must be an object");
    }
    if (callback != null && typeof callback !== "function") {
      fail(TypeError, "ERR_INVALID_ARG_TYPE", "callback must be a function");
    }
    const config = { encoding: "utf8", maxBuffer: 1024 * 1024, timeout: 0, ...opts };
    if (config.timeout != null && (typeof config.timeout !== "number" || !Number.isInteger(config.timeout) || config.timeout < 0)) {
      fail(RangeError, "ERR_OUT_OF_RANGE", "Invalid timeout");
    }
    if (config.timeout > 2147483647) {
      unsupported("ExecFile timeouts above the timer limit");
    }
    if (config.maxBuffer != null && (typeof config.maxBuffer !== "number" || !(config.maxBuffer >= 0))) {
      fail(RangeError, "ERR_OUT_OF_RANGE", "Invalid maxBuffer");
    }
    if (config.argv0 != null) {
      text(config.argv0, "argv0");
    }
    const killSignal = config.killSignal ?? "SIGTERM";
    signal(killSignal);
    const child = spawn(file, args, {
      cwd: config.cwd,
      env: config.env,
      uid: config.uid,
      gid: config.gid,
      signal: config.signal,
      shell: config.shell,
      windowsHide: !!config.windowsHide,
      windowsVerbatimArguments: !!config.windowsVerbatimArguments
    });
    const output = [[], []];
    const streams = [child.stdout, child.stderr];
    const counts = [0, 0];
    let failure = null;
    let settled = false;
    let timer = null;
    let terminated = false;
    function finish(code, termination) {
      if (settled) {
        return;
      }
      settled = true;
      if (timer) {
        clearTimeout(timer);
      }
      if (!callback) {
        return;
      }
      const values = output.map(
        (chunks, index) => streams[index]?.readableEncoding ? chunks.join("") : Buffer.concat(chunks)
      );
      if (!failure && code === 0 && termination === null) {
        callback(null, ...values);
        return;
      }
      const command = displayCommand ?? (args.length ? file + " " + args.join(" ") : file);
      if (!failure) {
        failure = new Error(`Command failed: ${command}
${values[1]}`);
        failure.code = code;
        failure.killed = child.killed || terminated;
        failure.signal = termination;
      }
      failure.cmd = command;
      callback(failure, ...values);
    }
    function stop() {
      for (const stream of streams) {
        stream?.destroy();
      }
      terminated = true;
      try {
        child.kill(killSignal);
      } catch (error) {
        failure = error;
        finish();
      }
    }
    const encoding = Buffer.isEncoding(config.encoding) ? config.encoding : null;
    for (const [index, stream] of streams.entries()) {
      if (stream) {
        if (encoding) {
          stream.setEncoding(encoding);
        }
        stream.on("data", (chunk) => {
          if (failure) {
            return;
          }
          if (config.maxBuffer === Infinity) {
            output[index].push(chunk);
            return;
          }
          const length = stream.readableEncoding ? Buffer.byteLength(chunk, stream.readableEncoding) : chunk.length;
          const previous = counts[index];
          counts[index] += length;
          if (counts[index] > config.maxBuffer) {
            output[index].push(chunk.slice(0, config.maxBuffer - previous));
            failure = new RangeError(
              `${index === 0 ? "stdout" : "stderr"} maxBuffer length exceeded`
            );
            failure.code = "ERR_CHILD_PROCESS_STDIO_MAXBUFFER";
            stop();
          } else {
            output[index].push(chunk);
          }
        });
      }
    }
    child.on("error", (error) => {
      failure = error;
      for (const stream of streams) {
        stream?.destroy();
      }
      finish();
    });
    child.on("close", finish);
    if (config.timeout > 0) {
      timer = setTimeout(stop, config.timeout);
    }
    return child;
  }
  function execFile(file, args, opts, callback) {
    return execFileImpl(file, args, opts, callback);
  }
  function exec(command, opts, callback) {
    text(command, "command");
    if (!command) {
      fail(TypeError, "ERR_INVALID_ARG_VALUE", "command must not be empty");
    }
    if (typeof opts === "function") {
      callback = opts;
      opts = void 0;
    }
    if (opts != null && (typeof opts !== "object" || Array.isArray(opts))) {
      fail(TypeError, "ERR_INVALID_ARG_TYPE", "options must be an object");
    }
    const config = { ...opts };
    const windows = host.platform === "win32";
    const shell = typeof config.shell === "string" ? config.shell : windows ? host.process.env.comspec || "cmd.exe" : "/bin/sh";
    const cmd = windows && /(?:^|[\\/])cmd(?:\.exe)?$/i.test(shell);
    const args = shell === "" ? [] : cmd ? ["/d", "/s", "/c", `"${command}"`] : ["-c", command];
    config.shell = false;
    if (cmd) {
      config.windowsVerbatimArguments = true;
    }
    return execFileImpl(shell || command, args, config, callback, command);
  }
  function attachPromisify(operation) {
    Object.defineProperty(operation, Symbol.for("nodejs.util.promisify.custom"), {
      value: function(...args) {
        let child;
        const promise = new Promise((resolve, reject) => {
          child = operation(...args, (error, stdout, stderr) => {
            if (error) {
              error.stdout = stdout;
              error.stderr = stderr;
              reject(error);
            } else {
              resolve({ stdout, stderr });
            }
          });
        });
        promise.child = child;
        return promise;
      }
    });
  }
  attachPromisify(execFile);
  attachPromisify(exec);
  return { spawn, execFile, exec, ChildProcess };
}
return createChildProcessModule;
})()
