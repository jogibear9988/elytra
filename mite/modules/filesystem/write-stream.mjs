/** Native file writes use bounded requests; Writable owns queuing and backpressure. */
export default function createWriteStreams(fs, host, validate, acquireHandle) {
  const fail = (code, message) => {
    throw Object.assign(new TypeError(message), { code });
  };

  class WriteStream extends host.stream.Writable {
    constructor(filename, options = {}) {
      if (typeof options === 'string') {
        options = { encoding: options };
      }
      if (!options || typeof options !== 'object') {
        fail('ERR_INVALID_ARG_TYPE', 'options must be an object or encoding');
      }
      for (const key of Object.keys(options)) {
        if (
          ![
            'encoding',
            'flags',
            'mode',
            'fd',
            'start',
            'highWaterMark',
            'autoClose',
            'emitClose',
            'signal'
          ].includes(key)
        ) {
          fail('ERR_MITE_UNSUPPORTED_API', `WriteStream option ${key} is not implemented`);
        }
      }
      for (const key of ['autoClose', 'emitClose']) {
        if (options[key] !== undefined && typeof options[key] !== 'boolean') {
          fail('ERR_INVALID_ARG_TYPE', `${key} must be a boolean`);
        }
      }
      if (
        options.encoding !== undefined &&
        (typeof options.encoding !== 'string' || !host.Buffer.isEncoding(options.encoding))
      ) {
        fail('ERR_INVALID_ARG_VALUE', 'Unknown file stream encoding');
      }
      let position = options.start;
      if (position !== undefined && (!Number.isSafeInteger(position) || position < 0)) {
        fail('ERR_OUT_OF_RANGE', 'start must be a nonnegative safe integer');
      }
      const supplied = options.fd !== undefined && options.fd !== null;
      const pathname = supplied ? undefined : validate.path(filename);
      const flags = validate.flags(options.flags, 'w');
      const mode = validate.mode(options.mode, 0o666);
      const handle = supplied && typeof options.fd === 'object' ? options.fd : undefined;
      if (supplied && !handle && (!Number.isInteger(options.fd) || options.fd < 0)) {
        fail('ERR_INVALID_ARG_TYPE', 'fd must be a nonnegative descriptor or FileHandle');
      }
      // A handle lease spans queued writes, not merely one native request. Public
      // FileHandle.close ends this stream before the descriptor can be retired.
      const lease = handle ? acquireHandle(handle) : undefined;
      let fd = supplied ? (handle ? handle.fd : options.fd) : null;
      let bytes = 0;
      let writing;
      let stream;

      const onHandleClose = () => stream?.close();

      const preAborted = options.signal?.aborted === true;
      try {
        super({
          highWaterMark: options.highWaterMark ?? 16384,
          defaultEncoding: options.encoding ?? 'utf8',
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
              this.emit('open', fd);
              this.emit('ready');
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
                  count = handle
                    ? (await handle.write(chunk, offset, length, position ?? null)).bytesWritten
                    : await new Promise((resolve, reject) => {
                        fs.write(fd, chunk, offset, length, position ?? null, (error, written) =>
                          error ? reject(error) : resolve(written)
                        );
                      });
                } catch (error) {
                  if (error.code !== 'EAGAIN') {
                    throw error;
                  }
                  count = 0;
                }
                if (count === 0) {
                  if (++retries > 5) {
                    throw Object.assign(new Error('File write made no progress'), {
                      code: 'ERR_SYSTEM_ERROR'
                    });
                  }
                  await new Promise((resolve) => setTimeout(resolve, 0));
                  continue;
                }
                retries = 0;
                bytes += count;
                offset += count;
                if (position !== undefined) {
                  position += count;
                }
              }
            })();

            writing.then(() => callback(), callback);
          },

          destroy(error, callback) {
            handle?.removeListener('close', onHandleClose);
            // Release the lease only after the entire partial-write loop retires.
            // Release before awaiting close, which itself waits for that lease.
            Promise.resolve(writing)
              .catch(() => {})
              .then(async () => {
                lease?.release();
                if (fd !== null) {
                  if (lease) {
                    await lease.close();
                  } else {
                    await new Promise((resolve, reject) => {
                      fs.close(fd, (failure) => (failure ? reject(failure) : resolve()));
                    });
                  }
                }
              })
              .then(
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
        handle.on('close', onHandleClose);
      }
    }

    close(callback) {
      if (callback) {
        if (this.closed) {
          queueMicrotask(callback);
        } else {
          this.once('close', callback);
        }
      }
      if (this.writableFinished) {
        this.destroy();
      } else {
        this.once('finish', () => this.destroy());

        this.end();
      }
    }
  }

  return {
    WriteStream,

    createWriteStream: (filename, options) => new WriteStream(filename, options)
  };
}
