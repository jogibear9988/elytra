/** A pre-aborted factory request must never deliver connect or lose its abort error. */
import { createConnection, createServer } from 'node:net';
const controller = new AbortController();
controller.abort('before connect');
// A pre-aborted reference Socket can still reach accept on Linux. This fixture
// asserts connect/error delivery, not peer EOF: release accepted test peers so
// server.close cannot wait indefinitely on Node's startup race. Established
// cancellation and independent-peer suites separately verify native closure.
const server = createServer((peer) => peer.destroy());
let connects = 0;
let errors = 0;
let correct = false;

server.listen({ port: 0, host: '127.0.0.1' }, () => {
  const socket = createConnection({
    port: server.address().port,
    host: '127.0.0.1',
    signal: controller.signal
  });

  socket.on('connect', () => connects++);

  socket.on('error', (error) => {
    errors++;
    correct =
      error.name === 'AbortError' && error.code === 'ABORT_ERR' && error.cause === 'before connect';
  });

  socket.once('close', () => {
    setImmediate(() => server.close(() => console.log('pre connect', connects, errors, correct)));
  });
});
