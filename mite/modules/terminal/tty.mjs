/** Read-only native terminal detection. Stream/mode ownership is deliberately unsupported. */
export default function createTTY(host) {
  const probe = host.native.isatty;

  function isatty(fd) {
    return Number.isInteger(fd) && fd >= 0 && fd <= 2147483647 && probe(fd);
  }

  function unsupported(name) {
    throw Object.assign(new Error(`${name} is not implemented`), {
      code: 'ERR_MITE_UNSUPPORTED_API'
    });
  }

  function ReadStream() {
    unsupported('tty.ReadStream');
  }

  function WriteStream() {
    unsupported('tty.WriteStream');
  }

  return { isatty, ReadStream, WriteStream };
}
