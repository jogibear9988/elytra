/** Keep HTTP truncation strictness explicit instead of normalizing away Node behavior. */
const base = `http://127.0.0.1:${process.argv[2]}`;
try {
  const bytes = await (await fetch(base + '/short')).bytes();
  console.log('truncated gzip decoded', bytes.length);
} catch (error) {
  if (error.cause?.code !== 'Z_BUF_ERROR') {
    throw error;
  }
  console.log('truncated gzip rejected', error.cause.code);
}
console.log('DONE compressed policy');
