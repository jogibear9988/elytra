/** A public propagation stop cannot suppress native transport cancellation. */
import { createServer, createConnection } from 'node:net';
const controller = new AbortController();

controller.signal.addEventListener('abort', (event) => event.stopImmediatePropagation());

const server = createServer((peer) => peer.resume());
let aborted = false;

server.listen({ port: 0, host: '127.0.0.1' }, () => {
  const socket = createConnection({
    port: server.address().port,
    host: '127.0.0.1',
    signal: controller.signal
  });

  socket.on('error', (error) => (aborted = error.code === 'ABORT_ERR'));

  socket.once('connect', () => controller.abort());

  socket.once('close', () =>
    server.close(() => console.log('propagation', aborted, socket.destroyed))
  );
});
