/** Genuine pipe cancellation, distinct from TCP's shared AbortSignal policy. */
import net from 'node:net';
import fs from 'node:fs';
import { once } from 'node:events';
const base = process.env.MITE_TEST_PIPE;
if (!base) {
  throw Error('Require isolated abort endpoints');
}

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const pause = () => new Promise((resolve) => setTimeout(resolve, 5));

for (const pre of [true, false]) {
  const path = base + (pre ? '-pre-listen' : '-pending-listen');
  const controller = new AbortController();
  if (pre) {
    controller.abort({ phase: 'before-listen' });
  }
  const server = net.createServer();
  let listening = 0;
  let errors = 0;

  server.on('listening', () => {
    listening++;
  });

  server.on('error', () => {
    errors++;
  });

  const closed = new Promise((resolve) => server.once('close', resolve));
  server.listen({ path, signal: controller.signal });
  if (!pre) {
    controller.abort({ phase: 'pending-listen' });
  }
  await closed;
  check(
    listening === 0 && errors === 0 && !server.listening,
    'Canceled pending pipe listener cannot report listening'
  );
  // Node may bind transiently before processing its aborted signal. At close,
  // both runtimes must leave no owned Unix endpoint behind.
  if (process.platform !== 'win32') {
    check(!fs.existsSync(path), 'Aborted pipe listener leaked its filesystem endpoint');
  }
}
console.log('pre and pending pipe listen abort close without listening or remaining endpoint');

const peers = new Set();
const server = net.createServer((peer) => {
  peers.add(peer);

  peer.once('close', () => peers.delete(peer));

  peer.on('error', (failure) => {
    if (!['ECONNRESET', 'EPIPE', 'ENOTCONN'].includes(failure.code)) {
      throw failure;
    }
  });

  peer.on('data', (chunk) => peer.write(chunk));
});
server.listen(base);
await once(server, 'listening');
for (const pre of [true, false]) {
  const controller = new AbortController();
  const reason = { phase: pre ? 'before-connect' : 'pending-connect' };
  if (pre) {
    controller.abort(reason);
  }
  const socket = net.createConnection({ path: base, signal: controller.signal });
  let connects = 0;
  const errors = [];

  socket.on('connect', () => {
    connects++;
  });

  socket.on('error', (failure) => errors.push(failure));

  const closed = new Promise((resolve) => socket.once('close', resolve));
  if (!pre) {
    controller.abort(reason);
  }
  await closed;
  check(connects === 0, 'Pre/pending aborted pipe factory cannot connect');
  check(
    errors.length === 1 &&
      errors[0].name === 'AbortError' &&
      errors[0].code === 'ABORT_ERR' &&
      errors[0].cause === reason,
    'Pre/pending pipe abort preserves original cause'
  );
}
console.log('pre and immediate pipe factory abort preserve ABORT_ERR cause without connect');
// Node may still accept a pre-aborted factory's transient connection. Clean only
// our owned peers before the independent established-connection case.
await pause();
for (const peer of peers) {
  peer.destroy();
}
await pause();

const controller = new AbortController();
const reason = { phase: 'established-pipe' };
const socket = net.createConnection({ path: base, signal: controller.signal });
const errors = [];

socket.on('error', (failure) => errors.push(failure));

const closed = new Promise((resolve) => socket.once('close', resolve));
await once(socket, 'connect');
const payload = Buffer.from([0, 255, 1, 2, 3, 0, 7]);
const chunks = [];
let total = 0;
const echoed = new Promise((resolve) =>
  socket.on('data', (chunk) => {
    chunks.push(chunk);
    total += chunk.length;
    if (total >= payload.length) {
      resolve(Buffer.concat(chunks));
    }
  })
);
socket.write(payload);
check(
  (await echoed).equals(payload),
  'Established actual pipe transports binary bytes before abort'
);
let serverClosed = false;
const listenerClosed = new Promise((resolve) =>
  server.close(() => {
    serverClosed = true;
    resolve();
  })
);
await pause();
check(!serverClosed, 'Listener close waits for established pipe before cancellation');
controller.abort(reason);
await closed;
await listenerClosed;
check(
  socket.destroyed &&
    errors.length === 1 &&
    errors[0].code === 'ABORT_ERR' &&
    errors[0].cause === reason,
  'Established pipe abort closes with original cause'
);
check(serverClosed, 'Abort releases accepted pipe so listener close can finish');
console.log('established binary pipe abort releases client accepted peer and waiting listener');
