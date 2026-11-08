/** Public close() completes pending writes and closes explicitly borrowed descriptors. */
import fs from 'node:fs';
import { check, observe, event } from './support.mjs';

const stream = fs.createWriteStream('close.bin', { autoClose: false });
const events = observe(stream);
const closed = event(stream, 'close');

stream.write('tail', () => events.push('write-callback'));

let called = 0;

stream.close((error) => {
  check(!error, 'close callback has no error');
  ++called;
});

await closed;
await new Promise((resolve) => setTimeout(resolve, 0));
check(called === 1 && stream.fd === null, 'close callback and fd retirement');
console.log(
  'close',
  JSON.stringify(events),
  called,
  stream.bytesWritten,
  fs.readFileSync('close.bin', 'utf8')
);
