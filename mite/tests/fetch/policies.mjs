/** Unsupported APIs reject explicitly; none should initiate a native request. */
const base = 'http://127.0.0.1:1';
let count = 0;

async function unsupported(action) {
  try {
    await action();
  } catch (error) {
    if (error.code !== 'ERR_MITE_UNSUPPORTED_API') {
      throw error;
    }
    count++;
    return;
  }
  throw new Error('unsupported API succeeded');
}

for (const init of [
  { mode: 'no-cors' },
  { cache: 'force-cache' },
  { integrity: 'sha256-missing' },
  { referrer: 'http://example.com' },
  { referrerPolicy: 'origin' }
]) {
  await unsupported(() => new Request(base, init));
}
await unsupported(() => fetch(base, { dispatcher: {} }));
await unsupported(() => fetch(base, { agent: {} }));
if (count !== 7) {
  throw new Error('incomplete policy coverage');
}
for (const input of ['ftp://example.com', 'http://user:password@example.com', base]) {
  let failed = false;
  try {
    await fetch(input);
  } catch (error) {
    failed = error instanceof TypeError;
  }
  if (!failed) {
    throw new Error('invalid URL policy succeeded');
  }
}
console.log('PASS Fetch explicit policies 7 and URL guards 3');
