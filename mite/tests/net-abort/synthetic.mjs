/** Public synthetic abort events must not substitute for private native cancellation. */
import { createServer, createConnection } from 'node:net';
const controller = new AbortController();
const server = createServer((peer) => peer.resume());
let syntheticDestroyed = false;
let correct = false;

server.listen({ port: 0, host: '127.0.0.1' }, () => {
  const socket = createConnection({
    port: server.address().port,
    host: '127.0.0.1',
    signal: controller.signal
  });

  socket.on('error', (error) => (correct = error.code === 'ABORT_ERR'));

  socket.once('connect', () => {
    controller.signal.dispatchEvent(new Event('abort'));
    syntheticDestroyed = socket.destroyed;
    controller.abort('real abort');
  });

  socket.once('close', () =>
    server.close(() => console.log('synthetic socket', syntheticDestroyed, correct))
  );
});
