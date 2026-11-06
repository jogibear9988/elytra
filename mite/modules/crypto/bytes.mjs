/** Shared argument errors and zero-copy native-byte wrapping for crypto facades. */
export function fail(name, Type, code) {
  const error = new Type(`Invalid ${name} argument`);
  error.code = code;
  throw error;
}

export function wrapBytes(Buffer, bytes) {
  return Buffer.from(bytes.buffer, bytes.byteOffset, bytes.byteLength);
}
