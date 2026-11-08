/* Generated from mite/modules/compression/zlib.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/compression/policy.mjs
var constants = Object.freeze({
  Z_NO_FLUSH: 0,
  Z_PARTIAL_FLUSH: 1,
  Z_SYNC_FLUSH: 2,
  Z_FULL_FLUSH: 3,
  Z_FINISH: 4,
  Z_BLOCK: 5,
  Z_TREES: 6,
  Z_OK: 0,
  Z_STREAM_END: 1,
  Z_NEED_DICT: 2,
  Z_ERRNO: -1,
  Z_STREAM_ERROR: -2,
  Z_DATA_ERROR: -3,
  Z_MEM_ERROR: -4,
  Z_BUF_ERROR: -5,
  Z_VERSION_ERROR: -6,
  Z_NO_COMPRESSION: 0,
  Z_BEST_SPEED: 1,
  Z_BEST_COMPRESSION: 9,
  Z_DEFAULT_COMPRESSION: -1,
  Z_FILTERED: 1,
  Z_HUFFMAN_ONLY: 2,
  Z_RLE: 3,
  Z_FIXED: 4,
  Z_DEFAULT_STRATEGY: 0,
  Z_DEFLATED: 8
});
var error = (code, message, Type = Error) => Object.assign(new Type(message), { code });
var unsupported = (name) => {
  throw error("ERR_MITE_UNSUPPORTED_API", `node:zlib ${name} is not implemented`);
};
function integer(value, fallback, min, max, name) {
  if (value === void 0) {
    return fallback;
  }
  if (typeof value !== "number") {
    throw error("ERR_INVALID_ARG_TYPE", `${name} must be a number`, TypeError);
  }
  if (!Number.isInteger(value) || value < min || value > max) {
    throw error("ERR_OUT_OF_RANGE", `${name} is out of range`, RangeError);
  }
  return value;
}
function flushKind(value, fallback) {
  const kind = integer(value, fallback, 0, 6, "flush");
  if (![0, 2, 3, 4].includes(kind)) {
    unsupported(`flush kind ${kind}`);
  }
  return kind;
}
function options(value) {
  if (value === void 0 || value === null) {
    value = {};
  }
  if (typeof value !== "object") {
    throw error("ERR_INVALID_ARG_TYPE", "options must be an object", TypeError);
  }
  for (const name of Object.keys(value)) {
    if (![
      "windowBits",
      "level",
      "memLevel",
      "strategy",
      "chunkSize",
      "flush",
      "finishFlush",
      "maxOutputLength",
      "highWaterMark",
      "readableHighWaterMark",
      "writableHighWaterMark",
      "encoding",
      "decodeStrings",
      "emitClose",
      "autoDestroy",
      "signal"
    ].includes(name)) {
      unsupported(`option ${name}`);
    }
  }
  if (value.emitClose === false) {
    unsupported("emitClose:false");
  }
  if (value.decodeStrings === false) {
    unsupported("decodeStrings:false");
  }
  const window = integer(value.windowBits, 15, 8, 15, "windowBits");
  return {
    raw: value,
    window,
    level: integer(value.level, -1, -1, 9, "level"),
    memory: integer(value.memLevel, 8, 1, 9, "memLevel"),
    strategy: integer(value.strategy, 0, 0, 4, "strategy"),
    chunk: integer(value.chunkSize, 16384, 64, 1048576, "chunkSize"),
    flush: flushKind(value.flush, 0),
    finish: flushKind(value.finishFlush, 4),
    max: integer(value.maxOutputLength, 64 * 1048576, 1, 64 * 1048576, "maxOutputLength")
  };
}

// mite/modules/compression/zlib.mjs
function createZlibModule(host) {
  const { Transform } = host.stream;
  const { Buffer } = host;
  const state = /* @__PURE__ */ new WeakMap();
  const markers = /* @__PURE__ */ new WeakMap();
  function native(mode, configuration) {
    let window = configuration.window;
    if (window === 8 && mode === 4) {
      throw error("ERR_OUT_OF_RANGE", "Gzip windowBits must be at least 9", RangeError);
    }
    if (window === 8 && (mode === 0 || mode === 2)) {
      window = 9;
    }
    return host.native.create(
      mode,
      window,
      configuration.level,
      configuration.memory,
      configuration.strategy,
      configuration.chunk
    );
  }
  class CodecTransform extends Transform {
    constructor(mode, value) {
      const configuration = options(value);
      super(configuration.raw);
      state.set(this, {
        codec: native(mode, configuration),
        configuration,
        bytes: 0,
        pending: void 0,
        resume: void 0,
        scheduled: false
      });
    }
    get bytesWritten() {
      return state.get(this).bytes;
    }
    _transform(chunk, encoding, callback) {
      this._start(chunk, markers.get(chunk) ?? state.get(this).configuration.flush, callback);
    }
    _flush(callback) {
      this._start(Buffer.alloc(0), state.get(this).configuration.finish, callback);
    }
    _start(bytes2, kind, callback) {
      const current = state.get(this);
      current.pending = { bytes: bytes2, kind, callback, offset: 0, again: true };
      this._schedule();
    }
    _schedule() {
      const current = state.get(this);
      if (current.scheduled || !current.pending || this.destroyed) {
        return;
      }
      current.scheduled = true;
      host.schedule(() => {
        current.scheduled = false;
        this._pump();
      });
    }
    _pump() {
      const current = state.get(this);
      const job = current.pending;
      if (!job || this.destroyed) {
        return;
      }
      let result;
      try {
        const end = Math.min(job.bytes.length, job.offset + 1048576);
        result = current.codec.step(job.bytes.subarray(job.offset, end), job.kind);
      } catch (error2) {
        current.pending = void 0;
        job.callback(error2);
        return;
      }
      job.offset += result.consumed;
      current.bytes += result.consumed;
      const complete = !result.again && job.offset === job.bytes.length;
      const advance = () => {
        if (this.destroyed || current.pending !== job) {
          return;
        }
        if (complete) {
          current.pending = void 0;
          job.callback();
        } else {
          this._schedule();
        }
      };
      if (result.data.length && !this.push(Buffer.from(result.data))) {
        current.resume = advance;
      } else {
        advance();
      }
    }
    _read(size) {
      const current = state.get(this);
      const resume = current?.resume;
      if (resume) {
        current.resume = void 0;
        resume();
      }
      super._read(size);
    }
    _destroy(error2, callback) {
      const current = state.get(this);
      if (current) {
        current.codec.close();
        current.resume = void 0;
        const job = current.pending;
        current.pending = void 0;
        if (job) {
          job.callback(error2 ?? new Error("Zlib stream destroyed"));
        }
      }
      callback(error2);
    }
    flush(kind, callback) {
      if (typeof kind === "function") {
        callback = kind;
        kind = void 0;
      }
      const selected = flushKind(kind, 3);
      if (callback !== void 0 && typeof callback !== "function") {
        throw error("ERR_INVALID_ARG_TYPE", "callback must be a function", TypeError);
      }
      const marker = Buffer.alloc(0);
      markers.set(marker, selected);
      this.write(marker, callback);
    }
    close(callback) {
      if (callback !== void 0 && typeof callback !== "function") {
        throw error("ERR_INVALID_ARG_TYPE", "callback must be a function", TypeError);
      }
      this.destroy();
      if (callback) {
        host.nextTick(callback);
      }
    }
    params() {
      unsupported("params");
    }
    reset() {
      unsupported("reset");
    }
  }
  function bytes(input) {
    if (typeof input === "string") {
      return Buffer.from(input);
    }
    if (input instanceof ArrayBuffer) {
      return Buffer.from(new Uint8Array(input));
    }
    if (ArrayBuffer.isView(input)) {
      return Buffer.from(new Uint8Array(input.buffer, input.byteOffset, input.byteLength));
    }
    throw error("ERR_INVALID_ARG_TYPE", "input must be a string or byte buffer", TypeError);
  }
  const api = { constants };
  for (const [mode, name] of [
    "DeflateRaw",
    "InflateRaw",
    "Deflate",
    "Inflate",
    "Gzip",
    "Gunzip"
  ].entries()) {
    const Class = class extends CodecTransform {
      constructor(value) {
        super(mode, value);
      }
    };
    Object.defineProperty(Class, "name", { value: name });
    api[name] = Class;
    api[`create${name}`] = (value) => new Class(value);
    const method = name[0].toLowerCase() + name.slice(1);
    api[`${method}Sync`] = (input, value) => {
      const configuration = options(value);
      const codec = native(mode, configuration);
      const data = bytes(input);
      const chunks = [];
      let length = 0;
      let offset = 0;
      try {
        for (; ; ) {
          const final = offset === data.length;
          const result = codec.step(
            data.subarray(offset, Math.min(data.length, offset + 1048576)),
            final ? configuration.finish : configuration.flush
          );
          offset += result.consumed;
          length += result.data.length;
          if (length > configuration.max) {
            throw error("ERR_BUFFER_TOO_LARGE", "Zlib output exceeds maxOutputLength", RangeError);
          }
          if (result.data.length) {
            chunks.push(Buffer.from(result.data));
          }
          if (final && !result.again) {
            break;
          }
        }
        return Buffer.concat(chunks, length);
      } finally {
        codec.close();
      }
    };
    api[method] = (input, value, callback) => {
      if (typeof value === "function") {
        callback = value;
        value = void 0;
      }
      if (typeof callback !== "function") {
        throw error("ERR_INVALID_ARG_TYPE", "callback must be a function", TypeError);
      }
      const data = bytes(input);
      const configuration = options(value);
      const stream = new Class(value);
      const chunks = [];
      let length = 0;
      let settled = false;
      const finish = (err, result) => {
        if (settled) {
          return;
        }
        settled = true;
        callback(err, result);
      };
      stream.on("data", (chunk) => {
        length += chunk.length;
        if (length > configuration.max) {
          stream.destroy(
            error("ERR_BUFFER_TOO_LARGE", "Zlib output exceeds maxOutputLength", RangeError)
          );
        } else {
          chunks.push(chunk);
        }
      });
      stream.once("error", (err) => finish(err));
      stream.once("end", () => finish(null, Buffer.concat(chunks, length)));
      stream.end(data);
    };
  }
  for (const name of [
    "createUnzip",
    "Unzip",
    "createBrotliCompress",
    "createBrotliDecompress",
    "createZstdCompress",
    "createZstdDecompress",
    "brotliCompress",
    "brotliCompressSync",
    "brotliDecompress",
    "brotliDecompressSync",
    "zstdCompress",
    "zstdCompressSync",
    "zstdDecompress",
    "zstdDecompressSync",
    "crc32"
  ]) {
    api[name] = () => unsupported(name);
  }
  return api;
}
return createZlibModule;
})()
