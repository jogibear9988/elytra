/** Real borrowed standard-output descriptors; stream retirement never closes OS fd 1/2. */
export default function installProcessStdio(host, process) {
  const { Writable, fs } = host;

  function output(fd) {
    const stream = new Writable({
      autoDestroy: false,

      write(chunk, encoding, callback) {
        let offset = 0;

        function advance() {
          if (offset === chunk.byteLength) {
            callback();
            return;
          }
          // null position writes at the descriptor's current offset (also works for pipes).
          fs.write(fd, chunk, offset, chunk.byteLength - offset, null, (error, count) => {
            if (error) {
              callback(error);
              return;
            }
            if (!Number.isInteger(count) || count <= 0 || count > chunk.byteLength - offset) {
              callback(
                Object.assign(new Error('Standard output write made no valid progress'), {
                  code: 'EIO',
                  syscall: 'write'
                })
              );
              return;
            }
            offset += count;
            advance();
          });
        }

        advance();
      },

      destroy(error, callback) {
        // These descriptors belong to the embedding process, not this Writable wrapper.
        callback(error);
      }
    });
    Object.defineProperty(stream, 'fd', { value: fd, enumerable: true });
    return stream;
  }

  Object.defineProperties(process, {
    stdout: { value: output(1), enumerable: true, configurable: true },
    stderr: { value: output(2), enumerable: true, configurable: true },
    stdin: {
      enumerable: true,
      configurable: true,

      get() {
        throw Object.assign(new Error('process.stdin is not implemented'), {
          code: 'ERR_MITE_UNSUPPORTED_API'
        });
      }
    }
  });
}
