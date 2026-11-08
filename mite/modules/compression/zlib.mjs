/** Node-style compression streams backed by bounded native zlib steps. */
import { constants, error, unsupported, options, flushKind } from './policy.mjs';

export default function createZlibModule(host) {
  const { Transform } = host.stream;
  const { Buffer } = host;
  const state = new WeakMap();
  const markers = new WeakMap();

  function native(mode, configuration) {
    let window = configuration.window;
    if (window === 8 && mode === 4) {
      throw error('ERR_OUT_OF_RANGE', 'Gzip windowBits must be at least 9', RangeError);
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
        pending: undefined,
        resume: undefined,
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

    _start(bytes, kind, callback) {
      const current = state.get(this);
      current.pending = { bytes, kind, callback, offset: 0, again: true };
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
      } catch (error) {
        current.pending = undefined;
        job.callback(error);
        return;
      }
      job.offset += result.consumed;
      current.bytes += result.consumed;
      const complete = !result.again && job.offset === job.bytes.length;
      // A full readable queue owns the continuation until the consumer reads.
      const advance = () => {
        if (this.destroyed || current.pending !== job) {
          return;
        }
        if (complete) {
          current.pending = undefined;
          job.callback();
        } else {
          this._schedule();
        }
      };
      // Consumer exceptions propagate normally; never call a completed callback twice.
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
        current.resume = undefined;
        resume();
      }
      super._read(size);
    }

    _destroy(error, callback) {
      const current = state.get(this);
      if (current) {
        current.codec.close();
        current.resume = undefined;
        const job = current.pending;
        current.pending = undefined;
        if (job) {
          job.callback(error ?? new Error('Zlib stream destroyed'));
        }
      }
      callback(error);
    }

    flush(kind, callback) {
      if (typeof kind === 'function') {
        callback = kind;
        kind = undefined;
      }
      const selected = flushKind(kind, 3);
      if (callback !== undefined && typeof callback !== 'function') {
        throw error('ERR_INVALID_ARG_TYPE', 'callback must be a function', TypeError);
      }
      const marker = Buffer.alloc(0);
      markers.set(marker, selected);
      this.write(marker, callback);
    }

    close(callback) {
      if (callback !== undefined && typeof callback !== 'function') {
        throw error('ERR_INVALID_ARG_TYPE', 'callback must be a function', TypeError);
      }
      this.destroy();
      if (callback) {
        host.nextTick(callback);
      }
    }

    params() {
      unsupported('params');
    }

    reset() {
      unsupported('reset');
    }
  }

  function bytes(input) {
    if (typeof input === 'string') {
      return Buffer.from(input);
    }
    if (input instanceof ArrayBuffer) {
      return Buffer.from(new Uint8Array(input));
    }
    if (ArrayBuffer.isView(input)) {
      return Buffer.from(new Uint8Array(input.buffer, input.byteOffset, input.byteLength));
    }
    throw error('ERR_INVALID_ARG_TYPE', 'input must be a string or byte buffer', TypeError);
  }

  const api = { constants };
  for (const [mode, name] of [
    'DeflateRaw',
    'InflateRaw',
    'Deflate',
    'Inflate',
    'Gzip',
    'Gunzip'
  ].entries()) {
    const Class = class extends CodecTransform {
      constructor(value) {
        super(mode, value);
      }
    };
    Object.defineProperty(Class, 'name', { value: name });
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
        // Ordinary chunks precede the explicit final flush; keep the native input bound.
        for (;;) {
          const final = offset === data.length;
          const result = codec.step(
            data.subarray(offset, Math.min(data.length, offset + 1048576)),
            final ? configuration.finish : configuration.flush
          );
          offset += result.consumed;
          length += result.data.length;
          if (length > configuration.max) {
            throw error('ERR_BUFFER_TOO_LARGE', 'Zlib output exceeds maxOutputLength', RangeError);
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
      if (typeof value === 'function') {
        callback = value;
        value = undefined;
      }
      if (typeof callback !== 'function') {
        throw error('ERR_INVALID_ARG_TYPE', 'callback must be a function', TypeError);
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

      stream.on('data', (chunk) => {
        length += chunk.length;
        if (length > configuration.max) {
          stream.destroy(
            error('ERR_BUFFER_TOO_LARGE', 'Zlib output exceeds maxOutputLength', RangeError)
          );
        } else {
          chunks.push(chunk);
        }
      });

      stream.once('error', (err) => finish(err));

      stream.once('end', () => finish(null, Buffer.concat(chunks, length)));

      stream.end(data);
    };
  }
  for (const name of [
    'createUnzip',
    'Unzip',
    'createBrotliCompress',
    'createBrotliDecompress',
    'createZstdCompress',
    'createZstdDecompress',
    'brotliCompress',
    'brotliCompressSync',
    'brotliDecompress',
    'brotliDecompressSync',
    'zstdCompress',
    'zstdCompressSync',
    'zstdDecompress',
    'zstdDecompressSync',
    'crc32'
  ]) {
    api[name] = () => unsupported(name);
  }
  return api;
}
