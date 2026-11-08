/** Explicit ownership differences, consumer exceptions and queued iterator retirement. */
import { createInterface } from 'node:readline';
import { PassThrough, Readable } from 'node:stream';
import { setTimeout as sleep } from 'node:timers/promises';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const input = new PassThrough();
const rl = createInterface({ input });
const failure = new Error('consumer');
let forwarded = false;

rl.on('error', () => {
  forwarded = true;
});

rl.on('line', () => {
  throw failure;
});

let caught;
try {
  input.emit('data', Buffer.from('line\n'));
} catch (error) {
  caught = error;
}
check(caught === failure && !forwarded, 'consumer exception propagation');
rl.close();
const controller = new AbortController();
controller.abort();
const aborted = createInterface({ input: new PassThrough(), signal: controller.signal });
await new Promise((resolve) => aborted.once('close', resolve));
check(aborted.closed, 'pre-aborted cleanup');
const slow = new PassThrough();
const lines = createInterface({ input: slow });
const iterator = lines[Symbol.asyncIterator]();
const pending = iterator.next();
lines.close();
check((await pending).done, 'pending iterator close');
// Resume may synchronously call user handlers; a new pause must not be overwritten.
const reentrant = new PassThrough();
const flow = createInterface({ input: reentrant });
flow.pause();

flow.once('resume', () => flow.pause());

flow.resume();
check(reentrant.isPaused(), 'reentrant pause');
flow.close();
if (process.versions.mite) {
  const data = Readable.from([Buffer.from('short\n'.repeat(5000))]);
  const reader = createInterface({ input: data });
  const it = reader[Symbol.asyncIterator]();
  await it.next();
  await sleep(10);
  check(data.isPaused(), 'queue pressure reaches real input');
  await it.return();
  check(reader.closed, 'return retires input');
  const errored = new PassThrough();
  const errorLines = createInterface({ input: errored });
  const waiting = errorLines[Symbol.asyncIterator]().next();
  errored.emit('error', failure);
  let rejected;
  try {
    await waiting;
  } catch (error) {
    rejected = error;
  }
  check(rejected === failure && errorLines.closed, 'error retires interface and pending read');
}
console.log(
  'PASS readline delivery exceptions, abort/close, reentrant flow and owned iterator retirement'
);
