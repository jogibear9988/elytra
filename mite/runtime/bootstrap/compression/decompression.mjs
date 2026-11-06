/** Pull-driven inflate: one native output block per read, even for highly expanding input. */
export default function createDecompression(host) {
  const { ReadableStream, WritableStream } = host.webStreams;
  const states = new WeakMap();
  const empty = new Uint8Array();

  function create(format) {
    const native = host.inflate.create(format);
    let input;
    let offset = 0;
    let write;
    let close;
    let wake;
    let readController;
    let sinkController;
    let ending = false;
    let finished = false;
    let needInput = true;

    let unsubscribe = () => {};

    function notify() {
      wake?.();
      wake = undefined;
    }

    function fail(reason, writableAbort = false) {
      if (finished) {
        return;
      }
      finished = true;
      native.close();
      unsubscribe();
      readController.error(reason);
      // abort() owns the writable's state transition and Promise. Erroring its
      // controller during signal dispatch would incorrectly reject that abort.
      if (!writableAbort) {
        sinkController.error(reason);
      }
      write?.reject(reason);
      close?.reject(reason);
      input = undefined;
      notify();
    }

    const readable = new ReadableStream({
      type: 'bytes',

      start(controller) {
        readController = controller;
      },

      async pull(controller) {
        while (!finished) {
          if (!input && needInput && !ending) {
            await new Promise((resolve) => {
              wake = resolve;
            });
            continue;
          }
          try {
            const bytes = input ? input.subarray(offset, offset + 1048576) : empty;
            const result = native.step(bytes, ending && !input);
            offset += result.consumed;
            needInput = result.needInput;
            if (input && offset === input.byteLength && needInput) {
              input = undefined;
              offset = 0;
              write?.resolve();
              write = undefined;
            }
            const produced = result.data.byteLength;
            if (produced) {
              controller.enqueue(result.data);
            }
            if (result.done) {
              finished = true;
              native.close();
              unsubscribe();
              controller.close();
              controller.byobRequest?.respond(0);
              close?.resolve();
            }
            if (produced || result.done) {
              return;
            }
            if (!needInput && result.consumed === 0) {
              throw new TypeError('Inflater made no progress');
            }
          } catch (cause) {
            const error = new TypeError('Decompression failed', { cause });
            if (cause.code) {
              error.code = cause.code;
            }
            fail(error);
          }
        }
      },

      cancel(reason) {
        fail(reason);
      }
    });
    const writable = new WritableStream({
      start(controller) {
        sinkController = controller;
        unsubscribe = host.abort.subscribe(controller.signal, () =>
          fail(host.abort.reason(controller.signal), true)
        );
      },

      write(chunk) {
        try {
          const bytes =
            chunk instanceof ArrayBuffer
              ? new Uint8Array(chunk)
              : ArrayBuffer.isView(chunk)
                ? new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength)
                : null;
          if (!bytes) {
            throw new TypeError('Compressed chunks must be BufferSource values');
          }
          if (!bytes.byteLength) {
            return;
          }
          input = bytes.slice();
          offset = 0;
          return new Promise((resolve, reject) => {
            write = { resolve, reject };
            notify();
          });
        } catch (error) {
          fail(error);
          throw error;
        }
      },

      close() {
        ending = true;
        return new Promise((resolve, reject) => {
          close = { resolve, reject };
          notify();
        });
      },

      abort(reason) {
        fail(reason, true);
      }
    });
    return { readable, writable };
  }

  class DecompressionStream {
    constructor(format) {
      format = String(format);
      if (!['gzip', 'deflate', 'deflate-raw'].includes(format)) {
        throw new TypeError('Unsupported decompression format');
      }
      states.set(this, create(format));
    }

    get readable() {
      if (!states.has(this)) {
        throw new TypeError('Expected DecompressionStream receiver');
      }
      return states.get(this).readable;
    }

    get writable() {
      if (!states.has(this)) {
        throw new TypeError('Expected DecompressionStream receiver');
      }
      return states.get(this).writable;
    }
  }
  Object.defineProperty(DecompressionStream.prototype, Symbol.toStringTag, {
    value: 'DecompressionStream',
    configurable: true
  });
  return { DecompressionStream, create };
}
