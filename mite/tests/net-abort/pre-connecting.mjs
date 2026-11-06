/** A pre-aborted factory request must never deliver connect or lose its abort error. */
import { createConnection, createServer } from 'node:net';
const controller = new AbortController();
controller.abort('before connect');
const server = createServer((peer) => peer.resume());
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
