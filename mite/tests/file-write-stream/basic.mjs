/** UTF-8 backpressure and native write completion expose public lifecycle and fd retirement. */
import fs, { WriteStream, createWriteStream } from 'node:fs';
import { Writable } from 'node:stream';
import { check, observe, event } from './support.mjs';

const stream = createWriteStream('basic.bin', { highWaterMark: 2, mode: 0o600 });
const events = observe(stream);
check(stream instanceof WriteStream && stream instanceof Writable, 'Public stream class identity');
console.log('initial', stream.path, stream.fd, stream.pending, stream.bytesWritten);
let synchronous = true;
const closed = event(stream, 'close');
const drained = event(stream, 'drain');
const accepted = stream.write('ä', () => {
  check(!synchronous, 'Native write callback must be asynchronous');
  events.push('write-callback');
});
synchronous = false;
check(accepted === false, 'UTF-8 byte length reaches high water mark');
await drained;
check(stream.bytesWritten === 2 && stream.pending === false, 'Drain after actual native bytes');

stream.end(Buffer.from([0, 128, 255]), () => events.push('end-callback'));

await closed;
check(
  stream.bytesWritten === 5 && stream.fd === null && stream.pending === true,
  'Final native byte count and descriptor retirement'
);
console.log('events', JSON.stringify(events));
console.log(
  'final',
  stream.bytesWritten,
  stream.fd,
  stream.pending,
  stream.destroyed,
  stream.closed
);
console.log('bytes', fs.readFileSync('basic.bin').toString('hex'));
if (process.platform !== 'win32') {
  console.log('mode', (fs.statSync('basic.bin').mode & 0o777).toString(8));
}

const batched = new WriteStream('batched.bin', { highWaterMark: 1024 });
const batchClosed = event(batched, 'close');
const callbacks = [];
batched.cork();
for (let index = 0; index < 16; ++index) {
  const bytes = Buffer.alloc(8192, index);

  batched.write(bytes, () => callbacks.push(index));
}
batched.uncork();
batched.end();
await batchClosed;
check(batched.bytesWritten === 16 * 8192, 'Queued binary writes reach the real file');
check(
  callbacks.every((value, index) => value === index) && callbacks.length === 16,
  'Queued write callbacks are FIFO'
);
const bytes = fs.readFileSync('batched.bin');
check(
  bytes.length === 16 * 8192 && bytes[8192] === 1 && bytes[bytes.length - 1] === 15,
  'Actual queued file contents'
);
console.log('batch', bytes.length, callbacks.length, batched.bytesWritten);
