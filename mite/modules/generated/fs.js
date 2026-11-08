/* Generated from mite/modules/filesystem/fs.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/promisify-results.mjs
function namedPromisify(original, names, state) {
  Object.defineProperty(original, state.argumentNames, { value: names });
}

// mite/modules/filesystem/read-stream.mjs
function createReadStream(handle, value, host, closeHandle) {
  const unsupported = (name) => {
    throw Object.assign(new Error(`FileHandle.createReadStream ${name} is not implemented`), {
      code: "ERR_MITE_UNSUPPORTED_API"
    });
  };
  if (typeof value === "string") {
    value = { encoding: value };
  }
  if (value === void 0) {
    value = {};
  }
  if (value === null || typeof value !== "object") {
    throw Object.assign(new TypeError("options must be an object"), {
      code: "ERR_INVALID_ARG_TYPE"
    });
  }
  for (const name of Object.keys(value)) {
    if (!["encoding", "start", "end", "highWaterMark", "autoClose", "emitClose", "signal"].includes(
      name
    )) {
      unsupported(`option ${name}`);
    }
  }
  const integer = (v, fallback, min, max, name) => {
    if (v === void 0) {
      return fallback;
    }
    if (typeof v !== "number" || !Number.isSafeInteger(v) || v < min || v > max) {
      throw Object.assign(new RangeError(`${name} is out of range`), { code: "ERR_OUT_OF_RANGE" });
    }
    return v;
  };
  for (const key of ["autoClose", "emitClose"]) {
    if (value[key] !== void 0 && typeof value[key] !== "boolean") {
      throw Object.assign(new TypeError(`${key} must be a boolean`), {
        code: "ERR_INVALID_ARG_TYPE"
      });
    }
  }
  let position = integer(value.start, void 0, 0, Number.MAX_SAFE_INTEGER, "start");
  const end = value.end === Infinity ? Infinity : integer(value.end, Infinity, position ?? 0, Number.MAX_SAFE_INTEGER, "end");
  const size = integer(value.highWaterMark, 65536, 1, 1048576, "highWaterMark");
  const autoClose = value.autoClose !== false;
  let reading;
  let count = 0;
  let stream;
  const onClose = () => stream?.destroy();
  stream = new host.stream.Readable({
    highWaterMark: size,
    autoDestroy: autoClose,
    encoding: value.encoding,
    emitClose: value.emitClose,
    signal: value.signal,
    read() {
      if (reading) {
        return;
      }
      const remaining = end === Infinity ? size : Math.min(size, end - (position ?? count) + 1);
      if (remaining <= 0) {
        this.push(null);
        return;
      }
      const buffer = host.Buffer.alloc(remaining);
      reading = handle.read(buffer, 0, remaining, position ?? null);
      reading.then(
        ({ bytesRead }) => {
          reading = void 0;
          if (this.destroyed) {
            return;
          }
          if (position !== void 0) {
            position += bytesRead;
          }
          count += bytesRead;
          if (bytesRead === 0) {
            this.push(null);
          } else {
            this.push(buffer.subarray(0, bytesRead));
          }
        },
        (error) => {
          reading = void 0;
          this.destroy(error);
        }
      );
    },
    destroy(error, callback) {
      handle.removeListener("close", onClose);
      const finish = () => closeHandle();
      Promise.resolve(reading).catch(() => {
      }).then(finish).then(
        () => callback(error),
        (closeError) => callback(error ?? closeError)
      );
    }
  });
  if (!stream.destroyed) {
    handle.on("close", onClose);
  }
  Object.defineProperties(stream, { bytesRead: { get: () => count }, path: { value: void 0 } });
  return stream;
}

// mite/modules/filesystem/write-stream.mjs
function createWriteStreams(fs, host, validate, acquireHandle) {
  const fail = (code, message) => {
    throw Object.assign(new TypeError(message), { code });
  };
  class WriteStream extends host.stream.Writable {
    constructor(filename, options = {}) {
      if (typeof options === "string") {
        options = { encoding: options };
      }
      if (!options || typeof options !== "object") {
        fail("ERR_INVALID_ARG_TYPE", "options must be an object or encoding");
      }
      for (const key of Object.keys(options)) {
        if (![
          "encoding",
          "flags",
          "mode",
          "fd",
          "start",
          "highWaterMark",
          "autoClose",
          "emitClose",
          "signal"
        ].includes(key)) {
          fail("ERR_MITE_UNSUPPORTED_API", `WriteStream option ${key} is not implemented`);
        }
      }
      for (const key of ["autoClose", "emitClose"]) {
        if (options[key] !== void 0 && typeof options[key] !== "boolean") {
          fail("ERR_INVALID_ARG_TYPE", `${key} must be a boolean`);
        }
      }
      if (options.encoding !== void 0 && (typeof options.encoding !== "string" || !host.Buffer.isEncoding(options.encoding))) {
        fail("ERR_INVALID_ARG_VALUE", "Unknown file stream encoding");
      }
      let position = options.start;
      if (position !== void 0 && (!Number.isSafeInteger(position) || position < 0)) {
        fail("ERR_OUT_OF_RANGE", "start must be a nonnegative safe integer");
      }
      const supplied = options.fd !== void 0 && options.fd !== null;
      const pathname = supplied ? void 0 : validate.path(filename);
      const flags = validate.flags(options.flags, "w");
      const mode = validate.mode(options.mode, 438);
      const handle = supplied && typeof options.fd === "object" ? options.fd : void 0;
      if (supplied && !handle && (!Number.isInteger(options.fd) || options.fd < 0)) {
        fail("ERR_INVALID_ARG_TYPE", "fd must be a nonnegative descriptor or FileHandle");
      }
      const lease = handle ? acquireHandle(handle) : void 0;
      let fd = supplied ? handle ? handle.fd : options.fd : null;
      let bytes = 0;
      let writing;
      let stream;
      const onHandleClose = () => stream?.close();
      const preAborted = options.signal?.aborted === true;
      try {
        super({
          highWaterMark: options.highWaterMark ?? 16384,
          defaultEncoding: options.encoding ?? "utf8",
          autoDestroy: options.autoClose !== false,
          emitClose: options.emitClose,
          signal: options.signal,
          construct(callback) {
            if (supplied || preAborted) {
              callback();
              return;
            }
            fs.open(pathname, flags, mode, (error, opened) => {
              if (error) {
                callback(error);
                return;
              }
              fd = opened;
              this.emit("open", fd);
              this.emit("ready");
              callback();
            });
          },
          write(chunk, encoding, callback) {
            writing = (async () => {
              let offset = 0;
              let retries = 0;
              while (offset < chunk.length) {
                const length = Math.min(65536, chunk.length - offset);
                let count;
                try {
                  count = handle ? (await handle.write(chunk, offset, length, position ?? null)).bytesWritten : await new Promise((resolve, reject) => {
                    fs.write(
                      fd,
                      chunk,
                      offset,
                      length,
                      position ?? null,
                      (error, written) => error ? reject(error) : resolve(written)
                    );
                  });
                } catch (error) {
                  if (error.code !== "EAGAIN") {
                    throw error;
                  }
                  count = 0;
                }
                if (count === 0) {
                  if (++retries > 5) {
                    throw Object.assign(new Error("File write made no progress"), {
                      code: "ERR_SYSTEM_ERROR"
                    });
                  }
                  await new Promise((resolve) => setTimeout(resolve, 0));
                  continue;
                }
                retries = 0;
                bytes += count;
                offset += count;
                if (position !== void 0) {
                  position += count;
                }
              }
            })();
            writing.then(() => callback(), callback);
          },
          destroy(error, callback) {
            handle?.removeListener("close", onHandleClose);
            Promise.resolve(writing).catch(() => {
            }).then(async () => {
              lease?.release();
              if (fd !== null) {
                if (lease) {
                  await lease.close();
                } else {
                  await new Promise((resolve, reject) => {
                    fs.close(fd, (failure) => failure ? reject(failure) : resolve());
                  });
                }
              }
            }).then(
              () => {
                fd = null;
                callback(error);
              },
              (failure) => {
                fd = null;
                callback(error ?? failure);
              }
            );
          }
        });
      } catch (error) {
        lease?.release();
        throw error;
      }
      stream = this;
      Object.defineProperties(this, {
        fd: { get: () => fd },
        path: { value: pathname },
        pending: { get: () => fd === null },
        bytesWritten: { get: () => bytes }
      });
      if (handle && !this.destroyed) {
        handle.on("close", onHandleClose);
      }
    }
    close(callback) {
      if (callback) {
        if (this.closed) {
          queueMicrotask(callback);
        } else {
          this.once("close", callback);
        }
      }
      if (this.writableFinished) {
        this.destroy();
      } else {
        this.once("finish", () => this.destroy());
        this.end();
      }
    }
  }
  return {
    WriteStream,
    createWriteStream: (filename, options) => new WriteStream(filename, options)
  };
}

// mite/modules/filesystem/stat-watcher.mjs
function createStatWatchers(host, helpers) {
  const { native, EventEmitter } = host;
  const { path, nativePath, stats } = helpers;
  const watchers = /* @__PURE__ */ new Map();
  const states = /* @__PURE__ */ new WeakMap();
  const tick = host.process.nextTick;
  const token = {};
  function invalid(code, message, Type = TypeError) {
    return Object.assign(new Type(message), { code });
  }
  function state(value) {
    const result = states.get(value);
    if (!result) {
      throw invalid("ERR_INVALID_ARG_TYPE", "Illegal StatWatcher receiver");
    }
    return result;
  }
  function filename(value) {
    return host.path.resolve(path(value));
  }
  class StatWatcher extends EventEmitter {
    constructor(key, name, bigint) {
      super();
      if (key !== token) {
        throw invalid("ERR_ILLEGAL_CONSTRUCTOR", "StatWatcher cannot be constructed directly");
      }
      states.set(this, {
        name,
        bigint,
        id: null,
        stopped: false,
        admitted: false,
        early: [],
        refs: 1,
        maximum: 1
      });
    }
    ref() {
      const current = state(this);
      if (current.refs === current.maximum) {
        return this;
      }
      if (current.id !== null && current.refs++ === 0) {
        native.statWatchRef(current.id, true);
      }
      return this;
    }
    unref() {
      const current = state(this);
      if (current.refs === 0) {
        return this;
      }
      if (current.id !== null && --current.refs === 0) {
        native.statWatchRef(current.id, false);
      }
      return this;
    }
    stop() {
      const current = state(this);
      if (current.stopped) {
        return;
      }
      current.stopped = true;
      current.early.length = 0;
      const id = current.id;
      current.id = null;
      if (id !== null) {
        native.statWatchStop(id);
      }
      tick(() => this.emit("stop"));
    }
  }
  function notify(watcher, status, currentRaw, previousRaw) {
    const current = state(watcher);
    if (current.stopped) {
      return;
    }
    const value = [stats(currentRaw, current.bigint), stats(previousRaw, current.bigint)];
    if (!current.admitted) {
      if (current.early.length >= 64) {
        watcher.stop();
        throw invalid("ERR_MITE_STAT_WATCH_LIMIT", "Stat watcher admission queue exhausted", Error);
      }
      current.early.push(value);
      return;
    }
    watcher.emit("change", ...value);
  }
  function schedule(watcher) {
    tick(() => {
      const current = state(watcher);
      while (!current.stopped && current.early.length) {
        watcher.emit("change", ...current.early.shift());
      }
    });
  }
  function watchFile(value, options, listener) {
    const name = filename(value);
    if (options === null || typeof options !== "object") {
      listener = options;
      options = null;
    }
    const selected = { interval: 5007, persistent: true, ...options };
    if (typeof listener !== "function") {
      throw invalid("ERR_INVALID_ARG_TYPE", "watchFile listener must be a function");
    }
    let watcher = watchers.get(name);
    if (watcher) {
      const current2 = state(watcher);
      current2.refs++;
      current2.maximum++;
      watcher.on("change", listener);
      return watcher;
    }
    const interval = selected.interval;
    if (typeof interval !== "number") {
      throw invalid("ERR_INVALID_ARG_TYPE", "watchFile interval must be a number");
    }
    if (!Number.isInteger(interval) || interval < 0 || interval > 4294967295) {
      throw invalid("ERR_OUT_OF_RANGE", "watchFile interval must be a uint32", RangeError);
    }
    watcher = new StatWatcher(token, name, selected.bigint === true);
    const current = state(watcher);
    watchers.set(name, watcher);
    try {
      const result = native.statWatchStart(
        nativePath(name),
        interval,
        !!selected.persistent,
        (status, currentRaw, previousRaw) => notify(watcher, status, currentRaw, previousRaw)
      );
      if (result.status) {
        throw native.statWatchError(result.status, name);
      }
      if (current.stopped || watchers.get(name) !== watcher) {
        native.statWatchStop(result.id);
        throw invalid("ERR_MITE_STAT_WATCH_CLOSED", "Stat watcher stopped during admission", Error);
      }
      current.id = result.id;
      if (!selected.persistent) {
        current.refs = 0;
      }
      watcher.on("change", listener);
      if (current.stopped || watchers.get(name) !== watcher) {
        throw invalid(
          "ERR_MITE_STAT_WATCH_CLOSED",
          "Stat watcher stopped while installing listener",
          Error
        );
      }
      current.admitted = true;
      if (current.early.length) {
        schedule(watcher);
      }
      return watcher;
    } catch (failure) {
      if (watchers.get(name) === watcher) {
        watchers.delete(name);
      }
      watcher.removeAllListeners("change");
      watcher.stop();
      throw failure;
    }
  }
  function unwatchFile(value, listener) {
    const name = filename(value);
    const watcher = watchers.get(name);
    if (!watcher) {
      return;
    }
    const current = state(watcher);
    if (typeof listener === "function") {
      const before = watcher.listenerCount("change");
      watcher.removeListener("change", listener);
      if (watcher.listenerCount("change") < before) {
        current.maximum--;
        watcher.unref();
      }
    } else {
      watcher.removeAllListeners("change");
      current.refs = 0;
      current.maximum = 0;
      if (current.id !== null) {
        native.statWatchRef(current.id, false);
      }
    }
    if (watcher.listenerCount("change") === 0) {
      watcher.stop();
      if (watchers.get(name) === watcher) {
        watchers.delete(name);
      }
    }
  }
  return { watchFile, unwatchFile };
}

