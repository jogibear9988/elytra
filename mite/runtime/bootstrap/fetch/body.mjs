/** Body ownership, snapshots, tee cloning and convenience consumption on LibJS streams. */
export default function createBodies(host) {
  const { ReadableStream } = host.webStreams;
  const Buffer = host.Buffer;
  const encoder = new host.TextEncoder();
  const locked = Object.getOwnPropertyDescriptor(ReadableStream.prototype, 'locked').get;

  function isStream(value) {
    try {
      locked.call(value);
      return true;
    } catch {
      return false;
    }
  }

  function used(body) {
    return body.used || (body.stream !== null && host.streamState.isDisturbed(body.stream));
  }

  function unusable(body) {
    return used(body) || (body.stream !== null && locked.call(body.stream));
  }

  function fromBytes(bytes, type = null) {
    return {
      stream: new ReadableStream({
        type: 'bytes',

        start(controller) {
          // Preserve a separate replay snapshot: enqueue transfers its buffer.
          if (bytes.byteLength) {
            controller.enqueue(bytes.slice());
          }
          controller.close();
        }
      }),
      source: bytes,
      length: bytes.byteLength,
      type,
      used: false
    };
  }

  function extract(input) {
    if (typeof input === 'symbol') {
      throw new TypeError('Cannot convert Symbol to a body string');
    }
    if (input === null || input === undefined) {
      return { stream: null, source: null, length: 0, type: null, used: false };
    }
    if (isStream(input)) {
      const body = { stream: input, source: null, length: null, type: null, used: false };
      if (unusable(body)) {
        throw new TypeError('Body stream is locked or already used');
      }
      return body;
    }
    if (input instanceof ArrayBuffer) {
      return fromBytes(new Uint8Array(input).slice());
    }
    if (ArrayBuffer.isView(input)) {
      return fromBytes(new Uint8Array(input.buffer, input.byteOffset, input.byteLength).slice());
    }
    if (input instanceof host.URLSearchParams) {
      return fromBytes(
        encoder.encode(input.toString()),
        'application/x-www-form-urlencoded;charset=UTF-8'
      );
    }
    if (
      input &&
      typeof input === 'object' &&
      (typeof input.stream === 'function' ||
        Object.prototype.toString.call(input) === '[object FormData]')
    ) {
      throw host.unsupported('Blob/File/FormData bodies');
    }
    // TextEncoder performs the required scalar-value replacement of lone surrogates.
    return fromBytes(encoder.encode(String(input)), 'text/plain;charset=UTF-8');
  }

  function clone(body) {
    if (unusable(body)) {
      throw new TypeError('Body is locked or already used');
    }
    if (body.stream === null) {
      return { ...body };
    }
    const [left, right] = body.stream.tee();
    body.stream = left;
    return { ...body, stream: right };
  }

  function transfer(body) {
    if (unusable(body)) {
      throw new TypeError('Body is locked or already used');
    }
    if (body.stream === null) {
      return { ...body };
    }
    const reader = body.stream.getReader();
    body.used = true;
    return {
      ...body,
      used: false,
      stream: new ReadableStream({
        async pull(controller) {
          try {
            const { value, done } = await reader.read();
            if (done) {
              controller.close();
            } else {
              controller.enqueue(value);
            }
          } catch (error) {
            controller.error(error);
          }
        },

        cancel(reason) {
          return reader.cancel(reason);
        }
      })
    };
  }

  async function bytes(body) {
    if (unusable(body)) {
      throw new TypeError('Body is locked or already used');
    }
    if (body.stream === null) {
      return new Uint8Array();
    }
    body.used = true;
    const reader = body.stream.getReader();
    const chunks = [];
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) {
          break;
        }
        if (!(value instanceof Uint8Array)) {
          throw new TypeError('Body stream chunks must be Uint8Array');
        }
        chunks.push(Buffer.from(value));
      }
      return new Uint8Array(Buffer.concat(chunks));
    } catch (error) {
      reader.cancel(error).catch(() => {});

      throw error;
    } finally {
      reader.releaseLock();
    }
  }

  function install(Type, state) {
    Object.defineProperties(Type.prototype, {
      body: {
        configurable: true,
        enumerable: true,

        get() {
          return state(this).body.stream;
        }
      },
      bodyUsed: {
        configurable: true,
        enumerable: true,

        get() {
          return used(state(this).body);
        }
      }
    });
    for (const [name, convert] of Object.entries({
      bytes: (value) => value,

      arrayBuffer: (value) => value.buffer,

      text: (value) => new host.TextDecoder().decode(value),

      json: (value) => JSON.parse(new host.TextDecoder().decode(value))
    })) {
      Object.defineProperty(Type.prototype, name, {
        configurable: true,
        writable: true,

        value: async function () {
          return convert(await bytes(state(this).body));
        }
      });
    }
    for (const name of ['blob', 'formData']) {
      Object.defineProperty(Type.prototype, name, {
        configurable: true,
        writable: true,

        value: async function () {
          state(this);
          throw host.unsupported(`Body.${name}`);
        }
      });
    }
  }

  return { extract, clone, transfer, used, unusable, fromBytes, isStream, install };
}
