/** Capture a caller-owned send payload before any asynchronous transport or Blob read. */
export default function createPayloadCapture(host) {
  const { Buffer, types, snapshotBytes, Blob } = host;
  const size = Object.getOwnPropertyDescriptor(Blob.prototype, 'size').get;
  const readBlob = Blob.prototype.arrayBuffer;
  const apply = Reflect.apply;
  const length = Object.getOwnPropertyDescriptor(
    Object.getPrototypeOf(Uint8Array.prototype),
    'byteLength'
  ).get;
  const maximum = 1024 * 1024;

  function check(bytes) {
    if (bytes > maximum) {
      const error = new RangeError('WebSocket message exceeds the 1 MiB limit');
      error.code = 'ERR_MITE_WEBSOCKET_LIMIT';
      throw error;
    }
    return bytes;
  }

  function string(value) {
    if (typeof value === 'symbol') {
      throw new TypeError('Cannot convert a Symbol value to a string');
    }
    return String(value);
  }

  return function capture(value) {
    if (types.isArrayBuffer(value) || types.isTypedArray(value) || types.isDataView(value)) {
      const view = snapshotBytes(value);
      const bytes = check(apply(length, view, []));
      return { bytes, binary: true, value: Buffer.from(view) };
    }
    let blobSize;
    try {
      blobSize = apply(size, value, []);
    } catch {}
    if (blobSize !== undefined) {
      return {
        bytes: check(blobSize),
        binary: true,

        read: () => apply(readBlob, value, []).then((buffer) => Buffer.from(snapshotBytes(buffer)))
      };
    }
    const text = string(value);
    const bytes = check(Buffer.byteLength(text, 'utf8'));
    return { bytes, binary: false, value: Buffer.from(text, 'utf8') };
  };
}