// mite/modules/filesystem/event-watcher.mjs
function createEventWatchers(host, helpers) {
  const { native, Buffer, EventEmitter, abort } = host;
  const { path, nativePath } = helpers;
  const tick = host.process.nextTick;
  const states = /* @__PURE__ */ new WeakMap();
  const token = {};
  function error(code, message, Type = TypeError) {
    return Object.assign(new Type(message), { code });
  }
  function state(value) {
    const current = states.get(value);
    if (!current) {
      throw error("ERR_INVALID_ARG_TYPE", "Illegal FSWatcher receiver");
    }
    return current;
  }
  function dispose(watcher, notify) {
    const current = state(watcher);
    if (current.closed) {
      return;
    }
    current.closed = true;
    current.early.length = 0;
    current.bytes = 0;
    const unsubscribe = current.unsubscribe;
    current.unsubscribe = null;
    unsubscribe?.();
    const id = current.id;
    current.id = null;
    if (id !== null) {
      native.fsWatchStop(id);
    }
    if (notify) {
      tick(() => watcher.emit("close"));
    }
  }
  class FSWatcher extends EventEmitter {
    constructor(key, name, encoding) {
      super();
      if (key !== token) {
        throw error("ERR_ILLEGAL_CONSTRUCTOR", "FSWatcher cannot be constructed directly");
      }
      states.set(this, {
        name,
        encoding,
        id: null,
        closed: false,
        admitted: false,
        early: [],
        bytes: 0,
        unsubscribe: null
      });
    }
    close() {
      dispose(this, true);
    }
    ref() {
      const current = state(this);
      if (current.id !== null) {
        native.fsWatchRef(current.id, true);
      }
      return this;
    }
    unref() {
      const current = state(this);
      if (current.id !== null) {
        native.fsWatchRef(current.id, false);
      }
      return this;
    }
    [Symbol.dispose]() {
      this.close();
    }
  }
  function deliver(watcher, record) {
    const current = state(watcher);
    if (current.closed) {
      return;
    }
    if (record.status < 0) {
      const failure = native.fsWatchError(record.status, current.name);
      dispose(watcher, false);
      watcher.emit("error", failure);
    } else {
      watcher.emit("change", record.mask & 1 ? "rename" : "change", record.filename);
    }
  }
  function receive(watcher, status, mask, value) {
    const current = state(watcher);
    if (current.closed) {
      return;
    }
    if (!Number.isInteger(status) || status < -2147483648 || status > 2147483647 || !Number.isInteger(mask) || mask < 0 || mask > 3) {
      dispose(watcher, false);
      throw error("ERR_MITE_FS_WATCH_DATA", "Invalid native filesystem event status", Error);
    }
    let filename = null;
    if (value !== null) {
      if (!(value instanceof Uint8Array) || value.byteLength > 1048576) {
        dispose(watcher, false);
        throw error("ERR_MITE_FS_WATCH_DATA", "Invalid native event filename", Error);
      }
      const bytes = Buffer.from(value);
      filename = current.encoding === "buffer" ? bytes : bytes.toString(current.encoding);
    }
    const record = { status, mask, filename };
    if (!current.admitted) {
      const bytes = value?.byteLength ?? 0;
      if (current.early.length >= 64 || current.bytes + bytes > 4194304) {
        dispose(watcher, false);
        throw error("ERR_MITE_FS_WATCH_LIMIT", "Filesystem event admission queue exhausted", Error);
      }
      current.bytes += bytes;
      current.early.push(record);
    } else {
      deliver(watcher, record);
    }
  }
  function schedule(watcher) {
    tick(() => {
      const current = state(watcher);
      while (!current.closed && current.early.length) {
        deliver(watcher, current.early.shift());
      }
      current.bytes = 0;
    });
  }
  function attachAbort(watcher, signal) {
    const current = state(watcher);
    let attaching = true;
    const unsubscribe = abort.subscribe(signal, () => {
      if (attaching) {
        tick(() => watcher.close());
      } else {
        watcher.close();
      }
    });
    attaching = false;
    if (current.closed) {
      unsubscribe();
    } else {
      current.unsubscribe = unsubscribe;
    }
  }
  function watch(filename, options, listener) {
    if (typeof options === "function") {
      listener = options;
      options = void 0;
    }
    if (typeof options === "string") {
      options = { encoding: options };
    } else if (options === void 0 || options === null) {
      options = {};
    } else if (typeof options !== "object") {
      throw error("ERR_INVALID_ARG_TYPE", "Filesystem watch options must be an object or string");
    }
    const selected = { ...options };
    const persistent = selected.persistent === void 0 ? true : !!selected.persistent;
    const recursive = selected.recursive === void 0 ? false : !!selected.recursive;
    const encoding = selected.encoding || "utf8";
    const signal = selected.signal;
    if (encoding !== "buffer" && (typeof encoding !== "string" || !Buffer.isEncoding(encoding))) {
      throw error("ERR_INVALID_ARG_VALUE", "Invalid filesystem watch encoding");
    }
    if (listener && typeof listener !== "function") {
      throw error("ERR_INVALID_ARG_TYPE", "Filesystem watch listener must be a function");
    }
    if (signal !== void 0 && (!abort || !abort.isSignal(signal))) {
      throw error("ERR_INVALID_ARG_TYPE", "Filesystem watch signal must be an AbortSignal");
    }
    if (selected.ignore !== void 0 && selected.ignore !== null) {
      throw error(
        "ERR_MITE_UNSUPPORTED_API",
        "Filesystem watch ignore filters are not implemented",
        Error
      );
    }
    if (recursive && host.platform !== "darwin" && host.platform !== "win32") {
      throw error(
        "ERR_MITE_UNSUPPORTED_API",
        "Recursive filesystem events are unavailable on this platform",
        Error
      );
    }
    const name = path(filename);
    const snapshot = typeof name === "string" ? name : Buffer.from(name);
    const watcher = new FSWatcher(token, snapshot, encoding);
    const current = state(watcher);
    try {
      const result = native.fsWatchStart(
        nativePath(snapshot),
        persistent,
        recursive,
        (status, mask, value) => receive(watcher, status, mask, value)
      );
      if (result.status) {
        throw native.fsWatchError(result.status, snapshot);
      }
      if (current.closed) {
        native.fsWatchStop(result.id);
        throw error(
          "ERR_MITE_FS_WATCH_CLOSED",
          "Filesystem watcher stopped during admission",
          Error
        );
      }
      current.id = result.id;
      if (listener) {
        watcher.on("change", listener);
      }
      if (current.closed) {
        throw error(
          "ERR_MITE_FS_WATCH_CLOSED",
          "Filesystem watcher closed while adding listener",
          Error
        );
      }
      if (signal !== void 0) {
        attachAbort(watcher, signal);
      }
      current.admitted = true;
      if (current.early.length) {
        schedule(watcher);
      }
      return watcher;
    } catch (failure) {
      dispose(watcher, false);
      throw failure;
    }
  }
  return { watch };
}

