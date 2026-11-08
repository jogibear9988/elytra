/** Pull bounded file chunks through the existing native FileHandle read/close contract. */
export default function createReadStream(handle, value, host, closeHandle) {
  const unsupported = (name) => {
    throw Object.assign(new Error(`FileHandle.createReadStream ${name} is not implemented`), {
      code: 'ERR_MITE_UNSUPPORTED_API'
    });
  };

  if (typeof value === 'string') {
    value = { encoding: value };
  }
  if (value === undefined) {
    value = {};
  }
  if (value === null || typeof value !== 'object') {
    throw Object.assign(new TypeError('options must be an object'), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }
  for (const name of Object.keys(value)) {
    if (
      !['encoding', 'start', 'end', 'highWaterMark', 'autoClose', 'emitClose', 'signal'].includes(
        name
      )
    ) {
      unsupported(`option ${name}`);
    }
  }

  const integer = (v, fallback, min, max, name) => {
    if (v === undefined) {
      return fallback;
    }
    if (typeof v !== 'number' || !Number.isSafeInteger(v) || v < min || v > max) {
      throw Object.assign(new RangeError(`${name} is out of range`), { code: 'ERR_OUT_OF_RANGE' });
    }
    return v;
  };

  for (const key of ['autoClose', 'emitClose']) {
    if (value[key] !== undefined && typeof value[key] !== 'boolean') {
      throw Object.assign(new TypeError(`${key} must be a boolean`), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }
  }
  let position = integer(value.start, undefined, 0, Number.MAX_SAFE_INTEGER, 'start');
  const end =
    value.end === Infinity
      ? Infinity
      : integer(value.end, Infinity, position ?? 0, Number.MAX_SAFE_INTEGER, 'end');
  const size = integer(value.highWaterMark, 65536, 1, 1048576, 'highWaterMark');
  const autoClose = value.autoClose !== false;
  let reading;
  let count = 0;
  // Readable may destroy synchronously for an already-aborted signal. Install
  // a usable close callback first and never attach it after destruction.
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
          reading = undefined;
          if (this.destroyed) {
            return;
          }
          if (position !== undefined) {
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
          reading = undefined;
          this.destroy(error);
        }
      );
    },

    destroy(error, callback) {
      handle.removeListener('close', onClose);
      // FileHandle.close itself waits for native references. Keep this wrapper
      // alive until an in-flight read retires before releasing its descriptor.
      const finish = () => closeHandle();

      Promise.resolve(reading)
        .catch(() => {})
        .then(finish)
        .then(
          () => callback(error),
          (closeError) => callback(error ?? closeError)
        );
    }
  });

  if (!stream.destroyed) {
    handle.on('close', onClose);
  }
  Object.defineProperties(stream, { bytesRead: { get: () => count }, path: { value: undefined } });
  return stream;
}
