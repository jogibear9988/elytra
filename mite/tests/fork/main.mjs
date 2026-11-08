/** Differential real self-fork, JSON/base64 FIFO snapshots and backpressure lifecycle. */
import { fork, ChildProcess } from 'node:child_process';
import { createHash } from 'node:crypto';
import { once } from 'node:events';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

const childURL = new URL('./child.mjs', import.meta.url);
const child = fork(childURL, ['echo'], { silent: true, execArgv: [] });
const events = [];
const received = [];
const callbacks = [];
const out = [];
const err = [];

child.on('spawn', () => events.push('spawn'));

child.on('disconnect', () => events.push('disconnect'));

child.on('close', () => events.push('close'));

child.on('exit', (code, signal) => {
  check(
    code === 0 && signal === null,
    `Natural child exit: code=${code} signal=${signal} received=${received.length} ` +
      `callbacks=${callbacks.length} events=${events.join(',')} ` +
      `childError=${Buffer.concat(err).toString().slice(-4096)}`
  );
  events.push('exit');
});

child.stdout.on('data', (chunk) => out.push(chunk));

child.stderr.on('data', (chunk) => err.push(chunk));

child.on('error', (error) => {
  throw error;
});

check(child instanceof ChildProcess && child.connected, 'Actual connected ChildProcess');
const closing = once(child, 'close');
const payload = {
  text: 'Grüße 日本語 🪲',
  nested: { count: 7 },
  omitted: undefined,
  list: [undefined, NaN, Infinity],
  buffer: Buffer.from([0, 255, 128])
};
const base64 = Buffer.from(Array.from({ length: 1027 }, (_, index) => index % 251)).toString(
  'base64'
);
const values = [
  payload,
  null,
  true,
  false,
  42,
  'text',
  [1, 'a', null],
  base64,
  'x'.repeat(768 * 1024)
];
const expected = values.map((value) => JSON.parse(JSON.stringify(value)));
let synchronous = true;
let pressured = false;
for (let index = 0; index < values.length; index++) {
  const result = child.send(values[index], (error) => {
    check(!synchronous && error == null, 'Accepted sends complete asynchronously once');
    callbacks.push(index);
  });
  pressured ||= result === false;
}
payload.nested.count = 999;
payload.buffer.fill(99);
synchronous = false;
let finished;
const completion = new Promise((resolve) => (finished = resolve));

child.on('message', (message) => {
  if (message.ready) {
    return;
  }
  if (message.done) {
    check(message.count === values.length, 'Child received every accepted frame');
    finished();
    return;
  }
  check(message.seq === received.length, 'Bidirectional JSON FIFO');
  received.push(message.value);
  if (received.length === expected.length) {
    child.send({ command: 'finish' });
  }
});

await completion;
const [code, signal] = await closing;
check(code === 0 && signal === null, 'Close reports actual child status');
check(JSON.stringify(received) === JSON.stringify(expected), 'JSON copied before caller mutation');
check(
  callbacks.length === values.length && new Set(callbacks).size === values.length,
  'Every accepted send callback fires once'
);
check(pressured, 'Large accepted message reports native backlog pressure');
check(
  events[0] === 'spawn' &&
    events.at(-1) === 'close' &&
    events.filter((name) => name === 'spawn').length === 1,
  'Spawn/exit/disconnect precede final close'
);
check(
  events.includes('disconnect') &&
    events.includes('exit') &&
    !child.connected &&
    child.channel == null,
  'EOF retires channel'
);
check(
  Buffer.concat(out).length === 0 && Buffer.concat(err).length === 0,
  'No inherited IPC bytes in stdio'
);

// Completed native writes remain readable through EOF; sink hashes every received frame.
const sink = fork(childURL, ['sink'], { silent: true, execArgv: [] });
const sinkOut = [];
let sinkError = '';

sink.stdout.on('data', (value) => sinkOut.push(value));

sink.stderr.on('data', (value) => (sinkError += value));

const sinkExit = once(sink, 'exit');
const sinkEOF = once(sink.stdout, 'end');
const hash = createHash('sha256');
let written = 0;
let allWritten;
const writesDone = new Promise((resolve) => (allWritten = resolve));
for (const value of ['a', { nested: [1, 2, 3] }, 'z'.repeat(768 * 1024)]) {
  hash.update(JSON.stringify(value));

  sink.send(value, (error) => {
    check(error == null, 'Pending disconnect write completed');
    written++;
    if (written === 3) {
      allWritten();
    }
  });
}
await writesDone;
sink.disconnect();
check(!sink.connected, 'Disconnect marks parent disconnected immediately');
await Promise.all([sinkExit, sinkEOF]);
const summary = JSON.parse(Buffer.concat(sinkOut).toString());
check(
  summary.count === 3 && summary.hash === hash.digest('hex') && written === 3 && !sinkError,
  'Pending writes drain before child EOF ' + JSON.stringify({ summary, written, sinkError })
);
let afterSend;
let inline = true;
check(
  sink.send('late', (error) => {
    check(!inline, 'Closed send callback deferred');
    afterSend = error.code;
  }) === false,
  'Closed send returns false'
);
inline = false;
await new Promise((resolve) => setTimeout(resolve, 1));
check(afterSend === 'ERR_IPC_CHANNEL_CLOSED', 'Closed channel send callback error');
const disconnectedError = once(sink, 'error');
sink.disconnect();
check(
  (await disconnectedError)[0].code === 'ERR_IPC_DISCONNECTED',
  'Repeated disconnect reports error asynchronously'
);
console.log(
  JSON.stringify({
    fifo: received.length,
    snapshot: true,
    base64: true,
    pressure: pressured,
    callbacks: callbacks.length,
    disconnected: !child.connected,
    flushed: summary.count,
    closedSend: afterSend
  })
);
