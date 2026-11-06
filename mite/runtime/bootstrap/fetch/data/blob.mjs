/** Adapt the pinned immutable Blob core to captured LibJS codecs and byte streams. */
export default function createBlobs(SourceBlob, host) {
  const size = Object.getOwnPropertyDescriptor(SourceBlob.prototype, 'size').get;
  const type = Object.getOwnPropertyDescriptor(SourceBlob.prototype, 'type').get;
  const slice = SourceBlob.prototype.slice;
  const arrayBuffer = SourceBlob.prototype.arrayBuffer;
  const text = SourceBlob.prototype.text;
  const files = new WeakMap();
  const encoder = new host.TextEncoder();
  const decoder = new host.TextDecoder('utf-8', { ignoreBOM: true });

  function scalar(value) {
    if (typeof value === 'symbol') {
      throw new TypeError('Cannot convert Symbol to a string');
    }
    return decoder.decode(encoder.encode(String(value)));
  }

  function isBlob(value) {
    try {
      size.call(value);
      return true;
    } catch {
      return false;
    }
  }

  function options(value) {
    if (value == null) {
      return {};
    }
    if (typeof value !== 'object' && typeof value !== 'function') {
      throw new TypeError('Blob options must be a dictionary');
    }
    return value;
  }

  class Blob extends SourceBlob {
    constructor(parts = [], init = undefined) {
      const opts = options(init);
      if (
        parts === null ||
        !['object', 'function'].includes(typeof parts) ||
        typeof parts[Symbol.iterator] !== 'function'
      ) {
        throw new TypeError('Blob parts must be an iterable object');
      }
      const endings = opts.endings === undefined ? 'transparent' : String(opts.endings);
      if (!['transparent', 'native'].includes(endings)) {
        throw new TypeError('Invalid Blob endings');
      }
      const values = [];
      for (const part of parts) {
        if (isBlob(part) || part instanceof ArrayBuffer || ArrayBuffer.isView(part)) {
          values.push(part);
        } else {
          let text = scalar(part);
          if (endings === 'native') {
            text = text.replace(/\r?\n/g, host.platform === 'win32' ? '\r\n' : '\n');
          }
          values.push(text);
        }
      }
      const mime = opts.type === undefined ? '' : String(opts.type);
      super(values, { type: /^[\x20-\x7E]*$/.test(mime) ? mime.toLowerCase() : '' });
    }

    static [Symbol.hasInstance](value) {
      return isBlob(value);
    }

    get size() {
      return size.call(this);
    }

    get type() {
      return type.call(this);
    }

    arrayBuffer() {
      return arrayBuffer.call(this);
    }

    text() {
      return text.call(this);
    }

    slice(start = 0, end = size.call(this), contentType = '') {
      function integer(value) {
        const number = Number(value);
        return Number.isFinite(number) ? Math.trunc(number) : 0;
      }

      return new Blob([slice.call(this, integer(start), integer(end))], { type: contentType });
    }

    async bytes() {
      return new Uint8Array(await arrayBuffer.call(this));
    }

    stream() {
      const blob = this;
      const length = size.call(blob);
      let position = 0;
      let cancelled = false;
      return new host.webStreams.ReadableStream({
        type: 'bytes',

        async pull(controller) {
          if (position >= length) {
            controller.close();
            controller.byobRequest?.respond(0);
            return;
          }
          const buffer = await arrayBuffer.call(
            slice.call(blob, position, Math.min(position + 65536, length))
          );
          if (cancelled) {
            return;
          }
          position += buffer.byteLength;
          controller.enqueue(new Uint8Array(buffer));
          if (position === length) {
            controller.close();
            controller.byobRequest?.respond(0);
          }
        },

        cancel() {
          cancelled = true;
        }
      });
    }
  }

  class File extends Blob {
    constructor(parts, name, init = undefined) {
      if (arguments.length < 2) {
        throw new TypeError('File requires parts and name');
      }
      const opts = options(init);
      super(parts, opts);
      const modified = opts.lastModified === undefined ? Date.now() : Number(opts.lastModified);
      // Preserve the measured Node 24 timestamp coercion, including fractions.
      files.set(this, { name: scalar(name), lastModified: Number.isNaN(modified) ? 0 : modified });
    }

    get name() {
      if (!files.has(this)) {
        throw new TypeError('Expected File receiver');
      }
      return files.get(this).name;
    }

    get lastModified() {
      if (!files.has(this)) {
        throw new TypeError('Expected File receiver');
      }
      return files.get(this).lastModified;
    }

    get [Symbol.toStringTag]() {
      return 'File';
    }

    static [Symbol.hasInstance](value) {
      return files.has(value);
    }
  }
  // Core slices are instances of the provider class. Route their internal reads
  // through the same captured byte-stream adapter as public Blob/File instances.
  SourceBlob.prototype.stream = Blob.prototype.stream;
  return {
    Blob,
    File,
    scalar,
    isBlob,

    isFile: (value) => files.has(value),

    size: (value) => size.call(value),

    type: (value) => type.call(value),

    stream: (value) => Blob.prototype.stream.call(value)
  };
}
