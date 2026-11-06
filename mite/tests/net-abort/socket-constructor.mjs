/** Associate cancellation at construction, then connect using ordinary Socket.connect. */
import { Socket, createServer } from 'node:net';
const controller = new AbortController();
const server = createServer((peer) => peer.resume());
let correct = false;

server.listen({ port: 0, host: '127.0.0.1' }, () => {
  const socket = new Socket({ signal: controller.signal });

  socket.on(
    'error',
    (error) => (correct = error.code === 'ABORT_ERR' && error.cause === 'constructor')
  );

  socket.once('connect', () => controller.abort('constructor'));

  socket.once('close', () =>
    server.close(() => console.log('constructor', correct, socket.destroyed))
  );

  socket.connect(server.address().port, '127.0.0.1');
});
