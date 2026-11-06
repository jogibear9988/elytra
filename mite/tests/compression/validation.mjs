/** Require explicit BufferSource and public-format failure in Mite's standard interface. */
for (const input of ['not bytes', {}, null]) {
  const decoder = new DecompressionStream('gzip');
  const reading = decoder.readable
    .getReader()
    .read()
    .catch((error) => error);
  const writer = decoder.writable.getWriter();
  const closed = writer.closed.catch((error) => error);
  let failure;
  try {
    await writer.write(input);
  } catch (error) {
    failure = error;
  }
  if (
    !(failure instanceof TypeError) ||
    (await reading) !== failure ||
    (await closed) !== failure
  ) {
    throw new Error('Invalid chunk did not fail both endpoints');
  }
}
for (const format of ['brotli', 'gzip-http']) {
  let failed = false;
  try {
    new DecompressionStream(format);
  } catch (error) {
    failed = error instanceof TypeError;
  }
  if (!failed) {
    throw new Error('Unavailable/private format escaped validation');
  }
}
console.log('PASS decoder BufferSource and private-format guards 5');
