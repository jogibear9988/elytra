/** Cancellation must close a real connected transport and preserve the original reason. */
import { createServer, createConnection } from 'node:net';
const controller = new AbortController();
const reason = { operation: 'cancel socket' };
const server = createServer((peer) => peer.pipe(peer));
let errors = 0;
let matches = false;

server.listen({ port: 0, host: '127.0.0.1' }, () => {
  const socket = createConnection({
    port: server.address().port,
    host: '127.0.0.1',
    signal: controller.signal
  });

  socket.on('error', (error) => {
    errors++;
    matches = error.name === 'AbortError' && error.code === 'ABORT_ERR' && error.cause === reason;
  });

  socket.once('connect', () => socket.write('echo before abort'));

  socket.once('data', (data) => {
    if (data.toString() !== 'echo before abort') {
      throw Error('Actual peer data was lost');
    }
    controller.abort(reason);
  });

  socket.once('close', (hadError) => {
    server.close(() => console.log('connected', socket.destroyed, hadError, errors, matches));
  });
});
