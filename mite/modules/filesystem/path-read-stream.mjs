/** Path streams reuse actual FileHandle reads and the bounded pull/close engine. */
import createHandleStream from './read-stream.mjs';

export default function createPathReadStreams(fs, host, validate, acquireHandle) {
  const fail = (code, message) => {
    throw Object.assign(new TypeError(message), { code });
  };

  function ReadStream(filename, value = {}) {
    if (!new.target) {
      return new ReadStream(filename, value);
    }
    if (typeof value === 'string') {
      value = { encoding: value };
    }
    if (!value || typeof value !== 'object') {
      fail('ERR_INVALID_ARG_TYPE', 'options must be an object or encoding');
    }
    for (const key of Object.keys(value)) {
      if (
        ![
          'encoding',
          'flags',
          'mode',
          'fd',
          'start',
          'end',
          'highWaterMark',
          'autoClose',
          'emitClose',
          'signal'
        ].includes(key)
      ) {
        fail('ERR_MITE_UNSUPPORTED_API', `ReadStream option ${key} is not implemented`);
      }
    }
    const supplied = value.fd !== undefined && value.fd !== null;
    const pathname = supplied ? undefined : validate.path(filename);
    const flags = validate.flags(value.flags, 'r');
    const mode = validate.mode(value.mode, 0o666);
    const providedHandle = supplied && typeof value.fd === 'object' ? value.fd : undefined;
    if (
      supplied &&
      !providedHandle &&
      (!Number.isInteger(value.fd) || value.fd < 0 || value.fd > 0x7fffffff)
    ) {
      fail('ERR_INVALID_ARG_TYPE', 'fd must be a descriptor or FileHandle');
    }
    const forwarded = {};
    for (const key of [
      'encoding',
      'start',
      'end',
      'highWaterMark',
      'autoClose',
      'emitClose',
      'signal'
    ]) {
      if (key in value) {
        forwarded[key] = value[key];
      }
    }
    let lease;
    let actual = providedHandle;
    let fd = supplied ? (providedHandle ? providedHandle.fd : value.fd) : null;
    let pending = !supplied;
    let closePromise;
    const proxy = new host.EventEmitter();

    const onClose = () => proxy.emit('close');

    proxy.read = (...args) => {
      if (actual) {
        return actual.read(...args);
      }
      return new Promise((resolve, reject) =>
        fs.read(fd, ...args, (error, bytesRead, buffer) =>
          error ? reject(error) : resolve({ bytesRead, buffer })
        )
      );
    };

    const close = () => {
      if (closePromise) {
        return closePromise;
      }
      actual?.removeListener('close', onClose);
      lease?.release();
      closePromise =
        fd === null
          ? Promise.resolve()
          : actual
            ? lease
              ? lease.close()
              : actual.close()
            : new Promise((resolve, reject) =>
                fs.close(fd, (error) => (error ? reject(error) : resolve()))
              );
      return closePromise.finally(() => {
        fd = null;
        pending = false;
      });
    };

    let stream;
    try {
      if (providedHandle) {
        lease = acquireHandle(providedHandle);
      }
      stream = createHandleStream(proxy, forwarded, host, close, {
        path: pathname,

        construct(callback) {
          if (supplied || value.signal?.aborted === true) {
            pending = false;
            callback();
            return;
          }

          fs.promises.open(pathname, flags, mode).then(
            (handle) => {
              actual = handle;
              fd = handle.fd;
              pending = false;
              actual.on('close', onClose);
              try {
                this.emit('open', fd);
                this.emit('ready');
              } finally {
                callback();
              }
            },
            (error) => {
              pending = false;
              callback(error);
            }
          );
        }
      });
      Object.setPrototypeOf(stream, new.target.prototype);
      Object.defineProperties(stream, { fd: { get: () => fd }, pending: { get: () => pending } });
      if (providedHandle && !stream.destroyed) {
        providedHandle.on('close', onClose);
      }
    } catch (error) {
      lease?.release();
      throw error;
    }
    return stream;
  }

  Object.setPrototypeOf(ReadStream, host.stream.Readable);
  ReadStream.prototype = Object.create(host.stream.Readable.prototype, {
    constructor: { value: ReadStream, writable: true, configurable: true }
  });

  ReadStream.prototype.close = function (callback) {
    if (callback !== undefined && typeof callback !== 'function') {
      fail('ERR_INVALID_ARG_TYPE', 'callback must be a function');
    }
    if (callback) {
      if (this.closed) {
        queueMicrotask(callback);
      } else {
        this.once('close', callback);
      }
    }
    this.destroy();
  };

  return { ReadStream, createReadStream: (filename, options) => new ReadStream(filename, options) };
}