// mite/modules/filesystem/fs.mjs
function createFileSystemModule(host) {
  const native = host.native;
  const Buffer = host.Buffer;
  const constants = native.constants;
  const windows = host.platform === "win32";
  const toNamespacedPath = host.path.toNamespacedPath;
  function nativePath(value) {
    if (!windows || typeof value === "number") {
      return value;
    }
    if (value instanceof Uint8Array) {
      value = Buffer.from(value.buffer, value.byteOffset, value.byteLength).toString("utf8");
    }
    return toNamespacedPath(value);
  }
  function fail(code, message, Type = TypeError) {
    const error = new Type(message);
    error.code = code;
    throw error;
  }
  function unsupported(feature) {
    fail("ERR_MITE_UNSUPPORTED_API", `${feature} is not implemented`);
  }
  function path(value) {
    if (value instanceof host.URL) {
      value = host.fileURLToPath(value);
    }
    if (typeof value !== "string" && !(value instanceof Uint8Array)) {
      fail("ERR_INVALID_ARG_TYPE", "path must be a string or Buffer");
    }
    if (typeof value === "string" ? value.includes("\0") : value.includes(0)) {
      fail("ERR_INVALID_ARG_VALUE", "path must not contain null bytes");
    }
    return value;
  }
  function options(value) {
    if (value === void 0 || value === null) {
      return {};
    }
    if (typeof value === "string") {
      return { encoding: value };
    }
    if (typeof value !== "object") {
      fail("ERR_INVALID_ARG_TYPE", "options must be a string or object");
    }
    if (value.signal !== void 0) {
      unsupported("AbortSignal filesystem cancellation");
    }
    if (value.flush) {
      unsupported("Filesystem flush option");
    }
    return value;
  }
  function codec(value, buffers = false) {
    if (!value || buffers && value === "buffer") {
      return value;
    }
    if (typeof value !== "string") {
      fail("ERR_INVALID_ARG_VALUE", "Invalid encoding");
    }
    if (value && !Buffer.isEncoding(value)) {
      fail("ERR_INVALID_ARG_VALUE", `Invalid encoding: ${value}`);
    }
    return value;
  }
  function integer(value, minimum, maximum, name) {
    if (typeof value !== "number") {
      fail("ERR_INVALID_ARG_TYPE", `${name} must be a number`);
    }
    if (!Number.isInteger(value) || value < minimum || value > maximum) {
      fail("ERR_OUT_OF_RANGE", `${name} is out of range`, RangeError);
    }
    return value;
  }
  function mode(value, fallback) {
    if (value == null) {
      return fallback;
    }
    if (typeof value === "string") {
      if (!/^[0-7]+$/.test(value)) {
        fail("ERR_INVALID_ARG_VALUE", "Invalid file mode");
      }
      value = parseInt(value, 8);
    }
    return integer(value, 0, 4294967295, "mode");
  }
  function flags(value, fallback) {
    if (value == null) {
      value = fallback;
    }
    if (typeof value === "number") {
      return integer(value, -2147483648, 2147483647, "flags");
    }
    if (typeof value !== "string") {
      fail("ERR_INVALID_ARG_VALUE", "flags must be a string or number");
    }
    const c = constants;
    const table = {
      r: c.O_RDONLY,
      "r+": c.O_RDWR,
      w: c.O_WRONLY | c.O_CREAT | c.O_TRUNC,
      wx: c.O_WRONLY | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
      xw: c.O_WRONLY | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
      "w+": c.O_RDWR | c.O_CREAT | c.O_TRUNC,
      "wx+": c.O_RDWR | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
      "xw+": c.O_RDWR | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
      a: c.O_WRONLY | c.O_CREAT | c.O_APPEND,
      ax: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_EXCL,
      xa: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_EXCL,
      "a+": c.O_RDWR | c.O_CREAT | c.O_APPEND,
      "ax+": c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_EXCL,
      "xa+": c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_EXCL,
      rs: c.O_RDONLY | c.O_SYNC,
      sr: c.O_RDONLY | c.O_SYNC,
      "rs+": c.O_RDWR | c.O_SYNC,
      "sr+": c.O_RDWR | c.O_SYNC,
      as: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_SYNC,
      sa: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_SYNC,
      "as+": c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_SYNC,
      "sa+": c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_SYNC
    };
    if (!Object.hasOwn(table, value)) {
      fail("ERR_INVALID_ARG_VALUE", `Invalid file flag: ${value}`);
    }
    return table[value];
  }
  class StatsBase {
    isFile() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFREG;
    }
    isDirectory() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFDIR;
    }
    isSymbolicLink() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFLNK;
    }
    isBlockDevice() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFBLK;
    }
    isCharacterDevice() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFCHR;
    }
    isFIFO() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFIFO;
    }
    isSocket() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFSOCK;
    }
  }
  class Stats extends StatsBase {
  }
  class BigIntStats extends StatsBase {
  }
  function stats(raw, bigint) {
    const result = new (bigint ? BigIntStats : Stats)();
    for (const name of [
      "dev",
      "mode",
      "nlink",
      "uid",
      "gid",
      "rdev",
      "blksize",
      "ino",
      "size",
      "blocks"
    ]) {
      result[name] = bigint ? BigInt(raw[name]) : Number(raw[name]);
    }
    for (const name of ["atime", "mtime", "ctime", "birthtime"]) {
      const value = raw[name];
      const nanos = BigInt(value.seconds) * 1000000000n + BigInt(value.nanos);
      result[name + "Ms"] = bigint ? nanos / 1000000n : Number(value.seconds) * 1e3 + value.nanos / 1e6;
      if (bigint) {
        result[name + "Ns"] = nanos;
      }
      result[name] = new Date(Number(nanos / 1000000n));
    }
    return result;
  }
  const kType = Symbol("type");
  class Dirent {
    constructor(name, parentPath, type) {
      this.name = name;
      this.parentPath = parentPath;
      this[kType] = type;
    }
    isFile() {
      return this[kType] === 1;
    }
    isDirectory() {
      return this[kType] === 2;
    }
    isSymbolicLink() {
      return this[kType] === 3;
    }
    isFIFO() {
      return this[kType] === 4;
    }
    isSocket() {
      return this[kType] === 5;
    }
    isCharacterDevice() {
      return this[kType] === 6;
    }
    isBlockDevice() {
      return this[kType] === 7;
    }
  }
  function prepare(op, pathname, argument, data) {
    const descriptor = ["close", "fstat", "ftruncate", "fsync", "fdatasync"].includes(op);
    const spec = {
      op,
      path: descriptor || (op === "readFile" || op === "writeFile") && typeof pathname === "number" ? integer(pathname, 0, 2147483647, "fd") : path(pathname)
    };
    const parentPath = spec.path;
    spec.path = nativePath(spec.path);
    if (windows && op === "readdir" && /[\\/]$/.test(String(parentPath)) && !spec.path.endsWith("\\")) {
      spec.path += "\\";
    }
    let convert = () => void 0;
    let recover;
    if (op === "open") {
      spec.flags = flags(argument.flags, "r");
      spec.mode = mode(argument.mode, 438);
      convert = (value) => value;
    } else if (op === "ftruncate") {
      const length = argument === void 0 ? 0 : integer(argument, -Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER, "len");
      spec.position = String(Math.max(0, length));
    } else if (op === "readFile") {
      const opts = options(argument);
      const encoding = codec(opts.encoding);
      spec.flags = flags(opts.flag, "r");
      convert = (raw) => {
        const buffer = Buffer.from(raw.buffer, raw.byteOffset, raw.byteLength);
        return encoding ? buffer.toString(encoding) : buffer;
      };
    } else if (op === "realpath") {
      const opts = options(argument);
      const encoding = codec(opts.encoding === void 0 ? "utf8" : opts.encoding, true);
      convert = (raw) => {
        const buffer = Buffer.from(raw.buffer, raw.byteOffset, raw.byteLength);
        return encoding === "buffer" ? buffer : buffer.toString(encoding || "utf8");
      };
    } else if (op === "writeFile") {
      const opts = options(argument);
      const encoding = codec(opts.encoding) || "utf8";
      if (typeof data === "string") {
        spec.data = Buffer.from(data, encoding);
      } else if (ArrayBuffer.isView(data)) {
        spec.data = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
      } else {
        fail("ERR_INVALID_ARG_TYPE", "data must be a string, Buffer, TypedArray or DataView");
      }
      spec.mode = mode(opts.mode, 438);
      spec.flags = flags(opts.flag || "w", "w");
    } else if (op === "access") {
      spec.mode = argument === void 0 ? constants.F_OK : integer(argument, -2147483648, 2147483647, "mode");
    } else if (op === "stat" || op === "lstat" || op === "fstat") {
      const opts = options(argument);
      convert = (raw) => stats(raw, !!opts.bigint);
      if (opts.throwIfNoEntry === false) {
        recover = (error) => error.code === "ENOENT" || error.code === "ENOTDIR";
      }
    } else if (op === "mkdir") {
      const opts = typeof argument === "number" ? { mode: argument } : options(argument);
      if (opts.recursive !== void 0 && typeof opts.recursive !== "boolean") {
        fail("ERR_INVALID_ARG_TYPE", "recursive must be a boolean");
      }
      spec.mode = mode(opts.mode, 511);
      spec.recursive = !!opts.recursive;
      convert = (value) => value;
    } else if (op === "rename") {
      spec.dest = nativePath(path(argument));
    } else if (op === "rmdir") {
      if (options(argument).recursive) {
        unsupported("Recursive rmdir");
      }
    } else if (op === "readdir") {
      const opts = options(argument);
      const encoding = codec(opts.encoding === void 0 ? "utf8" : opts.encoding, true);
      if (opts.recursive) {
        unsupported("Recursive readdir");
      }
      spec.withFileTypes = !!opts.withFileTypes;
      convert = (entries) => entries.map((entry) => {
        const buffer = Buffer.from(entry.name);
        const name = encoding === "buffer" ? buffer : buffer.toString(encoding || "utf8");
        return opts.withFileTypes ? new Dirent(name, parentPath, entry.type) : name;
      });
    }
    return { spec, convert, recover };
  }
  function sync(op, pathname, argument, data) {
    const { spec, convert, recover } = prepare(op, pathname, argument, data);
    try {
      return convert(native.run(spec));
    } catch (error) {
      if (recover && recover(error)) {
        return void 0;
      }
      throw error;
    }
  }
  function callbackOperation(op, pathname, argument, data, callback) {
    if (typeof callback !== "function") {
      fail("ERR_INVALID_ARG_TYPE", "callback must be a function");
    }
    const { spec, convert } = prepare(op, pathname, argument, data);
    native.run(spec, (error, value) => {
      if (error) {
        callback(error);
        return;
      }
      let converted;
      try {
        converted = convert(value);
      } catch (error2) {
        callback(error2);
        return;
      }
      if (op === "readFile" || op === "realpath" || op === "stat" || op === "lstat" || op === "readdir" || op === "mkdir" || op === "open" || op === "fstat") {
        callback(null, converted);
      } else {
        callback(null);
      }
    });
  }
  const fs = { constants, Stats, Dirent };
  Object.assign(fs, createStatWatchers(host, { path, nativePath, stats }));
  Object.assign(fs, createEventWatchers(host, { path, nativePath }));
  for (const op of [
    "readFile",
    "realpath",
    "writeFile",
    "access",
    "stat",
    "lstat",
    "mkdir",
    "rmdir",
    "unlink",
    "rename",
    "readdir",
    "fstat",
    "ftruncate",
    "fsync",
    "fdatasync"
  ]) {
    if (op === "writeFile") {
      fs.writeFileSync = (path2, data, opts) => sync(op, path2, opts, data);
      fs.writeFile = function(path2, data, opts, callback) {
        if (typeof opts === "function") {
          callback = opts;
          opts = void 0;
        }
        callbackOperation(op, path2, opts, data, callback);
      };
    } else {
      fs[op + "Sync"] = (path2, opts) => sync(op, path2, opts);
      fs[op] = function(path2, opts, callback) {
        if (typeof opts === "function") {
          callback = opts;
          opts = void 0;
        }
        callbackOperation(op, path2, opts, void 0, callback);
      };
    }
  }
  const realpathCallback = fs.realpath;
  const realpathSync = fs.realpathSync;
  fs.realpath.native = (...args) => realpathCallback(...args);
  fs.realpathSync.native = (...args) => realpathSync(...args);
  fs.openSync = (path2, flag, fileMode) => sync("open", path2, { flags: flag, mode: fileMode });
  fs.open = function(path2, flag, fileMode, callback) {
    if (typeof flag === "function") {
      callback = flag;
      flag = void 0;
      fileMode = void 0;
    } else if (typeof fileMode === "function") {
      callback = fileMode;
      fileMode = void 0;
    }
    callbackOperation("open", path2, { flags: flag, mode: fileMode }, void 0, callback);
  };
  fs.closeSync = (fd) => sync("close", fd);
  fs.close = function(fd, callback = (error) => {
    if (error) {
      throw error;
    }
  }) {
    callbackOperation("close", fd, void 0, void 0, callback);
  };
  function byteView(value) {
    if (!ArrayBuffer.isView(value)) {
      fail("ERR_INVALID_ARG_TYPE", "buffer must be a Buffer, TypedArray or DataView");
    }
    return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  }
  function readPosition(value) {
    if (value == null) {
      return "-1";
    }
    if (typeof value === "bigint") {
      if (value < -1n || value > 0x7fffffffffffffffn) {
        fail("ERR_OUT_OF_RANGE", "position is out of range", RangeError);
      }
      return String(value);
    }
    return String(integer(value, -1, Number.MAX_SAFE_INTEGER, "position"));
  }
  function writePosition(value) {
    if (typeof value !== "number") {
      return "-1";
    }
    if (!Number.isInteger(value)) {
      return "-1";
    }
    if (value < -Number.MAX_SAFE_INTEGER) {
      return "-1";
    }
    if (value > Number.MAX_SAFE_INTEGER) {
      unsupported("Write positions above the safe integer range");
    }
    return String(Math.trunc(value));
  }
  function prepareRead(fd, buffer, offset, length, position) {
    integer(fd, 0, 2147483647, "fd");
    let opts;
    if (!ArrayBuffer.isView(buffer)) {
      opts = buffer == null ? {} : buffer;
      if (typeof opts !== "object") {
        fail("ERR_INVALID_ARG_TYPE", "options must be an object");
      }
      buffer = opts.buffer === void 0 ? Buffer.alloc(16384) : opts.buffer;
    } else if (typeof offset === "object") {
      opts = offset || {};
    }
    if (opts) {
      offset = opts.offset;
      length = opts.length;
      position = opts.position;
    }
    const view = byteView(buffer);
    offset = integer(offset === void 0 ? 0 : offset, 0, view.byteLength, "offset");
    length = integer(
      length === void 0 ? view.byteLength - offset : length,
      0,
      view.byteLength - offset,
      "length"
    );
    return {
      spec: { op: "read", path: fd, length, position: readPosition(position) },
      buffer,
      convert(raw) {
        byteView(buffer).set(raw, offset);
        return raw.byteLength;
      }
    };
  }
  function prepareWrite(fd, buffer, offset, length, position) {
    integer(fd, 0, 2147483647, "fd");
    const original = buffer;
    if (typeof buffer === "string") {
      const encoding = typeof offset === "string" ? offset : length;
      position = typeof offset === "string" ? null : offset;
      buffer = Buffer.from(
        buffer,
        typeof encoding === "string" && Buffer.isEncoding(encoding) ? encoding : "utf8"
      );
      offset = 0;
      length = buffer.byteLength;
    } else if (offset !== null && typeof offset === "object") {
      const opts = offset;
      offset = opts.offset;
      length = opts.length;
      position = opts.position;
    }
    const view = byteView(buffer);
    offset = integer(offset === void 0 ? 0 : offset, 0, view.byteLength, "offset");
    length = integer(
      typeof length !== "number" ? view.byteLength - offset : length,
      0,
      view.byteLength - offset,
      "length"
    );
    return {
      spec: {
        op: "write",
        path: fd,
        data: view.subarray(offset, offset + length),
        position: writePosition(position)
      },
      buffer: original
    };
  }
  fs.readSync = function(fd, ...args) {
    byteView(args[0]);
    const operation = prepareRead(fd, ...args);
    return operation.convert(native.run(operation.spec));
  };
  fs.writeSync = function(fd, ...args) {
    return native.run(prepareWrite(fd, ...args).spec);
  };
  fs.read = function(fd, ...args) {
    const callback = args.pop();
    if (typeof callback !== "function") {
      fail("ERR_INVALID_ARG_TYPE", "callback must be a function");
    }
    const operation = prepareRead(fd, ...args);
    native.run(operation.spec, (error, raw) => {
      if (error) {
        callback(error, 0, operation.buffer);
        return;
      }
      let count;
      try {
        count = operation.convert(raw);
      } catch (error2) {
        callback(error2, 0, operation.buffer);
        return;
      }
      callback(null, count, operation.buffer);
    });
  };
  fs.write = function(fd, ...args) {
    const callback = args.pop();
    if (typeof callback !== "function") {
      fail("ERR_INVALID_ARG_TYPE", "callback must be a function");
    }
    const operation = prepareWrite(fd, ...args);
    native.run(
      operation.spec,
      (error, count) => callback(error, error ? 0 : count, operation.buffer)
    );
  };
  fs.appendFileSync = function(path2, data, opts) {
    opts = options(opts);
    return fs.writeFileSync(path2, data, {
      ...opts,
      flag: opts.flag === void 0 ? "a" : opts.flag
    });
  };
  fs.appendFile = function(path2, data, opts, callback) {
    if (typeof opts === "function") {
      callback = opts;
      opts = void 0;
    }
    opts = options(opts);
    return fs.writeFile(
      path2,
      data,
      { ...opts, flag: opts.flag === void 0 ? "a" : opts.flag },
      callback
    );
  };
  fs.existsSync = function(path2) {
    try {
      fs.accessSync(path2);
      return true;
    } catch {
      return false;
    }
  };
  fs.exists = function(path2, callback) {
    if (typeof callback !== "function") {
      fail("ERR_INVALID_ARG_TYPE", "callback must be a function");
    }
    try {
      fs.access(path2, (error) => callback(!error));
    } catch {
      queueMicrotask(() => callback(false));
    }
  };
  function call(operation, ...args) {
    return new Promise(
      (resolve, reject) => fs[operation](...args, (error, value) => error ? reject(error) : resolve(value))
    );
  }
  const handleToken = Symbol("FileHandle constructor");
  const writeLeases = /* @__PURE__ */ new WeakMap();
  class FileHandle extends host.EventEmitter {
    #fd;
    #owner;
    #active = 0;
    #closing = false;
    #closePromise;
    #resolveClose;
    #rejectClose;
    constructor(token, fd) {
      super();
      if (token !== handleToken) {
        fail("ERR_ILLEGAL_CONSTRUCTOR", "Illegal FileHandle constructor");
      }
      this.#fd = fd;
      this.#owner = native.own(fd);
      writeLeases.set(this, () => {
        if (this.#fd < 0 || this.#closing) {
          fail("EBADF", "file closed", Error);
        }
        ++this.#active;
        let released = false;
        return {
          release: () => {
            if (released) {
              return;
            }
            released = true;
            --this.#active;
            if (this.#closing && this.#active === 0) {
              this.#startClose();
            }
          },
          close: () => this.#closePromise ?? this.close()
        };
      });
    }
    get fd() {
      return this.#fd;
    }
    async #run(syscall, operation) {
      if (this.#fd < 0) {
        const error = new Error("file closed");
        error.code = "EBADF";
        error.syscall = syscall;
        throw error;
      }
      ++this.#active;
      try {
        return await operation(this.#fd);
      } finally {
        --this.#active;
        if (this.#closing && this.#active === 0) {
          this.#startClose();
        }
      }
    }
    #startClose() {
      const fd = this.#owner();
      this.#fd = -1;
      try {
        fs.close(fd, (error) => error ? this.#rejectClose(error) : this.#resolveClose());
      } catch (error) {
        try {
          fs.closeSync(fd);
        } catch {
        }
        this.#rejectClose(error);
      }
    }
    close() {
      if (this.#fd < 0) {
        return Promise.resolve();
      }
      if (this.#closePromise) {
        return this.#closePromise;
      }
      this.#closing = true;
      this.#closePromise = new Promise((resolve, reject) => {
        this.#resolveClose = resolve;
        this.#rejectClose = reject;
      });
      if (this.#active === 0) {
        this.#startClose();
      }
      this.emit("close");
      return this.#closePromise;
    }
    createReadStream(options2) {
      return createReadStream(this, options2, host, () => this.#closePromise ?? this.close());
    }
    createWriteStream(options2) {
      return fs.createWriteStream(void 0, { ...options2, fd: this });
    }
    read(...args) {
      return this.#run(
        "read",
        (fd) => new Promise((resolve, reject) => {
          fs.read(
            fd,
            ...args,
            (error, bytesRead, buffer) => error ? reject(error) : resolve({ __proto__: null, bytesRead, buffer })
          );
        })
      );
    }
    write(...args) {
      return this.#run(
        "write",
        (fd) => new Promise((resolve, reject) => {
          fs.write(
            fd,
            ...args,
            (error, bytesWritten, buffer) => error ? reject(error) : resolve({ __proto__: null, bytesWritten, buffer })
          );
        })
      );
    }
    stat(opts = {}) {
      return this.#run("fstat", (fd) => {
        const { bigint = false } = opts;
        return call("fstat", fd, { bigint });
      });
    }
    truncate(length = 0) {
      return this.#run("ftruncate", (fd) => call("ftruncate", fd, length));
    }
    sync() {
      return this.#run("fsync", (fd) => call("fsync", fd));
    }
    datasync() {
      return this.#run("fdatasync", (fd) => call("fdatasync", fd));
    }
    readFile(opts) {
      return this.#run("readFile", (fd) => call("readFile", fd, opts));
    }
    writeFile(data, opts) {
      return this.#run("writeFile", (fd) => call("writeFile", fd, data, opts));
    }
    appendFile(data, opts) {
      return this.#run("writeFile", (fd) => call("appendFile", fd, data, opts));
    }
    [Symbol.asyncDispose]() {
      return this.close();
    }
  }
  Object.assign(
    fs,
    createWriteStreams(fs, host, { path, flags, mode }, (handle) => {
      const acquire = writeLeases.get(handle);
      if (!acquire) {
        fail("ERR_INVALID_ARG_TYPE", "fd must be a FileHandle");
      }
      return acquire();
    })
  );
  const promises = { constants };
  for (const op of [
    "readFile",
    "realpath",
    "writeFile",
    "appendFile",
    "access",
    "stat",
    "lstat",
    "mkdir",
    "rmdir",
    "unlink",
    "rename",
    "readdir"
  ]) {
    promises[op] = function(...args) {
      return new Promise((resolve, reject) => {
        if (args[0] instanceof FileHandle && ["readFile", "writeFile", "appendFile"].includes(op)) {
          const [handle, ...rest] = args;
          resolve(handle[op](...rest));
          return;
        }
        if (typeof args[0] === "number") {
          fail("ERR_INVALID_ARG_TYPE", "Promise filesystem APIs require a path or FileHandle");
        }
        fs[op](...args, (error, value) => error ? reject(error) : resolve(value));
      });
    };
  }
  promises.open = async function(path2, flag, fileMode) {
    return new FileHandle(handleToken, await call("open", path2, flag, fileMode));
  };
  namedPromisify(fs.read, ["bytesRead", "buffer"], host.promisifyState);
  namedPromisify(fs.write, ["bytesWritten", "buffer"], host.promisifyState);
  fs.promises = promises;
  return fs;
}
return createFileSystemModule;
})()
