/** Web exception names/codes and receiver/argument errors are compared without stack text. */
for (const name of [
  'Error',
  'AbortError',
  'TimeoutError',
  'DataCloneError',
  'Unknown',
  'DOMStringSizeError',
  'NoDataAllowedError',
  'ValidationError'
]) {
  const error = new DOMException('message', name);
  console.log(
    error instanceof Error,
    error.name,
    error.message,
    error.code,
    Object.prototype.toString.call(error)
  );
}
console.log(DOMException.ABORT_ERR, DOMException.prototype.TIMEOUT_ERR);
for (const operation of [
  () => new AbortSignal(),
  () => AbortController.prototype.abort.call({ signal: new AbortController().signal }),
  () => AbortSignal.prototype.throwIfAborted.call({}),
  () => AbortSignal.any([AbortSignal.abort(), {}]),
  () => AbortSignal.any(null),
  () => AbortSignal.timeout('1'),
  () => AbortSignal.timeout(-1),
  () => AbortSignal.timeout(1.5),
  () => AbortSignal.timeout(NaN),
  () => AbortSignal.timeout(2 ** 32)
]) {
  try {
    operation();
  } catch (error) {
    console.log(error.name, error.code);
  }
}
