/** Timeout signals are unreferenced but deliver their reason when other work stays live. */
const signal = AbortSignal.timeout(2);
await new Promise((resolve) => {
  const keepAlive = setTimeout(() => {
    throw Error('Abort timeout did not fire');
  }, 1000);

  signal.addEventListener(
    'abort',
    () => {
      clearTimeout(keepAlive);
      console.log(
        signal.aborted,
        signal.reason.name,
        signal.reason.code,
        signal.reason instanceof DOMException
      );
      resolve();
    },
    { once: true }
  );
});
